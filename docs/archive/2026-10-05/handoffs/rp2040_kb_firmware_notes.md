# FluxRail RP2040硬件与Arduino配置备注

提取日期：2026-10-02（北京时间）。用途：后续编写 RP2040 主程序的硬件依据；本文件不包含程序代码。

## 1. 信息来源与使用范围

- 嘉立创EDA当前项目：`new_keyboard`，项目 UUID：`e577febf1562499abe901c3e43e43a5b`。
- 主板：`rp2040_kb`；原理图 `Schematic2 / rp2040`，页面 UUID `468cebb0039e2200`；PCB `PCB2`，UUID `c166ea56c4c02097`。
- 旋钮小板：同项目 `Board2`；原理图 `Schematic3 / P1`，页面 UUID `4b8ddcb2d29a4d41`，包含 U4、U5 两颗 MT6701。对应 `PCB3` 当前源码中无器件，小板信息以原理图为依据。
- 主板原理图和 PCB 网表均有 69 个器件；逐器件、逐引脚比较后，只有 SPI 片选网络名称大小写不同：原理图为 `SPI1 CSN` / `SPI0 CSN`，PCB 为 `SPI1 CSn` / `SPI0 CSn`。关联引脚相同。
- `Board1` 使用 RP2040-Zero，属于另一套设计，其 GPIO 映射不用于本文件。
- 下文“已提取”指实际网表；“推断”“建议”“待确认”不代表已经从硬件验证。网表检查不等于实板通断、焊接或 DRC 检查。

软件需求统一见[用户版主需求](FluxRail_需求总览_用户版.md)；各窗口按[开发交接入口](FluxRail_开发交接_窗口必读.md)阅读。本文主要维护网表/PCB和Arduino配置事实，不另存完整产品需求。软件文档版本v0.4，硬件提取日期与下述事实保持原记录。

**当前软件状态（2026-10-03阶段10）：** 模块1与既有公共USB/配置/IO4已实现并编译/模拟，见[模块1进度](../progress/模块1.md)。本文提取时的“尚未实现/编译”等语句是历史阶段记录，实板仍未验证；滑轨实现以主需求的绝对语义为准，第5节早期相对移动建议不作为实施口径。板型/GPIO/Flash事实未改。

## 2. 关键芯片与器件

| 板子 / 位号 | 提取到的型号 | 数量 | 用途 / 备注 |
| --- | --- | ---: | --- |
| 主板 U2 | RP2040，C2040 | 1 | 主控，LQFN-56 加接地焊盘；程序使用 GPIO 编号，不使用封装脚号 |
| 主板 U1 | W25Q32JVSSIQ，C179173 | 1 | 外置 QSPI Flash，3.3V 供电 |
| 主板 U3 | SGM2028-3.3YN5G/TR，C81088 | 1 | 5V 转 3.3V LDO，输出网络 `VCC` |
| 主板 X1 | X322512MSB4SI | 1 | 原理图标称 12MHz 晶振 |
| 主板 H1、H2、H3 | SLSS49E-3，C2904393 | 3 | 模拟霍尔，SOT-23-3；应按此具体型号核对参数，不套用其他 SS49E 封装脚序 |
| 主板 BT1 至 BT10 | 凯华轴座；封装“凯华轴座+3528反贴灯” | 10 | 自定义轴座与灯组合；网表没有独立列出这 10 颗灯的确切料号 |
| 主板 LED1 | XL-5050RGBC-2812B，C2843785 | 1 | 独立 RGB 测试灯，GPIO11；独立于 10 颗按键灯 |
| 小板 U4、U5 | MT6701CT-STD-R，C3003196 | 2 | SOP-8 磁角度传感器；不是机械 A/B 编码器 |
| 主板 FPC1、FPC2 | LAIL-FPC-CX01-6P1.0-W | 2 | 旋钮接口，6 个信号触点，另外 7、8 脚接地 |
| 小板 CN1、CN2 | 自定义器件 MT6701-1 | 2 | 小板 6 脚接口，脚序与主板 FPC 不同，见第 6 节 |
| 主板 USB1 | LAIL-TYPE-C-16PIN-08-WT | 1 | USB Type-C 接口 |
| 主板 D3 | USBLC6-2SC6 | 1 | USB 数据线 ESD 保护 |
| 主板 F1 | nSMD100-16V | 1 | VBUS 与 +5V 之间的自恢复保险丝 |
| 主板 SW1、SW2 | PTS815SJK250SMTRLFS | 2 | RUN 复位与 QSPI_SS 启动按键 |
| 主板 H4 | LAIL-PZ2.54-4P-L | 1 | SWD 调试排针；H4 不是第四路霍尔 |

