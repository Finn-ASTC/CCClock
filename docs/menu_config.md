# CLK Menu 配置文件设计

## 1. 概述

菜单外观通过 JSON 配置文件定义,四层架构:

```
Layer 1  defs (leaf)      — 最小渲染单元:一段带样式的字符串
  ↓ 递归组合
Layer 2  defs (composite) — 普通复合:叶子或复合的数组,任意深度嵌套
  ↓ 每个引用展开为一行内的一段
Layer 3  sections         — 区块:normal / tab_bar / item_list 三类
  ↓ 从上到下拼接
Layer 4  framework        — 完整菜单框架
```

### 统一递归格式

`defs` 中每个 key 的值可以是三种形式之一:

| 形式 | 含义 |
|------|------|
| object (leaf) | 叶子渲染单元,`type` 决定渲染行为 |
| object (special) | 特殊复合,含 active/inactive 两套布局 (`tab` / `item_label` / `item_value`) |
| array | 普通复合,引用其他 defs,递归展开 |

---

## 2. defs — 渲染单元定义

### 2.1 Leaf 叶子

#### string

固定字符串,占其长度格。

```jsonc
"corner_tl": { "type": "string", "string": "┌", "fg": "#888", "bg": "#000" }
```

| 字段 | 必须 | 说明 |
|------|------|------|
| `type` | 是 | `"string"` |
| `string` | 是 | 显示内容,可多字符 |
| `fg` | 否 | 前景色 `"#RRGGBB"`,默认终端默认色 |
| `bg` | 否 | 背景色 `"#RRGGBB"`,默认终端默认色 |
| `attr` | 否 | `"bold"` / `"dim"` / `"italic"` / `"none"`,默认 `"none"` |

> `fg` / `bg` / `attr` 是所有叶子共有的样式字段,以下不再复述。

`type: "string"` 也可用于填充——例如内容为 `"─"` 或 `" "` 的叶子,配合 row 层的 `fill: N` 标记实现循环平铺。是否填充不取决于 def 的类型,只取决于 row 引用处是否写了 `fill`。

#### tab_str

动态内容:tab 按钮名。内容由代码运行时填入。样式分 active/inactive 两套。

```jsonc
"tab_str": {
    "type": "tab_str",
    "active":   { "fg": "#000", "bg": "#FFF", "attr": "bold" },
    "inactive": { "fg": "#888", "bg": "#000", "attr": "dim"  }
}
```

#### item_label_str

动态内容:item 的标签名。内容由代码运行时填入。

```jsonc
"item_label_str": {
    "type": "item_label_str",
    "active":   { "fg": "#000", "bg": "#FFF", "attr": "bold" },
    "inactive": { "fg": "#CCC", "bg": "#000", "attr": "none" }
}
```

#### item_value_str

动态内容:item 的值。内容由代码运行时填入。

```jsonc
"item_value_str": {
    "type": "item_value_str",
    "active":   { "fg": "#0F0", "bg": "#FFF", "attr": "none" },
    "inactive": { "fg": "#0F0", "bg": "#000", "attr": "none" }
}
```

### 2.2 特殊复合

系统内每种必须且仅一份,名字固定为 `"tab"` / `"item_label"` / `"item_value"`。

```jsonc
"tab": {
    "type": "tab",
    "active":   ["br_open",  "tab_str", "br_close"],
    "inactive": ["gap_2",    "tab_str", "gap_2"]
},
"item_label": {
    "type": "item_label",
    "active":   ["indicator", "item_label_str"],
    "inactive": ["indent",    "item_label_str"]
},
"item_value": {
    "type": "item_value",
    "active":   ["item_value_str"],
    "inactive": ["item_value_str"]
}
```

渲染时根据当前状态选用 `active` 或 `inactive` 布局——tab 选中的那一栏用 active,item 当前选中行用 active,其余用 inactive。

### 2.3 普通复合

纯数组,引用 `defs` 中其他定义。从左到右展开拼接。可嵌套任意深度。

```jsonc
"frame_top":    ["corner_tl", "line_h", "corner_tr"],
"header_block": ["frame_top", "frame_line"]
```

---

## 3. sections — 三种区块

### 3.1 normal

每行展开一次。用于装饰(框线、分隔线等)。

约束:行内不得出现 `tab` / `item_label` / `item_value`。

### 3.2 tab_bar

每行展开一次。行内 `tab` 特殊复合被替换为全部 tab 按钮。

约束:行内必须含 `tab`,不得含 `item_label` 或 `item_value`。

