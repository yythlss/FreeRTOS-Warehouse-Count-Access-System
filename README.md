# FreeRTOS 仓库人员数量统计与门禁系统

这是一个基于 STM32F103C8T6 和 FreeRTOS 的仓库人数统计与门禁状态演示项目。系统使用两路 HC-SR04 分别检测入口和出口，通过 OLED 显示当前人数、时间和日期，并预留了 TF 卡日志功能。

## 项目来源

本项目在立创开源硬件平台的 **FreeRTOS 仓库人员数量统计与门禁系统** 基础上重新配置和开发，感谢原作者公开设计资料和源代码。

- 原项目作者：7anx
- 原项目地址：[https://oshwhub.com/7anx/project_kfijeaht](https://oshwhub.com/7anx/project_kfijeaht)
- 当前仓库：[yythlss/FreeRTOS-Warehouse-Count-Access-System](https://github.com/yythlss/FreeRTOS-Warehouse-Count-Access-System)

## 功能

- 两路 HC-SR04 检测人员进入和离开。
- 使用 TIM3 输入捕获测量 ECHO 高电平时间。
- 实时统计当前人数，人数不会小于 0 或超过设定上限。
- 满员时点亮状态 LED，再次检测到入口目标时蜂鸣器短鸣。
- OLED 显示当前人数、时间、日期和星期。
- RTC 日期和星期保存到备份寄存器，复位后可恢复。
- 可选 TF 卡日志，记录启动、进入、离开、拒绝进入和周期状态。
- 保留 SWD 调试接口，SPI1 使用 PB3、PB4、PB5 重映射引脚。

目前双路超声波、OLED 和 RTC 已完成实机测试。由于暂时没有 TF 卡和读卡模块，TF 卡读写功能尚未进行硬件测试，代码中默认关闭。

## 与原项目的主要区别

| 项目 | 原项目 | 当前版本 |
| --- | --- | --- |
| RTOS 接口 | CMSIS-RTOS V1 | CMSIS-RTOS V2 |
| FreeRTOS 配置 | Kernel V10.0.1 配置 | Kernel V10.3.1 配置 |
| HAL 时基 | SysTick | TIM1 |
| OLED 接口 | I2C1，PB8/PB9 | I2C2，PB10/PB11 |
| OLED 地址 | 固定 `0x3C` | 自动探测 `0x3C` 和 `0x3D` |
| OLED 列地址 | 从 `0x00` 开始 | 从 `0x02` 开始，适配当前 SH1106 屏幕 |
| OLED 内容 | 人数和时间 | 人数、时间、日期和英文星期 |
| RTC 时钟源 | 32.768 kHz LSE | HSE/128，输入频率 62500 Hz |
| RTC 备份 | DR1 保存初始化标记 | DR1 保存标记，DR2 保存日期，DR3 保存星期 |
| 超声波任务通知 | FreeRTOS 原生任务通知 | CMSIS-RTOS V2 线程标志 |
| 重复计数处理 | 固定时间间隔 | 目标锁存、离开确认和冷却时间 |
| 满员事件 | LED、蜂鸣器提示 | 增加 `DENIED` 事件，满员时不增加人数 |
| TF 卡 | 默认运行日志任务 | 增加 `TF_CARD_ENABLED` 开关，当前默认关闭 |
| 任务初始化 | 创建任务和队列 | 启动调度器前检查任务、队列和互斥锁句柄 |

SD 卡底层仍使用原项目的 SPI1 硬件 SPI 驱动，没有改成 GPIO 模拟 SPI。

## 硬件连接

所有模块必须与 STM32 共地。

| 模块 | 模块引脚 | STM32 引脚 | 说明 |
| --- | --- | --- | --- |
| OLED | SCL | PB10 | I2C2_SCL |
| OLED | SDA | PB11 | I2C2_SDA |
| 入口 HC-SR04 | TRIG | PB0 | GPIO 输出 |
| 入口 HC-SR04 | ECHO | PA6 | TIM3_CH1 |
| 出口 HC-SR04 | TRIG | PB1 | GPIO 输出 |
| 出口 HC-SR04 | ECHO | PA7 | TIM3_CH2 |
| 状态 LED | 控制端 | PA0 | 低电平点亮 |
| 蜂鸣器 | SIG/IN | PA1 | 高电平鸣叫 |
| TF/SD | CS | PA4 | 低电平片选 |
| TF/SD | SCK | PB3 | SPI1 重映射 |
| TF/SD | MISO | PB4 | SPI1 重映射 |
| TF/SD | MOSI | PB5 | SPI1 重映射 |
| ST-Link | SWDIO | PA13 | SWD 调试 |
| ST-Link | SWCLK | PA14 | SWD 调试 |

HC-SR04 一般使用 5 V 供电，ECHO 输出也接近 5 V。连接 PA6、PA7 前必须增加分压或电平转换电路，避免损坏 STM32。

PB3、PB4 默认与 JTAG 复用，本工程关闭完整 JTAG并保留SWD，使这些引脚可以用于SPI1。

## OLED 显示

当前显示格式：

```text
People: 0/6

   HH:MM:SS

YYYY/MM/DD SAT
```

OLED驱动使用128×64显存，支持5×7普通字符和2倍时间字符。启动时会检测 `0x3C` 和 `0x3D` 两个常用地址；没有检测到屏幕时直接返回，不影响其他任务运行。

当前屏幕使用SH1106驱动，刷新每一页时将列地址设置为 `0x02`，用于修正两像素水平偏移。

## 软件结构

| 任务 | 优先级 | 栈大小 | 主要职责 |
| --- | --- | ---: | --- |
| `InSensorTask` | AboveNormal | 1024 B | 入口测距、满员判断和入口事件 |
| `OutSensorTask` | AboveNormal | 1024 B | 出口测距和出口事件 |
| `PeopleDisplayTask` | Normal | 1024 B | 更新人数、门禁状态、OLED和日志缓存 |
| `RTCTask` | Low | 512 B | 每秒读取RTC并更新时间缓存 |
| `TFCardTask` | BelowNormal | 4096 B | 挂载FatFs并写入日志；关闭TF功能时仅延时等待 |

使用的RTOS对象：

- `EventQueue`：传递 `IN`、`OUT` 和 `DENIED` 事件。
- `peopleMutex`：保护当前人数。
- `measureMutex`：避免两路超声波同时触发。
- `log_buffer[16]`：TF功能启用后缓存待写入事件。

## 人数检测逻辑

每个超声波任务以120 ms为周期测距。距离在2～10 cm之间时判定为检测到目标。

一次目标触发后，对应通道进入锁存状态。只有连续两次检测到目标离开，才会解除锁存，允许下一次计数。代码还保留600 ms冷却时间，用于抑制短时间内的重复事件。

当前人数上限为6：

```c
#define MAX_PEOPLE                 6
#define DISTANCE_MIN_CM            2.0f
#define DISTANCE_THRESHOLD_CM      10.0f
#define SENSOR_SAMPLE_PERIOD_MS    120U
#define SENSOR_RELEASE_SAMPLES     2U
#define EVENT_COOLDOWN_MS          600U
```

## RTC

当前RTC使用8 MHz HSE经过128分频，RTC输入频率为62500 Hz：

```text
HSE 8 MHz -> HSE/128 -> RTC 62500 Hz
```

原先使用LSE时，当前实物板的RTC计数器没有正常按秒递增，切换到HSE/128后恢复走时。

STM32F1 HAL将日期保存在内存中，普通复位后可能重新变成2000年。本项目将年月日打包保存到DR2，并把星期保存到DR3，启动时再恢复到 `hrtc.DateToUpdate`。

`RTC_BKP_MAGIC` 用于判断RTC是否已经设置过。修改 `RTC_SET_*` 后，需要同时修改 `RTC_BKP_MAGIC`，新时间才会在下次启动时写入。

HSE依赖主电源，完全断电后不能像LSE一样依靠VBAT继续走时。如果需要断电计时，应修复PC14/PC15上的32.768 kHz晶振电路，或增加外部RTC模块。

## TF/SD日志

当前默认配置：

```c
#define TF_CARD_ENABLED            0U
```

连接读卡模块并完成硬件验证后，可将其改为 `1U`。启用后，代码会使用SPI1和FatFs尝试创建以下文件：

- `boot.txt`：记录 `BOOT` 事件。
- `log.csv`：记录时间、事件和人数。

`log.csv` 的表头为：

```text
time,event,people
```

事件类型包括 `IN`、`OUT`、`DENIED` 和周期状态 `STAT`。挂载失败时任务每5秒重新尝试。

这部分代码尚未使用真实TF卡验证。

## 编译

### Keil

打开：

```text
MDK-ARM/My_warehouse.uvprojx
```

当前工程已使用ARM Compiler 5.06完成构建，结果为：

```text
0 Error(s), 0 Warning(s)
```

### STM32CubeMX

配置文件：

```text
My_warehouse.ioc
```

自定义业务代码主要位于 `USER CODE BEGIN/END` 区域，`oled_user.c/.h` 和 `sd_spi.c/.h` 是独立用户文件。重新生成代码后建议先检查Git差异，再进行编译和提交。

## 项目目录

```text
Core/             业务逻辑、任务和自定义驱动
Drivers/          STM32 HAL与CMSIS
FATFS/            FatFs初始化和diskio适配
Middlewares/      FreeRTOS、CMSIS-RTOS V2与FatFs
MDK-ARM/          Keil工程
EWARM/            CubeMX生成的IAR工程目录
My_warehouse.ioc  STM32CubeMX配置
```

## 许可说明

使用、修改或再发布本项目时，请同时查看并遵守[原项目页面](https://oshwhub.com/7anx/project_kfijeaht)中的许可和使用要求。

STM32 HAL、CMSIS、FreeRTOS和FatFs分别遵守各自源码附带的许可证。本仓库当前没有额外声明一份覆盖全部衍生代码的许可证。
