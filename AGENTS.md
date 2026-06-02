# Repository Guidelines

## 项目结构与模块组织

这是一个基于 CMake 和 Pico SDK 构建的 Raspberry Pi Pico 固件项目。源码文件位于仓库根目录：

- `main.c` 包含 GPIO 初始化、TinyUSB 任务处理、CDC 回显逻辑和 LED 状态闪烁。
- `msc_disk.c` 实现 USB 大容量存储设备行为。
- `usb_descriptors.c` 定义 USB 描述符。
- `tusb_config.h` 保存 TinyUSB 配置。
- `CMakeLists.txt` 定义 `pico_msd` 可执行目标及 Pico/TinyUSB 链接库。

当前没有独立的 `src/`、`include/`、`tests/` 或资源目录。除非有明确重构计划，新模块应继续放在根目录附近，保持现有布局。

## 构建、测试与开发命令

配置项目前先设置 `PICO_SDK_PATH`：

```bash
export PICO_SDK_PATH=/path/to/pico-sdk
mkdir -p build
cd build
cmake ..
make
```

构建产物为 `build/pico_msd.uf2`，可复制到 BOOTSEL 模式下的 Pico。也可以使用 `cmake --build build` 代替 `make`。

## 编码风格与命名约定

固件代码使用 C11，与 `CMakeLists.txt` 保持一致。函数和变量使用小写蛇形命名，例如 `read_switch_value()` 和 `blink_interval_ms`。宏和枚举常量使用大写命名，例如 `SWITCH_PIN` 和 `BLINK_MOUNTED`。

USB、GPIO 和 LED 逻辑优先拆成小型任务函数。新增 USB 回调时必须遵循 TinyUSB 的回调命名。编辑代码块时保持缩进一致，避免无关格式化。

## 测试指南

当前未包含自动化测试框架。验证修改时，应从干净的 `build/` 目录重新构建；涉及行为变更时，将 `pico_msd.uf2` 烧录到 Pico。重点检查 USB 枚举、LED 闪烁状态和生成的大容量存储内容。

如果后续添加测试，请放入 `tests/` 目录，并在此处记录运行命令。

## 提交与 Pull Request 规范

近期提交历史使用简短的祈使句，例如 `Add demo video link` 和 `Update Readme.md`。请沿用这种风格：提交信息保持简洁，并说明变更结果。

Pull Request 应包含变更摘要、使用的构建命令、已完成的硬件验证，以及 Pico SDK 或开发板相关假设。涉及行为变更时，建议附上串口日志或主机端 USB 枚举输出。

## 安全与配置提示

不要提交本地 Pico SDK 路径、生成的 `build/` 输出或机器相关配置。修改硬编码 USB 标识或文件系统内容时要谨慎；GRUB 集成可能依赖 `Readme.md` 中提到的文件系统 UUID 等稳定值。
