# CLK Key I/O 重设计

## 1. 概述

将 `clk_key_io` 从一个 byte handler 升级为完整的 UTF-8 感知键盘输入引擎。

核心变化：

```
old: 单字节读 → 简单 ASCII 解析 → key_event (字节粒度)
             └→ text_poll 内部硬编码编辑逻辑

new: 单字节读 → raw_read_key 状态机 → key_event (128位掩码 + UTF-8文本)
             ├→ NORMAL 模式: 纯事件
             └→ INPUT 模式:  事件 + 文本, 调用方控制编辑
```

## 2. 事件模型

### 2.1 键位掩码

`__uint128_t key_mask`，每键占一个二进制位。修饰键（Shift/Ctrl/Alt/Meta）同格——也是掩码中的位，与主键位平等。

switch case 使用编译期常量组合匹配：

```c
case MOD_CTRL | KEY_S:
case MOD_CTRL | MOD_SHIFT | KEY_J:
case KEY_ESC:
```

#### 键位分配表

```
位 0-25:   大写 A-Z
位 26-51:  小写 a-z
位 52-61:  数字 0-9
位 62-72:  符号(无修饰) ` - = [ ] \ ; ' , . /
位 73-83:  符号(Shift版) ~ _ + { } | : " < > ?
位 84-93:  标点(Shift+数字) ! @ # $ % ^ & * ( )
位 94:     空格
位 95-98:  方向键 ↑ ↓ ← →
位 99-102: 导航 Home End PageUp PageDown
位 103-104: 删除 Delete Insert
位 105-108: 编辑 Enter Tab Backspace Escape
位 109-116: 功能 F1-F8
位 117-120: 修饰 Shift Ctrl Alt Meta
位 121-127: 预留
```

总计 127 位，7 位预留。修饰键用 `MOD_*` 宏，其余全部 `KEY_*` 宏。

### 2.2 事件结构体

```c
typedef struct {
    __uint128_t key_mask;
    char  text[8];
    uint8_t text_len;
    bool  has_text;
} clk_key_event;
```

- 一次 yield = 一个完整解析单元（单字节 ASCII / 多字节 UTF-8 / CSI 序列）
- 中文字符无事件位，纯走 `text` 流
- 未消费文本 frame-accurate 丢弃：被下次 `get_key_event` 覆写。调用方需保留则自行拷贝

### 2.3 SHIFT + 字母

终端发 `0x41`（'A'）时，状态机直接映射到 `KEY_A_UPPER`，**不打 `MOD_SHIFT` 位**。因为终端层原始 raw 模式下不传输"Shift 是否按下"，只传输结果字符。

## 3. 状态机（raw_read_key 重写）

单状态机，消费 `read(stdin, &ch, 1)` 字节流，产出 `{key_mask, text}`。

### 3.1 处理路径

| 输入                       | 处理                                                                                                                                |
| -------------------------- | ----------------------------------------------------------------------------------------------------------------------------------- |
| `ch < 0x20`                | ASCII 控制字符 → 查表映射 `MOD_CTRL \| KEY_A..Z`，无文本                                                                            |
| `ch == 0x7F`               | Backspace → `KEY_BS`，无文本                                                                                                        |
| `ch >= 0x20 && ch <= 0x7E` | ASCII 可打印 → 对应键位 + `text = char`                                                                                             |
| `ch >= 0xC0`               | UTF-8 lead byte → 根据首字节计算预期字节数，`read()` 补齐 → 解码出 text，**无事件位**                                               |
| `ch == 0x1B`               | ESC → 1ms `select()` 超时判断：<br> &bull; 无后续 → `KEY_ESC`<br> &bull; `[` → 进入 CSI 序列解析<br> &bull; `O` → 进入 SS3 序列解析 |

### 3.2 CSI 序列解析

`ESC [` 后接参数（可含 `;` 分隔的修饰键编码）到终结符，产出对应的方向/导航/F-key 位 + 修饰位组合。

xterm 修饰键参数编码：

```
0 或空 → 无修饰
2     → Shift
3     → Alt
5     → Ctrl
6     → Ctrl+Shift
7     → Ctrl+Alt
8     → Ctrl+Shift+Alt
```

### 3.3 modifyOtherKeys 协议

启用 Level 2 协议（`\033[>4;2m`），在 `clk_key_io_init()` 中向终端发送。

开启后，所有带修饰键的普通字符通过 CSI `u` 序列传输：

```
Ctrl+/  → ESC [ 47 ; 5 u    (keycode 47 = '/', mod 5 = Ctrl)
Ctrl+A  → ESC [ 97 ; 5 u    (keycode 97 = 'a', mod 5 = Ctrl)
```

符号 Ctrl 组合不再依赖 ASCII 控制字符映射，彻底消除冲突（Ctrl+/ 和 Ctrl+- 不再同时映射到 `0x1F`）。

Level 2 vs Level 3：

|              | Level 2              | Level 3             |
| ------------ | -------------------- | ------------------- |
| 无修饰普通键 | 直接按 raw byte 处理 | 全部走 CSI `u` 解析 |
| 带修饰普通键 | 走 CSI `u`           | 走 CSI `u`          |
| 兼容性       | 主流全支持           | 仅新版支持          |
| 向后兼容     | 不破坏无修饰键逻辑   | 破坏                |

选择 Level 2：解决 Ctrl+符号冲突，保留无修饰键路径。

## 4. 双模式 API

### 4.1 模式控制

```c
void clk_key_io_set_normal(void);
void clk_key_io_set_input(char* buf, size_t max_len, size_t* len, size_t* pos);
```

`set_input` 绑定用户缓冲区并切换到 INPUT 态。`set_normal` 切回 NORMAL 态，无参数。

默认启动为 NORMAL 态。

### 4.2 事件获取

两者仅在对应模式返回有效事件，否则返回 `key_mask = 0` 的空事件。

```c
clk_key_event clk_normal_get_key_event(void);
clk_key_event clk_input_get_key_event(void);
```

- **NORMAL**：返回事件，文本在状态机输出阶段丢弃
- **INPUT**：返回带 `text/text_len/has_text` 的事件，**不自动写入 buf**

### 4.3 调用方典型代码

```c
clk_key_event ev = clk_input_get_key_event();

switch (ev.key_mask) {
    case MOD_CTRL | KEY_S:       save();    break;
    case KEY_ESC:                cancel();  break;
    case KEY_ENTER:              confirm(); break;
    case KEY_LEFT:               clk_input_move_cursor(-1); break;
    case KEY_RIGHT:              clk_input_move_cursor(1);  break;
    case KEY_DEL:                clk_input_delete_after();  break;
    case KEY_BS:                 clk_input_delete_before(); break;
    default:
        if (ev.has_text)
            clk_input_write(CLK_WRITE_INSERT, ev.text, ev.text_len);
        break;
}
```

快捷键优先拦截 → 特殊键 → 最后 default 走文本编辑。不存在文本泄露到 buf 的时序问题。

## 5. 文本编辑原语

仅 INPUT 模式生效，操作绑定的 `buf/len/pos`。全部**字符粒度**，内部处理 UTF-8 字节边界。

| 函数                                    | 语义                                                   |
| --------------------------------------- | ------------------------------------------------------ |
| `clk_input_write(mode, text, byte_len)` | 按 mode(INSERT/OVERWRITE) 在光标位置写入文本，pos 后移 |
| `clk_input_move_cursor(offset)`         | 光标移 offset 个字符，负左正右                         |
| `clk_input_delete_before()`             | 删除光标前一字符（Backspace），pos/len 同步更新        |
| `clk_input_delete_after()`              | 删除光标处字符（Delete），len 更新 pos 不动            |

内部使用 static 工具函数处理 UTF-8 边界扫描和字符宽度判断。

## 6. ring buffer

条目从旧 `clk_key_event` 扩展为含 `text/text_len/has_text` 的新结构体。容量 256 条，满时覆盖最老条目（整条覆写，不关心内容）。

## 7. 配套工具

`clk_term.h/c` 新增显示宽度函数，供调用方计算光标列位置：

```c
/** Display columns for the first byte_len bytes of a UTF-8 string.
 *  CJK/fullwidth characters count as 2; ASCII as 1. */
int clk_term_utf8_display_width(const char* str, size_t byte_len);
```

使用 POSIX `wcwidth()` 实现。

## 8. 实现顺序

| 阶段 | 内容                                   | 文件                   |
| ---- | -------------------------------------- | ---------------------- |
| 1    | 键位宏定义 + 新 `clk_key_event` 结构体 | `clk_key_io.h`         |
| 2    | 双 API + 编辑原语声明                  | `clk_key_io.h`         |
| 3    | raw_read_key 状态机重写                | `clk_key_io.c`         |
| 4    | 双 API + ring buffer 适配实现          | `clk_key_io.c`         |
| 5    | 5 个编辑原语实现                       | `clk_key_io.c`         |
| 6    | `clk_term_utf8_display_width`          | `clk_term.h/c`         |
| 7    | 测试用例补全                           | `test_key_io.c`        |
| 8    | `write_demo.c` 适配新 API              | `sandbox/write_demo.c` |

## 9. 向后兼容说明

### 不变

- `clk_key_io_init / close` 生命周期不变
- ring buffer 架构不变
- `clk_term_init` 不依赖新 API

### 需适配的调用方

| 文件                    | 影响                                                          |
| ----------------------- | ------------------------------------------------------------- |
| `sandbox/write_demo.c`  | 从 `text_start/stop` → `set_input/normal`，事件结构字段名变更 |
| `sandbox/key_io_demo.c` | 同 write_demo                                                 |
| `src/main.c`            | `clk_get_key_event` → `clk_normal_get_key_event`（命名变更）  |
| `test/test_key_io.c`    | 测试注入接口和断言需重写                                      |

### 废弃接口

| 旧 API                    | 替代                         |
| ------------------------- | ---------------------------- |
| `clk_get_key_event()`     | `clk_normal_get_key_event()` |
| `clk_key_io_text_start()` | `clk_key_io_set_input()`     |
| `clk_key_io_text_stop()`  | `clk_key_io_set_normal()`    |
| `clk_key_io_text_poll()`  | `clk_input_get_key_event()`  |
