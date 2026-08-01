# FreeRTOS 仓库人员数量统计与门禁系统

基于 STM32F103C8T6、FreeRTOS、双路 HC-SR04、I2C OLED、片内 RTC 和预留 TF/SD 存储接口实现的仓库人数统计与门禁状态演示工程。

## 来源与致谢

本仓库是在以下开源项目和本地参考源码的基础上进行复现、重新配置和修改的衍生工程，不是原始方案：

- 原项目：FreeRTOS 仓库人员数量统计与门禁系统
- 原项目发布者：7anx
- 原项目页面：[立创开源硬件平台（OSHWHub）](https://oshwhub.com/7anx/project_kfijeaht)
- 本仓库：[yythlss/FreeRTOS-Warehouse-Count-Access-System](https://github.com/yythlss/FreeRTOS-Warehouse-Count-Access-System)

本次差异核对使用了工作区中的两个参考源码目录：

| 参考目录 | 从代码中确认的用途 |
| --- | --- |
| `My_project_fixed_5s` | 完整业务参考工程：双路测距、人数处理、OLED、RTC，以及每 5 秒执行的 TF 日志任务 |
| `SD_OLED_GPIO_SPI_ConfigFixed` | SD/OLED/GPIO/SPI 诊断工程：暂停入口、出口和人数显示任务，使用 GPIO 模拟 SPI，OLED 显示挂载、命令和写文件状态，并写入 `SDTEST.TXT` |

下文只记录上述源码、`.ioc` 配置和当前 `My_warehouse` 代码中能够直接核实的内容。

## 当前验证状态

| 项目 | 状态 |
| --- | --- |
| Keil 构建 | ARM Compiler 5.06 构建通过，`0 Error(s), 0 Warning(s)` |
| 双路 HC-SR04 | 已实机运行 |
| OLED | 已实机显示；当前驱动使用 SH1106 两列偏移 |
| RTC | 改用 HSE/128 后已实机走时 |
| TF/SD | 当前没有 TF 卡和读卡模块，尚未进行硬件读写验证；代码默认关闭访问 |

## 与两个参考源码的可验证差异

| 项目 | `My_project_fixed_5s` | `SD_OLED_GPIO_SPI_ConfigFixed` | 当前 `My_warehouse` |
| --- | --- | --- | --- |
| 工程用途 | 完整人数统计与日志业务 | SD/OLED/GPIO/SPI 单项诊断 | 完整人数统计、OLED 与 RTC；保留可选 TF 日志代码 |
| FreeRTOS 接口 | CMSIS-RTOS V1 | CMSIS-RTOS V1 | CMSIS-RTOS V2 |
| FreeRTOS 配置版本 | 配置头标注 Kernel V10.0.1 | 同左 | 配置头标注 Kernel V10.3.1 |
| HAL 时基 | SysTick | SysTick | TIM1；FreeRTOS 使用 SysTick |
| OLED 外设 | I2C1，PB8/PB9 | I2C1，PB8/PB9 | I2C2，PB10/PB11 |
| OLED 地址 | 固定 `0x3C` | 固定 `0x3C` | 启动时依次探测 `0x3C`、`0x3D`；均不存在时返回，不阻塞其他功能 |
| OLED 列地址 | 每页从列 `0x00` 开始 | 每页从列 `0x00` 开始 | 每页从列 `0x02` 开始，适配当前 SH1106 屏幕的两列偏移 |
| OLED 内容 | 人数和大号时间 | SD 诊断状态 | 人数、大号时间、日期和英文星期 |
| RTC 时钟源 | LSE，32768 Hz | LSE，32768 Hz | HSE/128，62500 Hz |
| RTC 备份数据 | DR1 保存初始化标记 | DR1 保存初始化标记 | DR1 保存初始化标记，DR2 保存日期，DR3 保存星期 |
| 超声波通知 | 原生 FreeRTOS 任务通知 | 相关业务任务在诊断版本中暂停 | CMSIS-RTOS V2 线程标志 |
| 人员重复触发控制 | 固定 1000 ms 时间间隔 | 入口、出口任务暂停 | 距离锁存、连续离开采样和 600 ms 冷却共同判断 |
| 人数与距离参数 | 上限 5，阈值 20 cm | 定义相同但业务任务暂停 | 上限 6，检测范围 2～10 cm |
| SD 通信 | SPI1 硬件 SPI，PB3/PB4/PB5 | GPIO 模拟 SPI，PB3/PB4/PB5 | `sd_spi.c` 沿用 `My_project_fixed_5s` 的硬件 SPI 实现 |
| SD 文件 | `boot.txt`、`log.csv` | `SDTEST.TXT` | 启用后使用 `boot.txt`、`log.csv`；没有移植诊断工程的 `SDTEST.TXT` 流程 |
| 无卡行为 | 日志任务周期尝试挂载 | 周期显示诊断错误并重试 | `TF_CARD_ENABLED=0` 时不访问卡；启用后每 5 秒重试 |
| 日志事件缓存 | 原生 FreeRTOS 日志队列 | 不运行人数日志流程 | 固定 16 项环形缓冲区，仅在 TF 功能启用时编译 |
| RTOS 对象检查 | 未发现统一的创建结果检查 | 未发现统一的创建结果检查 | 启动调度器前检查任务、消息队列和互斥锁句柄 |

这里没有将所有变化都称为“改进”。例如，RTC 从 LSE 改成 HSE/128 是针对当前硬件上 LSE 不走时所采用的工作方案，它有明确的断电限制。

## 当前功能

- 两个 HC-SR04 分别测量入口和出口距离。
- TIM3_CH1、TIM3_CH2 捕获 ECHO 脉宽。
- 两路测距共用一个互斥锁，避免同时触发造成声波串扰和 TIM3 竞争。
- 人数保持在 `0` 到 `MAX_PEOPLE` 之间。
- 达到人数上限时点亮 PA0 LED；再次检测到入口目标时产生 `DENIED` 事件并短鸣蜂鸣器。
- OLED 显示人数、`HH:MM:SS`、`YYYY/MM/DD` 和 `MON`～`SUN`。
- RTC 任务每秒读取一次时间和日期，并更新显示缓存及 FatFs 时间戳。
- TF 功能启用后，代码会尝试记录 `BOOT`、`IN`、`OUT`、`DENIED` 和周期 `STAT`。

当前门禁状态仅由 LED 和蜂鸣器表示。代码没有直接驱动继电器、电磁锁或舵机。

## OLED 显示

当前布局对应以下格式：

```text
People: 0/6

   HH:MM:SS

YYYY/MM/DD SAT
```

驱动使用 128×64 显存，每个普通字符占 6 像素宽。日期后预留一个字符位置，再显示三位英文星期。刷新每一页时从 SH1106 列地址 `0x02` 开始。

## 硬件连接

所有模块必须与 STM32 共地。

| 功能 | 模块引脚 | STM32 引脚 | 当前配置 |
| --- | --- | --- | --- |
| OLED | SCL | PB10 | I2C2_SCL |
| OLED | SDA | PB11 | I2C2_SDA |
| 入口 HC-SR04 | TRIG | PB0 | GPIO 推挽输出 |
| 入口 HC-SR04 | ECHO | PA6 | TIM3_CH1 |
| 出口 HC-SR04 | TRIG | PB1 | GPIO 推挽输出 |
| 出口 HC-SR04 | ECHO | PA7 | TIM3_CH2 |
| 状态 LED | 控制端 | PA0 | 低电平点亮 |
| 蜂鸣器 | SIG/IN | PA1 | 高电平鸣叫 |
| TF/SD | CS | PA4 | 低电平片选，空闲时拉高 |
| TF/SD | SCK | PB3 | SPI1 重映射 |
| TF/SD | MISO | PB4 | SPI1 重映射 |
| TF/SD | MOSI | PB5 | SPI1 重映射 |
| 调试 | SWDIO/SWCLK | PA13/PA14 | 保留 SWD，关闭完整 JTAG |

HC-SR04 的 ECHO 通常接近 5 V，连接 PA6、PA7 前必须使用分压或电平转换，将电压降至 STM32 可接受范围。继电器、电磁锁和大功率蜂鸣器也不能由 GPIO 直接驱动。

## FreeRTOS 任务和对象

CMSIS-RTOS V2 的 `stack_size` 单位是字节。当前生成代码中的实际配置如下：

| 任务 | 优先级 | `stack_size` | 职责 |
| --- | --- | ---: | --- |
| `InSensorTask` | AboveNormal | 1024 B | 入口测距、满员判断和入口事件 |
| `OutSensorTask` | AboveNormal | 1024 B | 出口测距和出口事件 |
| `PeopleDisplayTask` | Normal | 1024 B | 处理人数事件、门禁状态、OLED和日志缓存 |
| `RTCTask` | Low | 512 B | 每秒读取RTC并更新缓存 |
| `TFCardTask` | BelowNormal | 4096 B | TF启用时挂载FatFs、写日志；关闭时仅周期延时 |

其他对象：

- `EventQueue`：长度 10，每项为 `uint32_t`，传递 `IN`、`OUT`、`DENIED`。
- `peopleMutex`：保护 `current_people`。
- `measureMutex`：串行化两路超声波测距。
- `log_buffer[16]`：TF启用时缓存待写入事件，满时覆盖最旧记录。

## 当前参数

主要参数位于 `Core/Src/main.c`：

```c
#define MAX_PEOPLE                 6
#define DISTANCE_MIN_CM            2.0f
#define DISTANCE_THRESHOLD_CM      10.0f
#define SENSOR_SAMPLE_PERIOD_MS    120U
#define SENSOR_RELEASE_SAMPLES     2U
#define EVENT_COOLDOWN_MS          600U
#define TF_RETRY_PERIOD_MS         5000U
#define TF_CARD_ENABLED            0U
```

RTC 初始值同样位于该文件。修改 `RTC_SET_*` 后，如果备份寄存器仍有效，还要修改 `RTC_BKP_MAGIC`，使新值在下一次启动时写入一次。

## RTC 说明

当前 CubeMX 配置为：

```text
RTC clock source: HSE/128
HSE:              8 MHz
RTC input:        62500 Hz
```

选择 HSE/128 的原因是当前实物板的 LSE 通路虽然能置位就绪标志，但 RTC 计数器没有正常按秒递增；切换到 HSE/128 后已确认走时。

限制：

- HSE 依赖主电源，VDD 断电后不会像 LSE 那样由 VBAT 继续提供时钟。
- 如果需要完全断电后继续走时，应修复 PC14/PC15 上的 32.768 kHz 晶振及负载电容，或增加外部 RTC。
- DR2、DR3 能保存日期和星期，但不能弥补 HSE 在主电源断开时停止的问题。

## TF/SD 状态

当前没有 TF 卡和读卡模块，因此：

```c
#define TF_CARD_ENABLED 0U
```

这是当前实际使用状态。设置为 `1U` 后，代码会编译并运行硬件 SPI、FatFs、日志环形缓冲区和文件写入路径，但这些路径目前没有在真实 TF 硬件上验证，README 不将其描述为“已测试可用”。

当前代码中的预期文件格式来自实现本身：

```text
boot.txt: BOOT 记录
log.csv: time,event,people
```

`log.csv` 的事件类型包括 `IN`、`OUT`、`DENIED` 和 `STAT`。

## 构建

### Keil

打开：

```text
MDK-ARM/My_warehouse.uvprojx
```

当前本地验证使用 ARM Compiler 5.06。PB3、PB4 被 SPI1 重映射占用，因此工程关闭完整 JTAG并保留SWD。

### STM32CubeMX

打开：

```text
My_warehouse.ioc
```

重新生成代码后，应检查 Git 差异，确认 `USER CODE BEGIN/END` 中的 RTC、任务和显示逻辑仍然保留。`oled_user.c/.h`、`sd_spi.c/.h` 是独立用户文件。

## 目录

```text
Core/             业务逻辑、任务、自定义OLED/SD驱动和STM32接口
Drivers/          STM32 HAL与CMSIS
FATFS/            FatFs初始化和USER diskio适配
Middlewares/      FreeRTOS、CMSIS-RTOS V2与FatFs
MDK-ARM/          Keil工程
EWARM/            CubeMX生成的IAR工程目录（本次未验证）
My_warehouse.ioc  STM32CubeMX配置
```

## 许可说明

本仓库当前没有单独声明一份覆盖全部衍生代码的许可证。使用、修改或再发布时，请先查看并遵守[原项目页面](https://oshwhub.com/7anx/project_kfijeaht)标注的许可和使用要求。

STM32 HAL、CMSIS、FreeRTOS 和 FatFs 等第三方组件分别遵守各自源码中附带的许可证。本README不扩大原项目或第三方组件的授权范围。
