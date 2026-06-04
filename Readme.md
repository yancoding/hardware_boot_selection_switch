# 使用 Raspberry Pi Pico 实现硬件启动选择开关

这是一个基于 Raspberry Pi Pico、Pico SDK 和 TinyUSB 的硬件启动选择项目。Pico 会模拟一个 USB 大容量存储设备，向主机提供 `switch.cfg` 配置文件；GRUB 读取该文件中的 `os_hw_switch` 值后，根据外部开关位置选择默认启动项。

项目灵感来自 [Hackaday.io: Hardware Boot Selection Switch](https://hackaday.io/project/179539-hardware-boot-selection-switch)。完整制作步骤可参考 [Hackster.io 项目页](https://www.hackster.io/Madrajib/hardware-boot-select-switch-using-pico-a3e3d5)，演示视频见 [YouTube](https://youtu.be/8JmKnxeOoC0)。

## 工作原理

- Pico 通过 TinyUSB 同时提供 CDC 和 MSC 接口。
- MSC 盘中包含 `switch.cfg`，内容类似 `set os_hw_switch=0`。
- 固件读取 `SWITCH_PIN` 的电平，并动态返回 `0` 或 `1`。
- GRUB 启动时查找该 USB 盘，加载 `switch.cfg`，再决定启动 Linux、Windows 或默认系统。

当前代码中开关引脚为 `GPIO 28`：

```c
#define SWITCH_PIN 28
```

可选的 TFT 状态屏使用 1.8 寸 128x160 ST7735S SPI 模块，用来显示当前将启动的系统：

| TFT 引脚 | Pico 引脚 | 说明 |
| --- | --- | --- |
| `GND` | `GND` | 地 |
| `VDD` | `3V3(OUT)` | 3.3V 供电 |
| `SCL` | `GPIO18` | SPI0 SCK |
| `SDA` | `GPIO19` | SPI0 MOSI |
| `RST` | `GPIO20` | 屏幕复位 |
| `DC` | `GPIO16` | 数据/命令选择 |
| `CS` | `GPIO17` | SPI 片选 |
| `BLK` | `GPIO21` | 背光控制 |

屏幕显示规则与 `switch.cfg` 保持一致：`0` 显示 Ubuntu，`1` 显示 Windows。USB suspend 时固件会关闭背光，resume 后重新打开并刷新当前状态。

## 环境要求

- Raspberry Pi Pico 或兼容 RP2040 开发板
- Pico SDK，并已设置 `PICO_SDK_PATH`
- CMake 3.12 或更高版本
- 可用的 C/C++ 交叉编译工具链

## 编译与烧录

```bash
git clone <this-repository-url>
cd hardware_boot_selection_switch

mkdir -p build
cd build
cmake ..
make
```

编译完成后，将生成的 `build/pico_msd.uf2` 复制到处于 BOOTSEL 模式的 Pico。烧录后，主机应能识别到一个 TinyUSB MSC 设备。

## Ubuntu / GRUB 配置

编辑 GRUB 自定义配置：

```bash
sudo vim /etc/grub.d/40_custom
```

加入以下逻辑。示例中的 `0000-1234` 是固件中 FAT12 盘的文件系统 UUID：

```bash
# 查找硬件开关对应的 USB 存储设备
search --no-floppy --fs-uuid --set hdswitch 0000-1234

# 找到设备后读取 switch.cfg，并根据开关位置选择启动项
if [ "${hdswitch}" ] ; then
  source ($hdswitch)/switch.cfg

  if [ "${os_hw_switch}" == 0 ] ; then
    # 启动 Linux
    set default="0"
  elif [ "${os_hw_switch}" == 1 ] ; then
    # 启动 Windows
    set default="2"
  else
    # 回退到系统默认配置
    set default="${GRUB_DEFAULT}"
  fi
else
  set default="${GRUB_DEFAULT}"
fi
```

保存后更新 GRUB：

```bash
sudo update-grub
```

`set default` 的编号需要根据本机 GRUB 菜单顺序调整。修改前建议先备份 `/etc/grub.d/40_custom`。

## 项目文件

- `main.c`：初始化 GPIO、TinyUSB，并循环处理 USB 与 LED 状态。
- `msc_disk.c`：实现虚拟 FAT12 磁盘和动态 `switch.cfg` 内容。
- `usb_descriptors.c`：定义 CDC、MSC 和字符串描述符。
- `tusb_config.h`：TinyUSB 编译配置。
- `CMakeLists.txt`：Pico SDK 构建入口。

## 注意事项

- 不要随意修改文件系统 UUID，GRUB 配置依赖该值定位设备。
- 如果更改 `SWITCH_PIN`，需要同步调整硬件接线。
- 写入 GRUB 配置前，请确认 Linux 和 Windows 对应的菜单编号。
- 当前项目未包含自动化测试，建议每次修改后重新编译并在 Pico 上验证 USB 枚举和开关行为。