### 3.3 item_list

`rows` 按当前可见 item 数量纵向重复每个 item。

约束:rows 内必须**同时**含 `item_label` 和 `item_value`,不得含 `tab`。

---

## 4. 行内格式 (row)

row 内每个元素有三种写法:

```jsonc
// 写法 A:字符串简写,引用 defs
"edge_l"

// 写法 B:等价 A
{ "ref": "edge_l" }

// 写法 C:带 fill 锚点
{ "ref": "blank", "fill": 0.30 }
```

### fill 标记

`fill` 是 **row 层级的标记**：**将该 ref 指向的单元渲染字符串循环平铺**,从当前行位置填到总行宽 N%(0.0~1.0)处。

| 规则 | 说明 |
|------|------|
| 含义 | `fill: N` = 循环平铺到 N% 位置,不是增量 |
| 递增 | 同一行中 `fill` 值必须严格递增,否则解析报错 |
| 末位 | 最后一个 fill 可不写锚点数据,默认填到行尾，并为最后单元留出位置 |
| 越界 | 若当前位置已超过 `fill` 对应的位置,填 0 格 |
| 可给多数 ref | 任意 string 叶子 / 普通复合 / `tab` 特殊复合 都可用 `fill` |

`fill: N` 的具体效果取决于目标类型：

| 目标类型 | `fill: N` 的行为 |
|----------|-------------------|
| string 叶子 / 普通复合 | 将该单元渲染后的字符串循环平铺,填到 N% |
| `tab` 特殊复合 | **例外**：不循环平铺。tab 栏限制在 N% 宽度内渲染,超出截断 |
| `item_label` / `item_value` | **禁止使用 `fill`**,解析报错 |

不加 `fill` 的引用使用单元自身固定宽度(如 `"│"` = 1 格)。

示例:

```
"arrow": { "type": "string", "string": "-<>-" }

{ "ref": "arrow", "fill": 0.40 }
→ "-<>--<>--<>--<>--<>-"   (循环平铺到 40%)
```

```jsonc
[
    "edge_l",
    { "ref": "blank", "fill": 0.10 },
    { "ref": "tab",   "fill": 0.40 },
    "blank",
    "edge_r"
]
// 左边框 → 空格平铺到 10% → tab 栏限宽 40% → 剩余空白填到底 → 右边框
```

---

## 5. framework — 框架布局

区块名数组,从上到下垂直拼接。

```jsonc
"framework": {
    "layout": [
        "header",
        "tab_row",
        "separator",
        "item_block",
        "footer"
    ]
}
```

---

## 6. 完整示例 JSON

