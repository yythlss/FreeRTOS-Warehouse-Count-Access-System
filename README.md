# FreeRTOS 仓库人员数量统计与门禁系统

基于 STM32F103C8T6、FreeRTOS、双路 HC-SR04、0.96 英寸 OLED、RTC 和 TF 卡设计的仓库人数统计与门禁状态演示系统。

> [!IMPORTANT]
> 本仓库是基于立创开源硬件平台项目进行复现和改进的二次开发版本，并非原始方案。感谢原作者公开设计资料与实现思路。

## 开源来源与致谢

- 原项目：**FreeRTOS 仓库人员数量统计与门禁系统**
- 原项目作者/发布者：**7anx**
- 原项目链接：[立创开源硬件平台（OSHWHub）](https://oshwhub.com/7anx/project_kfijeaht)
- 本改进版维护者：**yythlss**
- 本仓库：[yythlss/FreeRTOS-Warehouse-Count-Access-System](https://github.com/yythlss/FreeRTOS-Warehouse-Count-Access-System)

本仓库保留了原方案的核心目标和总体思路：通过入口、出口两路超声波传感器检测人员进出，由 FreeRTOS 任务并发处理人数、显示、RTC 和存储功能。

## 与原方案的主要不同点

以下对比以本次复现时参考的原方案代码与配置为准；立创开源页面若有后续更新，可能与此表存在差异。

| 项目 | 原方案/参考工程 | 本改进版 |
| --- | --- | --- |
| RTOS 接口 | CMSIS-RTOS V1 API | CMSIS-RTOS V2 API |
| 系统时基 | 原工程时基配置 | HAL 使用 TIM1，FreeRTOS 使用 SysTick |
| OLED 总线 | I2C1 重映射至 PB8/PB9 | I2C2：PB10/PB11 |
| OLED 兼容性 | 固定 SSD1306 地址 | 自动探测 `0x3C` 和 `0x3D`，屏幕缺失时安全运行 |
| RTC 初始化 | 调试阶段可能反复设时 | 使用 RTC 备份寄存器标记，仅首次启动写入初始时间 |
| RTC 文件时间 | 基础 RTC 显示 | 为 FatFs 实现有效的 FAT 文件时间戳 |
| 人员触发判定 | 固定时间消抖 | 增加“检测—离开—重新允许触发”锁存，减少人员停留造成的重复计数 |
| 超声波测距 | 双路 TIM3 输入捕获 | 保留双路输入捕获，改为 V2 线程标志通知，并用互斥锁避免两路串扰 |
| 满员处理 | LED/蜂鸣器提示 | 增加 `DENIED` 事件，满员时不增加人数并记录拒绝事件 |
| TF/FatFs | SD SPI 与文件记录 | 补全 `diskio`、扇区信息、CSV 日志和无卡重试/禁用机制 |
| 无 TF 模块运行 | 可能持续尝试初始化 | 提供 `TF_CARD_ENABLED` 编译开关；默认关闭，不会因无卡探测拖慢 RTC |
| Keil 免费版 | 未专门控制镜像尺寸 | 使用轻量文本格式化，启用/禁用 TF 两种模式均低于 32 KiB 限制 |
| 任务健壮性 | 基础任务创建 | 启动前检查任务、消息队列和互斥锁是否创建成功 |

## 功能

- 两个 HC-SR04 分别检测入口和出口。
- 当前人数限制在 `0` 到 `MAX_PEOPLE` 之间。
- OLED 显示当前人数、人数上限和 RTC 时间。
- 人数达到上限时点亮 LED，继续进入时蜂鸣器报警。
- RTC 每秒更新，并通过 LSE 和备份域保持时间。
- TF 功能启用后生成 `boot.txt` 和 `log.csv`。
- 日志记录 `IN`、`OUT`、`DENIED` 和周期状态 `STAT`。
- OLED 或 TF 模块缺失时，其他任务仍可继续运行。

> 当前“门禁锁定”由 PA0 LED 模拟。仓库中的代码没有直接驱动继电器、电磁锁或舵机；实际门锁必须增加独立驱动与保护电路。

## 硬件连接

所有模块必须与 STM32 共地。

| 功能 | 模块端 | STM32 引脚 | 说明 |
| --- | --- | --- | --- |
| OLED | SCL | PB10 | I2C2 SCL |
| OLED | SDA | PB11 | I2C2 SDA |
| 入口 HC-SR04 | TRIG | PB0 | GPIO 输出 |
| 入口 HC-SR04 | ECHO | PA6 | TIM3_CH1，必须降压到约 3.3 V |
| 出口 HC-SR04 | TRIG | PB1 | GPIO 输出 |
| 出口 HC-SR04 | ECHO | PA7 | TIM3_CH2，必须降压到约 3.3 V |
| 状态 LED | 控制端 | PA0 | 低电平点亮 |
| 蜂鸣器 | SIG/IN | PA1 | 高电平鸣叫 |
| TF 卡 | CS | PA4 | 低电平片选 |
| TF 卡 | SCK | PB3 | SPI1 重映射 |
| TF 卡 | MISO | PB4 | SPI1 重映射 |
| TF 卡 | MOSI | PB5 | SPI1 重映射 |

HC-SR04 通常使用 5 V 供电，ECHO 输出也接近 5 V。建议每路使用 `1 kΩ + 2 kΩ` 电阻分压后再连接 PA6/PA7。裸蜂鸣器、继电器和电磁锁不能直接由 STM32 GPIO 驱动。

PB3/PB4 默认与 JTAG 复用。本工程关闭完整 JTAG、保留 SWD，因此仍可使用 PA13/PA14 下载和调试。

## FreeRTOS 任务

| 任务 | 优先级 | 栈（words） | 职责 |
| --- | --- | --- | --- |
| `InSensorTask` | AboveNormal | 256 | 入口测距、满员判断和进入事件 |
| `OutSensorTask` | AboveNormal | 256 | 出口测距和离开事件 |
| `PeopleDisplayTask` | Normal | 256 | 更新人数、门禁状态、OLED 和日志缓存 |
| `RTCTask` | Low | 128 | 每秒读取并缓存 RTC |
| `TFCardTask` | BelowNormal | 1024 | TF 启用后挂载文件系统并写入日志 |

其他 RTOS 对象：

- `EventQueue`：传递 `IN`、`OUT`、`DENIED` 事件。
- `peopleMutex`：保护当前人数。
- `measureMutex`：保证两路 HC-SR04 不同时触发，减少声波串扰和 TIM3 竞争。
- 静态日志环形缓冲区：TF 启用后暂存待写事件，不额外消耗 FreeRTOS 动态堆。

## 关键参数

主要参数位于 `Core/Src/main.c`：

```c
#define MAX_PEOPLE                 5
#define DISTANCE_THRESHOLD_CM      20.0f
#define SENSOR_RELEASE_SAMPLES     2U
#define EVENT_COOLDOWN_MS          600U
```

首次启动时间也在同一文件中配置：

```c
#define RTC_SET_YEAR               2026U
#define RTC_SET_MONTH              7U
#define RTC_SET_DATE               31U
#define RTC_SET_HOUR               22U
#define RTC_SET_MINUTE             0U
#define RTC_SET_SECOND             0U
```

RTC 写入备份标记后，普通复位不会再次覆盖时间。需要重新写入初始时间时，应清除 RTC 备份域或更换 `RTC_BKP_MAGIC`。

## TF 卡开关

由于当前开发阶段没有连接 TF 模块，仓库默认关闭 TF 访问：

```c
#define TF_CARD_ENABLED            0U
```

模块接线完成、TF 卡格式化为 FAT16/FAT32 后，将其修改为：

```c
#define TF_CARD_ENABLED            1U
```

无卡模式和 TF 启用模式均已通过 Keil 编译，结果均为 `0 Error(s), 0 Warning(s)`。

## 编译

### Keil MDK-ARM

打开：

```text
MDK-ARM/My_warehouse.uvprojx
```

使用 ARM Compiler 5.06 和 STM32F1 Device Pack 构建。当前默认无卡模式的代码量约为 24 KiB，满足 Keil MDK-Lite 32 KiB 限制。

### STM32CubeMX

配置文件：

```text
My_warehouse.ioc
```

业务代码主要放在 `USER CODE BEGIN/END` 区域；`oled_user.c/.h`、`sd_spi.c/.h` 是独立用户文件。重新生成代码后仍建议先检查 Git 差异再提交。

## 目录说明

```text
Core/                     用户代码、HAL 入口和中断
Drivers/                  STM32 HAL 与 CMSIS
FATFS/                    FatFs 应用层和 diskio 适配
Middlewares/              FreeRTOS、CMSIS-RTOS V2 与 FatFs
MDK-ARM/                  Keil 工程
EWARM/                    IAR 工程文件
My_warehouse.ioc          STM32CubeMX 配置
```

## 开源与许可证说明

本仓库是衍生改进项目。使用、修改或再发布时，请同时保留原项目来源，并遵守[原项目页面](https://oshwhub.com/7anx/project_kfijeaht)标注的开源协议和使用要求。本仓库不扩大解释原项目的授权范围。

STM32 HAL、CMSIS、FreeRTOS 和 FatFs 等第三方组件分别遵守其自身许可证；相关版权声明以源码文件和上游项目为准。