W25Q32JVSSIQ 容量为 32Mbit，即 **4MiB / 4,194,304 字节**，后续固件 Flash 容量配置需匹配。[Winbond 官方规格](https://www.winbond.com/hq/product/code-storage-flash/qspi-nor/w25q-jv/?__locale=en&partNo=W25Q32JVSSIQ)

## 3. 应用引脚总表（以 RP2040 芯片物理脚号为主）

已按用户提供的原理图截图及重新导出的 U2 网表复核。

**三个 49E 的输出接 RP2040 芯片第 38、39、40 脚：H1 → 38 脚，H2 → 39 脚，H3 → 40 脚。**这是用户所指的引脚编号，以下表格优先列出这套物理脚号。

同一个芯片脚还有 GPIO 功能名称：38 脚 = GPIO26 / ADC0，39 脚 = GPIO27 / ADC1，40 脚 = GPIO28 / ADC2。后续调用按 GPIO 编号寻址的固件接口时，三路编号分别为 26、27、28；硬件接线查阅使用 38、39、40。

截图中的灯链标签为 `LED`，接口导出的网络名为 `LED_1`；两者在这里都对应 GPIO10 / U2 第 13 脚。以下保留导出网络名用于复核。

方向均相对于 RP2040；“输入上拉”是依据按键接地拓扑提出的初始化建议。

| 功能 | 原理图网络 | RP2040 芯片物理脚号 | GPIO 功能名称（供程序使用） | 连接终点 | 程序用途 / 方向 |
| --- | --- | ---: | --- | --- | --- |
| 按键 1 | BT1 | 2 | GPIO0 | BT1-3 | 数字输入上拉，低电平按下 |
| 按键 2 | BT2 | 3 | GPIO1 | BT2-3 | 数字输入上拉，低电平按下 |
| 按键 3 | BT3 | 4 | GPIO2 | BT3-3 | 数字输入上拉，低电平按下 |
| 按键 4 | BT4 | 5 | GPIO3 | BT4-3 | 数字输入上拉，低电平按下 |
| 按键 5 | BT5 | 6 | GPIO4 | BT5-3 | 数字输入上拉，低电平按下 |
| 按键 6 | BT6 | 7 | GPIO5 | BT6-3 | 数字输入上拉，低电平按下 |
| 按键 7 | BT7 | 8 | GPIO6 | BT7-3 | 数字输入上拉，低电平按下 |
| 按键 8 | BT8 | 9 | GPIO7 | BT8-3 | 数字输入上拉，低电平按下 |
| 按键 9 | BT9 | 11 | GPIO8 | BT9-3 | 数字输入上拉，低电平按下 |
| 按键 10 | BT10 | 12 | GPIO9 | BT10-3 | 数字输入上拉，低电平按下 |
| 按键 RGB 灯链 | LED_1 | 13 | GPIO10 | BT9-6 | WS2812B 类串行数据输出 |
| 独立 RGB 测试灯 | LED_TEST | 14 | GPIO11 | LED1-4 / DIN | 独立串行数据输出 |
| 旋钮 1 数据 | SPI1 RX | 15 | GPIO12 | FPC1-3 | SPI1 RX / SSI DO 输入 |
| 旋钮 1 片选 | SPI1 CSN | 16 | GPIO13 | FPC1-5 | SSI CSN 输出，低有效 |
| 旋钮 1 时钟 | SPI1 SCK | 17 | GPIO14 | FPC1-4 | SPI1 SCK / SSI CLK 输出 |
| 旋钮 2 数据 | SPI0 RX | 27 | GPIO16 | FPC2-3 | SPI0 RX / SSI DO 输入 |
| 旋钮 2 片选 | SPI0 CSN | 28 | GPIO17 | FPC2-5 | SSI CSN 输出，低有效 |
| 旋钮 2 时钟 | SPI0 SCK | 29 | GPIO18 | FPC2-4 | SPI0 SCK / SSI CLK 输出 |
| 旋钮 1 按压 | PUSH1 | 30 | GPIO19 | FPC1-2 | 数字输入；不是 SPI0 TX |
| 旋钮 2 按压 | PUSH2 | 31 | GPIO20 | FPC2-2 | 数字输入 |
| 霍尔 1 | H1 | 38 | GPIO26 | H1-3 / VOUT | ADC0 模拟输入 |
| 霍尔 2 | H2 | 39 | GPIO27 | H2-3 / VOUT | ADC1 模拟输入 |
| 霍尔 3 | H3 | 40 | GPIO28 | H3-3 / VOUT | ADC2 模拟输入 |

未接网络的 GPIO：GPIO15（U2-18）、GPIO21（U2-32）、GPIO22（U2-34）、GPIO23（U2-35）、GPIO24（U2-36）、GPIO25（U2-37）、GPIO29/ADC3（U2-41）。这是芯片引脚空闲，不能据此认为 PCB 上有可直接使用的排针或测试点。

## 4. 10 个按键与 LED 灯链

### 按键接法

10 个按键各占一个 GPIO，未采用行列矩阵。组合器件的 3 脚为 `BTn` 信号，4、5 脚均接 GND；1 脚接 +5V，2、6 脚为灯的数据网络。按键信号网络中没有提取到外置上拉电阻；后续使用 GPIO 内部上拉，并做按下、松开消抖。

### 按键灯的实际网络

| 组合器件 | 6 脚网络 | 2 脚网络 |
| --- | --- | --- |
| BT9 | LED_1（直接连 GPIO10） | LED9_OUT |
| BT8 | LED9_OUT | LED8_OUT |
| BT7 | LED8_OUT | LED7_OUT |
| BT6 | LED7_OUT | LED6_OUT |
| BT5 | LED6_OUT | LED5_OUT |
| BT4 | LED5_OUT | LED4_OUT |
| BT3 | LED4_OUT | LED3_OUT |
| BT2 | LED3_OUT | LED2_OUT |
| BT1 | LED2_OUT | LED1_OUT |
| BT10 | LED1_OUT | 未连接 |

**灯链顺序（用户2026-10-03确认）：**原PCB推断与用户确认一致，顺序为：

`GPIO10 → BT9 → BT8 → BT7 → BT6 → BT5 → BT4 → BT3 → BT2 → BT1 → BT10`

因此灯效数组索引 0 至 9 对应 `BT9、BT8、BT7、BT6、BT5、BT4、BT3、BT2、BT1、BT10`，不对应 BT1 至 BT10 的自然顺序。**灯链顺序已由用户确认；这不代表已完成实板点灯。RGB/GRB、具体灯料号及电气表现仍需实际验证。**表中的网络连接本身已由原理图和 PCB 双方确认。

按键灯供电为 +5V，有 C19、C20、C21、C23 至 C29 共 10 个 100nF 去耦电容。主板网表显示 GPIO10 直接接首灯数据网络，没有提取到该路径上的串联电阻或电平转换芯片。需要根据实际灯珠的输入高电平规格确认 3.3V 数据与 5V 供电是否匹配。

### 独立测试灯 LED1

LED1-1 / VDD = +5V；LED1-2 / DOU 未连接；LED1-3 / GND = GND；LED1-4 / DIN = GPIO11 / LED_TEST。

它是另一路单灯，不是10灯链的第11颗。当前软件用途为模式指示灯，暂定实际25%亮度；颜色/优先规则见[模块1](../spec/02_模块1_键盘与灯光.md)。

## 5. 三路霍尔（SS49E 类）

| 位号 | 具体型号 | 1 脚 VDD | 2 脚 GND | 3 脚 VOUT | RP2040 芯片物理脚号 | GPIO / ADC 功能 |
| --- | --- | --- | --- | --- | ---: | --- |
| H1 | SLSS49E-3 | VCC / 3.3V | GND | H1 | 38 | GPIO26 / ADC0 |
| H2 | SLSS49E-3 | VCC / 3.3V | GND | H2 | 39 | GPIO27 / ADC1 |
| H3 | SLSS49E-3 | VCC / 3.3V | GND | H3 | 40 | GPIO28 / ADC2 |

三路 VOUT 均直接连 ADC，引脚路径中未提取到串联电阻或单独的输出 RC 滤波。霍尔区域有 C11、C14、C15 三个 100nF 电源去耦。RP2040 的 ADC_AVDD 经 R10 = 200Ω 从 VCC 供电，C35 = 2.2uF、C36 = 100nF 对地滤波；这是 ADC 电源滤波。

程序备注建议：使用 ADC0、ADC1、ADC2 采样，关闭对应 GPIO 数字输入和上下拉；记录每路静止基线、活动范围、磁极导致的正负方向，增加平滑和中心死区，再转换为鼠标 X 轴相对移动。RP2040 的 GPIO26 至 GPIO29 可用作 ADC 输入，参见[官方 RP2040 数据手册](https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf)。

### PCB 位置与加权重心法参数

2026-10-02 从 `rp2040_kb` 的 PCB2（UUID：`c166ea56c4c02097`）直接读取 H1/H2/H3 元件放置坐标，并核对其 VOUT 网络。以下是 **PCB 封装放置原点**，不是位号文字的位置，也不是从原理图排列推断的位置；芯片内部磁敏感区相对封装原点的偏移未核实。

三颗器件均在 **顶层（Top）**，放置角度均为 **-90°**，使用相同 SOT-23-3 封装。API 坐标单位为 mil，换算关系为 `1 mil = 0.0254 mm`。下表保留 API 原始坐标系，绝对坐标不代表到板边的距离。

| 位号 | GPIO / ADC | 原始 X（mil） | 原始 Y（mil） | X（mm） | Y（mm） |
| --- | --- | ---: | ---: | ---: | ---: |
| H1 | GPIO26 / ADC0 | 3005.984252 | -1445.748031 | 76.352000 | -36.722000 |
| H2 | GPIO27 / ADC1 | 4334.743587 | -1445.738896 | 110.102487 | -36.721768 |
| H3 | GPIO28 / ADC2 | 5663.483744 | -1445.728585 | 143.852487 | -36.721506 |

三颗沿 PCB X 方向按 **H1 → H2 → H3** 排列，H2 在中间，几乎共线。H1 到 H3 的 Y 差仅约 **0.000494 mm**，可按一维排列处理。元件原点之间的距离为：

| 两颗传感器 | 距离（mm） |
| --- | ---: |
| H1 ↔ H2 | 33.750487 |
| H2 ↔ H3 | 33.750000 |
| H1 ↔ H3 | 67.500487 |

后续软件建议以 **H2 为位置零点，H1 指向 H3 为正方向**，使用以下局部坐标。这里的零点是算法参考点，磁铁实际静止位置仍需实测标定。

| 采样通道 | 位置 X（mm，精确到 6 位小数） | 相对 Y（mm） | 一维实用位置 X（mm） |
| --- | ---: | ---: | ---: |
| H1 / GPIO26 | -33.750487 | -0.000232 | **-33.75** |
| H2 / GPIO27 | 0.000000 | 0.000000 | **0.00** |
| H3 / GPIO28 | +33.750000 | +0.000262 | **+33.75** |

用户所说“重力法”暂按 **加权重心法**记录：`x估计 = (w1×x1 + w2×x2 + w3×x3) / (w1+w2+w3)`。位置参数按上表与 GPIO26/27/28 一一对应，不能只按 ADC 读数大小重新排列通道。

- `w1/w2/w3` 应来自各路校准后的有效磁场响应，不能直接把带静止偏置的原始 ADC 值作为权重。每路需记录无磁铁/参考状态基线、增益差异和实际磁极方向，再确定非负权重的计算方式。
- 总权重低于有效阈值、传感器饱和或采样异常时，不能继续按该式除法输出鼠标位移；阈值和处理行为留作软件配置。
- 该公式先得到一维位置估计，实际位置与磁场响应的关系需用磁铁移动实测校准；鼠标相对移动再由位置变化、死区和灵敏度得到。传感器共线布局用于这里的左右位置，不把 H1/H2/H3 分别定义为 X/Y/Z。
- 组装后若 H1 → H3 与希望的鼠标向右方向相反，反转软件方向配置；PCB 坐标不能单独确定整机朝向。磁铁高度、轨迹、静止点和有效行程仍待实物确认。

## 6. 两个 MT6701 旋钮及小板接口

### 芯片连接（小板原理图已提取）

| MT6701 SOP-8 脚 | 符号名称 / SSI 用途 | U4（旋钮 1 / FPC1） | U5（旋钮 2 / FPC2） |
| ---: | --- | --- | --- |
| 1 | VDD | VCC / 3.3V | VCC / 3.3V |
| 2 | MODE | VCC / 高电平 | VCC / 高电平 |
| 3 | OUT / 模拟或 PWM 输出 | 未连接 | 未连接 |
| 4 | GND | GND | GND |
| 5 | PUSH | PUSH1 → GPIO19 | PUSH2 → GPIO20 |
| 6 | A / SSI DO | SPI1 RX → GPIO12 | SPI0 RX → GPIO16 |
| 7 | B / SSI CLK | SPI1 SCK → GPIO14 | SPI0 SCK → GPIO18 |
| 8 | Z / SSI CSN | SPI1 CSn → GPIO13 | SPI0 CSn → GPIO17 |

U4、U5 分别有 C37、C38 = 100nF 电源去耦。MODE 接 VCC 与厂商 SSI 参考图一致。A/B/Z 是复用脚的符号名称，这套接法按 **SSI 绝对角度读取**使用，不应按机械旋钮 A/B 边沿计数编写驱动。[MT6701 官方数据手册，图 22 及第 7.8 节](https://www.magntek.com.cn/upload/pdf/202407/MT6701_Rev.1.8.pdf)

### 主板到小板的脚序映射

**两端脚号不是一一对应。**FPC1 对 CN1，FPC2 对 CN2，均需按同名信号连接：

| 信号 | 主板 FPC1 / FPC2 脚号 | 小板 CN1 / CN2 脚号 | MT6701 脚号 |
| --- | ---: | ---: | ---: |
| VCC / 3.3V | 1 | 1 | 1（并连 MODE 2） |
| PUSH | 2 | 5 | 5 |
| RX / DO | 3 | 4 | 6 |
| SCK / CLK | 4 | 3 | 7 |
| CSN | 5 | 2 | 8 |
| GND | 6 | 6 | 4 |

主板 FPC 的 7、8 脚也接 GND。以上是逻辑信号映射；实际线缆同面/异面、连接器触点方向、转接板是否已经换序，必须结合实物确认。单纯把 1 至 6 同号直连无法得到这套信号对应关系。

### 后续驱动备注

- 硬件只连接 DO、CLK、CSN，没有 MOSI；SSI 读取无需发送寄存器读取命令。GPIO19 已用于 PUSH1，不能作为 SPI0 TX 输出初始化。
- 厂商 SSI 帧为 24 位：14 位角度、4 位磁场状态、6 位 CRC；时钟空闲高、数据在上升沿更新，主控在下降沿采样，CSN 低有效。若用硬件 SPI 实现，依据该时序核对配置和整帧片选。[MT6701 官方数据手册，第 7.8 节](https://www.magntek.com.cn/upload/pdf/202407/MT6701_Rev.1.8.pdf)
- PUSH 表示磁铁间距变化产生的按压检测，数据手册描述检测到按压时输出高电平；它不是接地机械开关，不应套用 10 个按键的低有效逻辑。[MT6701 官方数据手册，第 7.9 节](https://www.magntek.com.cn/upload/pdf/202407/MT6701_Rev.1.8.pdf)
- 软件应将绝对角度变成相邻采样的增量，并处理跨越 0°/360°时的回绕，再转换为鼠标相对 X 或 Y 位移。
- 用户要求一个旋钮控制左右、另一个控制上下，但尚未指定 U4/U5 各对应哪一轴；轴映射、方向反转、灵敏度均留作配置。

## 7. 启动、调试、USB 与供电

### 启动和 SWD

| 功能 | 已提取连接 |
| --- | --- |
| RUN / 复位 | U2-26 → RUN；R7 = 10kΩ 上拉到 VCC，C8 = 100nF 对地；SW1 的 1、2 脚接 RUN，3、4 脚接 GND |
| BOOT 按键路径 | SW2 的 1、2 脚经 R8 = 1kΩ 接 QSPI_SS；3、4 脚接 GND；不占用普通 GPIO |
| SWD 排针 H4 | 1 = VCC / 3.3V；2 = SWDIO → U2-25（符号写 SWD）；3 = SWCLK → U2-24；4 = GND |
| 晶振 | X1 = 12MHz；XIN → U2-20；XOUT → U2-21，经 R5 = 1kΩ 接晶振另一端；C6、C7 各 15pF 对地 |

### 外置 Flash 专用引脚

| 信号 | U2 / RP2040 封装脚 | U1 / W25Q32JVSSIQ 脚 |
| --- | ---: | ---: |
| QSPI_SS | 56 | 1 / CS# |
| QSPI_SD1 | 55 | 2 / DO(IO1) |
| QSPI_SD2 | 54 | 3 / WP#(IO2) |
| QSPI_SD0 | 53 | 5 / DI(IO0) |
| QSPI_SCLK | 52 | 6 / CLK |
| QSPI_SD3 | 51 | 7 / HOLD# 或 RESET#(IO3) |

U1-4 接 GND，U1-8 接 VCC；C1 = 100nF 去耦。QSPI 是启动/执行固件的专用接口，与旋钮 SPI0、SPI1 分开。

### USB 数据路径

- D+：USB1 A6/B6 → D3-1/6 → USB_DP → R3 = 27Ω → U2-47 / USB_DP。
- D-：USB1 A7/B7 → D3-3/4 → USB_DN → R4 = 27Ω → U2-46 / USB_DM。
- 原理图 U2 与串联电阻之间的自动网络名分别是 `$2N10`、`$2N11`。
- USB1 A5 / CC1 经 R2 = 5.1kΩ 接 GND；B5 / CC2 经 R1 = 5.1kΩ 接 GND；SBU1/SBU2 未连接；外壳接地。
- USB 使用 RP2040 专用 USB 引脚。后续可实现键盘和鼠标复合 HID；键值与 HID 报告定义属于软件设计。

### 电源与固定功能脚

- USB VBUS → F1 → +5V；+5V 给按键灯、测试灯和 U3 输入供电。
- U3：1 / IN = +5V，2 / GND = GND，3 / EN = +5V，4 / BP/FB 经 C3 = 10nF 对地，5 / OUT = VCC（3.3V）。
- VCC 给 RP2040 I/O、USB 电源、Flash、三路霍尔和两个旋钮接口供电。
- U2 IOVDD：1、10、22、33、42、49 脚均接 VCC；44 / VREG_IN、48 / USB_VDD 也接 VCC。
- U2 45 / VREG_VOUT 接 1.1V，供 23、50 / DVDD；C32 = 2.2uF，C33、C34 = 100nF 对地。
- U2 43 / ADC_AVDD 通过 R10 = 200Ω 从 VCC 获取滤波电源；19 / TESTEN 接 GND；57 / 底部接地焊盘接 GND。

## 8. 写主程序前保留的配置与确认项

| 项目 | 当前状态 |
| --- | --- |
| 按键 GPIO 与数量 | 已确认GPIO0至GPIO9，10路独立输入；BT8/7/6/5=ASDF，BT4/3/2/1=JKL;，BT9/10=VN；网页可更改 |
| 按键灯输出与数量 | GPIO10 已确认；数量 10 为用户目标，组合器件也是 10 个；具体灯珠型号、DIN/DOUT 脚号及颜色字节顺序待确认 |
| 灯效和按键灯对应 | 单灯测试确认链顺序后建立按键到灯索引映射；灯效类型、速度、亮度和默认效果待定义 |
| 测试灯 | GPIO11 / LED1已确认，独立模式指示灯，暂定实际亮度25%，不同模式不同颜色 |
| 霍尔采样与位置 | 芯片物理脚 38/39/40 → GPIO26/27/28、ADC0/1/2；PCB 位置已提取：以 H2 为零点，H1/H2/H3 ≈ -33.75/0/+33.75 mm；三路基线、权重、磁铁行程和整机左右方向待标定，见第 5 节 |
| 旋钮接口 | U4 = SPI1 / FPC1；U5 = SPI0 / FPC2；PUSH1 = GPIO19，PUSH2 = GPIO20 |
| 主板与小板连线 | 必须核对 FPC 与 CN 不同脚序及实际转接方式 |
| 鼠标轴行为 | 两个旋钮对应 X/Y、正反方向、每圈位移和加速曲线待定义 |
| 灯供电与数据电平 | +5V 供电、GPIO 3.3V 直驱路径已提取；适配性需按实际灯珠规格验证 |
| 固件基础配置 | 自定义 RP2040 板、12MHz 晶振、4MiB Flash；不要沿用 Board1 的 RP2040-Zero 引脚配置 |

建议的软件模块划分：按键扫描/消抖、三路霍尔采样/校准、两路 MT6701 SSI 读取/角度增量、USB 键盘与鼠标 HID、10 灯链灯效、独立测试灯、用户配置。模块划分是后续开发建议，原理图不能确定具体灯效或交互逻辑。

## 9. Arduino IDE 板卡包与编译配置

配置核对日期：2026-10-02。电脑中已找到 Earle Philhower 的 Arduino-Pico **6.1.0**；以下配置以该版本实际安装的 `boards.txt`、Generic RP2040 引脚定义和启动文件为依据。已核对配置，没有编写主程序，也尚未进行实板上传验证。

### 板卡包

使用 **Raspberry Pi Pico/RP2040/RP2350**，作者 **Earle F. Philhower, III**。本项目先固定已安装的 **6.1.0**，后续记录实际使用版本，保证复现。

需要在另一台电脑安装时，在 Arduino IDE 的“文件 → 首选项 → 附加开发板管理器网址”添加：

`https://github.com/earlephilhower/arduino-pico/releases/download/global/package_rp2040_index.json`

然后在开发板管理器中搜索 `pico`，安装上述作者的包，选择 `Generic RP2040`。[板卡包官方安装说明](https://arduino-pico.readthedocs.io/en/latest/install.html)

### 工具菜单推荐配置

| 菜单项 | 本项目选择 | 对应硬件 / 原因 |
| --- | --- | --- |
| Board / 开发板 | **Generic RP2040** | 自定义 RP2040 主板，可配置 Flash 容量及启动方式 |
| Flash Size | **4MB (no FS)** | W25Q32JVSSIQ = 32Mbit = 4MiB；当前不预留文件系统 |
| Boot Stage 2 | **W25Q64JV QSPI /4** | 6.1.0 中其启动字节与包内 W25Q32JVxQ /4 文件一致，见下方核对说明 |
| CPU Speed | **125 MHz** | 先使用标准运行频率；按键、ADC、SSI 和 10 灯无需先超频 |
| USB Stack | **Adafruit TinyUSB** | 2026-10-03修订：自定义NKRO全键无冲键盘+旋钮相对鼠标/滑轨绝对鼠标+配置CDC；IO4人格另做专项适配；不使用普通六键Keyboard报告 |
| Upload Method | **Default (UF2)** | 使用板上 USB 及 BOOT 按键上传 |
| Optimize | **Small (-Os) (standard)** | 首版使用标准优化 |
| Operating System | **None** | 建议Arduino双核setup/loop与setup1/loop1；不默认引入RTOS |
| Debug Port | **Disabled** | 关闭核心调试输出；用户程序仍可使用 USB Serial |
| Debug Level | **None** | 关闭核心调试日志 |
| Profiling | **Disabled** | 保持关闭 |
| RTTI | **Disabled** | 保持默认 |
| C++ Exceptions | **Disabled** | 保持默认 |
| Stack Protector | **Disabled** | 首版保持板卡包默认 |
| Port / 端口 | 实际出现的 RP2040 USB 串口 | 端口号由电脑分配，不能在备注中固定 |

菜单名称与配置选项已按已安装的 6.1.0 核对；通用板与 Flash 菜单用途参见[官方菜单说明](https://arduino-pico.readthedocs.io/en/latest/ide.html)。

**12MHz 是板上 X1 晶振的频率，125MHz 是 RP2040 通过 PLL 得到的 CPU 运行频率。**这两个值可以同时成立；Generic RP2040 的标准 12MHz 晶振配置适合本板，不需要把 CPU Speed 设成 12MHz，也不需要另加晶振编译参数。

### 为什么 W25Q32JV 选择名为 W25Q64JV 的 Boot Stage 2

6.1.0 的 Generic RP2040 菜单没有单独列出 W25Q32JV，但安装包内有它的专用启动文件。已比较以下两个文件中的全部 **256 字节**，内容完全相同（文件注释不同）：

- [包内 W25Q32JVxQ /4 启动文件](https://raw.githubusercontent.com/earlephilhower/arduino-pico/6.1.0/boot2/rp2040/boot2_W25Q32JVxQ_4_padded_checksum.S)
- [菜单使用的 W25Q64JV /4 启动文件](https://raw.githubusercontent.com/earlephilhower/arduino-pico/6.1.0/boot2/rp2040/boot2_w25q64jv_4_padded_checksum.S)

据此选择 `W25Q64JV QSPI /4`。Boot Stage 2 选择的是 Flash 访问/启动方式，容量由独立的 Flash Size 设置决定，**本板容量仍是 4MB**。在 125MHz CPU 下，`/4` 对应约 31.25MHz 的 Flash 时钟，不是 MT6701 的 SSI 时钟。

若实板出现“UF2 写入后程序不启动”，可用 BOOT 重新上传，并暂改 `Generic SPI /4` 做基础启动排查；它是通用单线访问选项。[官方 Boot Stage 2 说明](https://arduino-pico.readthedocs.io/en/latest/ide.html#boot-stage-2-options-for-generic-rp2040)

### USB 库选择与硬件引脚注意事项

2026-10-03根据用户确认的全键无冲要求，首版采用 **Adafruit TinyUSB**、自定义 **NKRO键盘、旋钮相对鼠标和滑轨绝对鼠标HID** 及普通模式 **配置CDC**。普通Keyboard的六键数组不满足本版要求；切换菜单后必须改用对应TinyUSB实现，不能混用Pico SDK的Keyboard/Mouse。建议核心0独占USB/CDC/配置，核心1采集按键/霍尔/旋钮，详见[公共双核约定](../spec/01_公共接口与集成约定.md)。当前只修订配置文档，未实现/编译/实板枚举。[官方USB文档](https://arduino-pico.readthedocs.io/en/latest/usb.html)

- Generic RP2040 使用 GPIO 编号：按键 `0～9`，灯链 `10`，测试灯 `11`，霍尔 `26/27/28`（也可用 A0/A1/A2）。
- 三路霍尔的 12 位读取精度需要在程序中设置；**它不是 IDE 编译菜单项**。
- Generic 板的 `LED_BUILTIN` 默认指 GPIO25，本板 GPIO25 空闲。板上测试灯实际为 GPIO11 的 WS2812B 类灯，不能靠普通 Blink 示例和 `LED_BUILTIN` 验证它。
- 两路 SPI 的 RX/SCK/CS 默认 GPIO 与本板吻合，但 SPI0 默认 TX 是 GPIO19，已被本板用作 PUSH1。后续驱动必须禁用或避开 SPI0 TX，使用无 MOSI 的 SSI 接法；这也不是 IDE 菜单可以替代的初始化工作。
- Generic 板默认 UART、I2C 引脚与本板部分按键/霍尔复用；本项目不应无目的启用 Serial1/Serial2 或 Wire/Wire1。USB Serial 不占这些 GPIO。

若以后确实要保存 LittleFS 配置文件，可改为 `4MB (Sketch: 2MB, FS: 2MB)` 并记录分区变更；当前先使用 `4MB (no FS)`。灯效类型、霍尔校准、按键映射及鼠标灵敏度属于程序配置，不是板卡编译选项。

### 首次 USB 上传

按住 **SW2 / BOOT**，插入 USB，再松开，进入 RP2040 ROM 的 UF2 下载模式；应看到 `RPI-RP2` 盘。也可保持 USB 连接，按住 SW2，再按下并松开 SW1 / RUN 复位，最后松开 SW2。之后通过 Arduino 上传，或将导出的 UF2 放入该盘。首次进入 ROM 下载模式时可能没有普通 COM 串口；端口需在运行带 USB Serial 的固件后按实际枚举结果选择。[官方首次上传说明](https://arduino-pico.readthedocs.io/en/latest/install.html#uploading-sketches)

## 10. 软件文档入口

产品口径：[用户版主需求](FluxRail_需求总览_用户版.md)；窗口分工：[开发交接入口](FluxRail_开发交接_窗口必读.md)；实现契约：[公共接口](../spec/01_公共接口与集成约定.md)；工程组织：[模块拆分](../spec/11_Arduino模块拆分与四窗口交接.md)。

USB两身份的启动/描述符限制见[IO4与冷切换审核](../spec/09_IO4兼容与USB冷切换.md)。本机安装库未修改，适配未实现、双身份未编译/实板验证。
