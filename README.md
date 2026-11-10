# 电子设计协会软件培训第二题

**项目状态 · 全部内容已完成。** 本仓库包含题目全部基本功能和六项拓展功能的固件实现。项目基于 KF56-10 冠军开发板、STM32F407ZGT6 和板载 ILI9341 触摸屏，使用 STM32CubeMX、HAL、VS Code、CMake 和 Arm GNU Toolchain。界面由板载 LCD 驱动绘制。

原始题目见 [2026创新基地班软件考核-第二题（Word）](docs/2026创新基地班软件考核-第二题.docx)。

## 功能菜单

上电进入 `FUNCTION MENU`。如果屏幕显示四点校准提示，按顺序点完后进入菜单。触摸选项打开对应功能，按 **KEY0** 返回菜单。

| 菜单项 | 功能 |
| --- | --- |
| **PWM Output** | A 路 PWM 输出。用数字键盘设定频率和占空比，或拖动滑块调占空比。点 `STD` / `EXT` 切换量程。标准模式上限 **200 kHz**，占空比每 **1%** 一档。扩展模式上限 **500 kHz**，占空比每 **5%** 一档。两档均从 **1 Hz** 起，支持 **0%～100%**。 |
| **Stopwatch** | 毫秒秒表，支持开始、暂停、继续、清零。点 `LAP` 记录累计时间 `TOTAL` 和本段用时 `SPLIT`，显示最近四圈。 |
| **Input Capture** | 测量 PA0 输入方波的频率和占空比，随标准或扩展量程显示读数，并提示无信号或超量程。 |
| **Dual PWM Phase** | A、B 两路可独立设定频率和占空比。将两路设为同频后开启同步，按 **5°** 一档调节 B 路相对 A 路的相位，范围 **0°～360°**。 |
| **Auto Sweep** | 将 A 路从 **10 Hz** 扫至 **200 kHz**，共 **10 个频点**。显示设定频率、实测频率、误差及 `PASS` / `FAIL`，完成后恢复 A 路原设定。 |
| **SPWM Output** | PA3 输出 **20 kHz** 载波的 SPWM。外接低通滤波器后，在滤波器输出端观察 **50 Hz** 正弦波。 |
| **Snake** | 触摸方向键操控，支持计分、重开和自动模式。自动模式沿闭环逐步吃满棋盘。 |

数字键盘用 `Current` 显示当前设定，用 `New` 显示正在编辑的值，按 `OK` 后提交。

## 硬件接口

| 功能 | 引脚 | 外设 |
| --- | --- | --- |
| PWM A | PA11 | TIM1_CH4 |
| PWM B | PC7 | TIM8_CH2 |
| 方波输入捕获 | PA0 | TIM5_CH1，CH2 捕获同一输入的下降沿 |
| SPWM 输出 | PA3 | TIM2_CH4 |
| 返回菜单 | PE4 | KEY0，低电平有效 |
| 秒表时基 | — | TIM6，每 1 ms 更新 |

测外部方波时，信号发生器和开发板共地，PA0 输入电平保持在 **0～3.3 V**。运行自动扫频前，关闭双路同步，并用杜邦线连接 **PA11 → PA0**。验收实际输出和测量精度时，分别使用示波器和外部信号发生器。

## 构建与烧录

在 VS Code 中打开项目根目录，通过 STM32Cube 扩展或 CMake Tools 选择 `Debug` 预设并构建。命令行环境中，确保 CMake、Ninja 和 `arm-none-eabi-gcc` 已加入 `PATH`，然后执行：

```sh
cmake --preset Debug
cmake --build --preset Debug
```

构建产物为 `build/Debug/Second_Task.elf`。通过 ST-LINK 烧录后，可在同一次上电中从菜单依次演示各项功能。

## 代码结构

| 路径 | 职责 |
| --- | --- |
| [App/app.c](App/app.c) | 启动应用、切换页面、分发触摸和定时器回调、推进后台任务 |
| [App/menu.c](App/menu.c) / [App/ui.c](App/ui.c) | 绘制菜单和各功能页、识别控件触摸 |
| [App/pwm.c](App/pwm.c) / [App/capture.c](App/capture.c) | 双路 PWM 与相位控制、TIM5 输入捕获 |
| [App/stopwatch.c](App/stopwatch.c) / [App/sweep.c](App/sweep.c) / [App/spwm.c](App/spwm.c) / [App/snake.c](App/snake.c) | 秒表、扫频、SPWM 和贪吃蛇 |
| [App/touch_input.c](App/touch_input.c) / [Screen/](Screen/) | 触摸事件、LCD 驱动及触摸校准 |
| [Core/](Core/) / [Second_Task.ioc](Second_Task.ioc) | CubeMX 生成的启动和外设初始化代码及配置 |

现场要修改 PWM 频率上限、占空比上限或步进、相位步进时，可在 [App/app_config.h](App/app_config.h) 中调整对应常量。
