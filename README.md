# CCClock — Terminal ASCII Art Clock

终端 ASCII 艺术时钟，支持闹钟、番茄钟、BGM、JSON 配置热重载。

## 快速开始

### 依赖
- CMake 3.25+ / Ninja / vcpkg
- miniaudio（vcpkg 自动安装）

### 构建
```bash
cmake --preset wsl-debug
cmake --build build/wsl-debug
./build/wsl-debug/Clock
```

## 快捷键

| 键 | 功能 |
|----|------|
| `s` | 打开/关闭菜单 |
| `f` | 切换时间格式 |
| `r` | 切换字体 |
| `j/k` / `↑/↓` | 菜单项上下 |
| `h/l` / `←/→` | 调整值 |
| `Tab` | 切换标签页 |
| `Enter` | 确认操作 |
| `q` | 退出菜单 / 退出程序 |

## 配置

配置文件 `assets/config/app_config.json`，修改后热重载自动生效。

## 构建选项

```bash
cmake --preset wsl-debug       # Debug + ASan/UBSan
cmake --preset wsl-release     # Release -O3 -flto
ctest --preset wsl-debug       # 运行测试
```

## 许可

MIT