```jsonc
{
    "defs": {

        "corner_tl": { "type": "string", "string": "┌", "fg": "#888", "bg": "#000"        },
        "corner_tr": { "type": "string", "string": "┐", "fg": "#888", "bg": "#000"        },
        "corner_bl": { "type": "string", "string": "└", "fg": "#888", "bg": "#000"        },
        "corner_br": { "type": "string", "string": "┘", "fg": "#888", "bg": "#000"        },
        "edge_l":    { "type": "string", "string": "│", "fg": "#888", "bg": "#000"        },
        "edge_r":    { "type": "string", "string": "│", "fg": "#888", "bg": "#000"        },
        "gap":       { "type": "string", "string": " ", "fg": "#000", "bg": "#000"        },
        "gap_2":     { "type": "string", "string": "  ", "fg": "#000", "bg": "#000"       },
        "br_open":   { "type": "string", "string": "[", "fg": "#FFF", "bg": "#000"       },
        "br_close":  { "type": "string", "string": "]", "fg": "#FFF", "bg": "#000"       },
        "indicator": { "type": "string", "string": "▶ ", "fg": "#FFF", "bg": "#000"      },
        "indent":    { "type": "string", "string": "  ", "fg": "#000", "bg": "#000"       },

        "line_h":  { "type": "string", "string": "─", "fg": "#888", "bg": "#000"             },
        "line_s":  { "type": "string", "string": "─", "fg": "#444", "bg": "#000"             },
        "blank":   { "type": "string", "string": " ", "fg": "#000", "bg": "#000"             },

        "tab_str": {
            "type": "tab_str",
            "active":   { "fg": "#000", "bg": "#FFF", "attr": "bold" },
            "inactive": { "fg": "#888", "bg": "#000", "attr": "dim"  }
        },
        "item_label_str": {
            "type": "item_label_str",
            "active":   { "fg": "#000", "bg": "#FFF", "attr": "bold" },
            "inactive": { "fg": "#CCC", "bg": "#000", "attr": "none" }
        },
        "item_value_str": {
            "type": "item_value_str",
            "active":   { "fg": "#0F0", "bg": "#FFF", "attr": "none" },
            "inactive": { "fg": "#0F0", "bg": "#000", "attr": "none" }
        },

        "tab": {
            "type": "tab",
            "active":   ["br_open",  "tab_str", "br_close"],
            "inactive": ["gap_2",    "tab_str", "gap_2"]
        },
        "item_label": {
            "type": "item_label",
            "active":   ["indicator", "item_label_str"],
            "inactive": ["indent",    "item_label_str"]
        },
        "item_value": {
            "type": "item_value",
            "active":   ["item_value_str"],
            "inactive": ["item_value_str"]
        },

        "frame_top":    ["corner_tl", "line_h", "corner_tr"],
        "frame_bottom": ["corner_bl", "line_h", "corner_br"],
        "frame_line":   ["edge_l",    "blank",  "edge_r"],
        "sep_line":     ["edge_l",    "line_s", "edge_r"],
        "header_block": ["frame_top", "frame_line"]
    },

    "sections": {
        "header": {
            "type": "normal",
            "rows": [
                "frame_top",
                "frame_line"
            ]
        },
        "tab_row": {
            "type": "tab_bar",
            "rows": [
                [
                    "edge_l",
                    { "ref": "tab", "fill": 0.40 },
                    "blank",
                    "edge_r"
                ]
            ]
        },
        "separator": {
            "type": "normal",
            "rows": [
                "sep_line"
            ]
        },
        "item_block": {
            "type": "item_list",
            "rows": [
                [
                    "edge_l",
                    { "ref": "blank", "fill": 0.10 },
                    "item_label",
                    "blank",
                    "edge_r"
                ],
                [
                    "edge_l",
                    { "ref": "blank", "fill": 0.10 },
                    "item_value",
                    "blank",
                    "edge_r"
                ]
            ]
        },
        "footer": {
            "type": "normal",
            "rows": [
                "frame_bottom"
            ]
        }
    },

    "framework": {
        "layout": [
            "header",
            "tab_row",
            "separator",
            "item_block",
            "footer"
        ]
    }
}
```

---

## 7. 约束速查表

| # | 约束 |
|---|------|
| 1 | `tab` / `item_label` / `item_value` 三种特殊复合必须在 defs 中有且各仅一份 |
| 2 | `tab_bar` 行内必须含 `tab`,不得含 `item_label` 或 `item_value` |
| 3 | `item_list` rows 内必须同时含 `item_label` 和 `item_value`,不得含 `tab` |
| 4 | `normal` 行内不得含 `tab` / `item_label` / `item_value` |
| 5 | row 内 `fill: N` 值必须严格递增;最后一个可不写(默认行尾) |
| 6 | `fill` 不得加在 `item_label` 或 `item_value` 的引用上 |
| 7 | 被 `fill` 修饰过的 ref,其对应的 def 不得再出现在任何 composite 数组成员中 |

---

## 8. 渲染流程

```
clk_menu_render_to_texture(menu, tex)
│
├─ 1. 遍历 framework.layout 中的每个 section
│
│   ├─ type = normal:
│   │     逐行展开 rows[] → 解析每行的 fill 锚点 → 计算各单元宽度
│   │     → 将叶子内容写入 texture 对应位置
│   │
│   ├─ type = tab_bar:
│   │     逐行展开 rows[],遇 "tab" 特殊复合时:
│   │       → 遍历所有 tab,当前 tab 用 active 布局,其余用 inactive
│   │       → 每个 tab 展开成 [tab_str 的 active/inactive 样式 + 前后缀]
│   │       → tab 之间插入空白分隔
│   │
│   ├─ type = item_list:
│   │     对当前 tab 的每个可见 item (从 scroll_offset 开始):
│   │       → 重复 rows 整个块（含装饰行）
│   │       → 当前选中 item 用 active 布局,其余用 inactive
│   │       → "item_label" 特殊复合展开时,item_label_str 替换为实际 label 字符串
│   │       → "item_value" 特殊复合展开时,item_value_str 替换为实际 value 字符串
│   │       → 超出菜单高度的截断不渲染
│   │
│   └─ section 之间垂直紧接,总高度 = 各 section 行数之和
│
└─ 2. 返回:texture 已完全填充
```
