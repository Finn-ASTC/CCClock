# AGENTS.md — CCClock 开发指南

## 项目概述

CCClock 是一个终端 ASCII 艺术时钟应用，使用 C17 编写，CMake + vcpkg 构建。
32ms 主循环帧率，支持闹钟、番茄钟、音频播放、JSON 配置文件热加载。

## 架构层级

项目采用分层架构，所有源文件位于 `src/`，公开头文件位于 `include/`。
修改代码时先确认目标模块的层级位置，然后按依赖关系向下追溯：

| 层级 | 模块 | 职责 |
|------|------|------|
| Layer 0 基础 | `clk_term`, `clk_key_io`, `clk_json`, `clk_time`, `clk_file_util`, `clk_fs_watch` | 终端渲染、键盘输入、JSON 解析、时间工具、文件 I/O、文件监控 |
| Layer 1 领域 | `clk_clock`, `clk_audio`, `clk_ascii_render`, `clk_menu` | 时钟逻辑、音频播放、ASCII 字体渲染、菜单数据模型 |
| Layer 2 UI/主题 | `clk_menu_theme`, `clk_menu_instance`, `clk_app_config` | 菜单主题引擎、菜单实例渲染、应用配置与热加载 |
| Layer 3 引导 | `clk_app_setup`, `main.c` | 应用启动装配、主循环 |

## 修改代码的硬性规则

### 1. 改动前必须通读模块全部代码

修改任何模块（`.c` + `.h`）或为其添加新功能时，**必须先把该模块的所有代码完整阅读完毕**，
理清其内部状态机、数据生命周期、函数调用链和边界处理逻辑。禁止只看头文件声明或只看部分实现就开始写代码。

### 2. 调用依赖层 API 前必须通读依赖层全部代码

当你需要调用的 API 来自其他模块时，**必须完整阅读该依赖模块的所有代码（`.c` + `.h`）**，
明确其公开 API 面：有哪些函数、类型、枚举值可用，各自的所有权语义（谁分配、谁释放、
是 borrowed 还是 owned 指针）、返回值约定（`bool` true=成功？`int` 0=成功？），
不得猜测或假设 API 的存在与行为。

例如：
- 修改 `clk_ascii_render` → 必须先通读 `clk_term`（完整理解 `clk_texture`/`clk_sprite`/样式注册的全部 API）
- 修改 `clk_menu_instance` → 必须先通读 `clk_menu`、`clk_menu_theme`、`clk_term`、`clk_key_io`
- 修改 `clk_app_setup` → 必须先通读 `clk_app_config`、`clk_clock`、`clk_ascii_render`、`clk_menu`、`clk_menu_theme`、`clk_menu_instance`

### 3. 命名规范

**禁止使用**奇怪的前缀/后缀、逆天缩写（如 `cnt` 代替 `count`、`buf` 代替 `buffer`、`sz` 代替 `size` 等）。
命名必须自解释，能从名字直接读懂含义。

#### 3.1 项目实际命名约定

| 类别 | 约定 | 示例 |
|------|------|------|
| 函数 | `clk_<模块>_<动作>_<宾语>`，全部 snake_case | `clk_json_object_set`, `clk_menu_add_item_str`, `clk_texture_write_cell` |
| 类型（struct/union/enum typedef） | `clk_` 前缀 + snake_case | `clk_color`, `clk_json_value`, `clk_menu_event` |
| 枚举值 | `CLK_` 前缀 + SCREAMING_SNAKE_CASE | `CLK_JSON_NULL`, `CLK_MENU_INPUT_NEXT_ITEM`, `CLK_CURSOR_BLOCK_BLINK` |
| 宏常量 | `CLK_` 前缀 + SCREAMING_SNAKE_CASE | `CLK_ATTR_BOLD`, `CLK_ATTR_ITALIC` |

#### 3.2 子命名空间

- Term 模块拆分为三个子前缀：`clk_term_*`（终端生命周期/渲染/样式）、`clk_texture_*`（纹理操作）、`clk_sprite_*`（精灵操作）。
- JSON 模块按操作对象拆分子命名空间：`clk_json_object_*`、`clk_json_array_*`、`clk_json_object_iterator_*`。
- Menu 模块的类型化操作使用 `_<类型>` 后缀：`clk_menu_add_item_str`、`clk_menu_add_item_int`、`clk_menu_set_value_str`。

#### 3.3 发现不良命名时立即改造

遇到不符合上述约定的命名（如 `CELL_NORMAL` 缺少 `CLK_` 前缀），**必须立即同步改造**全部引用点，
不得容忍遗留。改造范围包括：声明、定义、所有调用处、测试代码。

#### 3.4 常见生命周期后缀

| 后缀 | 语义 | 对应析构 |
|------|------|----------|
| `_init` | 栈上或原地初始化，不分配堆内存 | `_close` 或无析构 |
| `_create` | 堆上分配新实例 | `_destroy` |
| `_free` | JSON 模块专用的 `_create` 配对析构（历史遗留，等同于 `_destroy`） | `clk_json_free` |
| `_init_borrowed` | 借用外部数据，不接管所有权 | 无需析构 |

## 构建与测试

```bash
# 配置 (WSL debug)
cmake --preset wsl-debug

# 构建
cmake --build build/wsl-debug

# 运行所有测试
cd build/wsl-debug && ctest --output-on-failure

# 运行单个测试
./build/wsl-debug/test/test_json

# 运行主应用
./build/wsl-debug/src/Clock
```

debug 构建默认启用 AddressSanitizer 和 UndefinedBehaviorSanitizer。

## 代码风格

- 使用 `.clang-format`（Google 风格），提交前确保 `clang-format -i <file>`。
- 不在代码中写注释，除非绝对必要。靠自解释的命名和清晰的函数拆分来表达意图。
- 测试使用 `test/test_utils.h` 提供的 `TEST`/`TEST_REQUIRE` 宏。

## 目录结构

| 目录 | 内容 |
|------|------|
| `include/` | 公开头文件，一个模块一个 `.h` |
| `src/` | 库实现 + `main.c` 入口 |
| `test/` | 测试代码，`test/test_utils.h` 提供测试基础设施 |
| `sandbox/` | 演示和基准测试可执行文件 |
| `assets/` | 配置文件、ASCII 字体 JSON、音频 MP3 |
| `docs/` | 设计文档（`key_io_design.md`、`menu_config.md`） |
