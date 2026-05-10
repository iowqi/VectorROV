# VectorROV

[![License: MIT](https://img.shields.io/badge/License-MIT-blue.svg)](LICENSE)

**语言:** [English](README.md) · [中文](README.zh-CN.md)

面向 **VR 遥操作** 与 **全景视觉** 的开源 **有缆水下 ROV** 平台：以 **四矢量推进** 为核心，结合 **双鱼眼拼接 + NDI 图传**、嵌入式 **STM32H723 + 双路 FDCAN** 电控，以及 **MuJoCo + Stable-Baselines3** 的仿真与强化学习实验链路。本仓库对应 **毕业设计** 的工程实现（福建理工大学 · 计算机科学与技术）。

设计目标强调 **机械—电控—视觉—仿真** 的 **可运行闭环** 与 **可复现流程**；平台预留 **可扩展双臂（6DOF×2）** 能力，详见 [TODO](#todo)。

---

## 🤖 原型实物图

<p align="center">
  <img src="assets/img/rov_physical_prototype.jpg" alt="VectorROV 实物样机" width="720"/>
</p>

<p align="center"><em>样机：四涵道推进器、水密主舱体与透明舱盖、滑橇式展台架，以及水密航插与缆线接口等。</em></p>

---

## ✨ 主要工作

- **🧭 四矢量推进** — 提升力与力矩的可表达范围；推进舱采用 **磁驱快拆式水密结构**，便于维护（详见论文机械章节）。
- **⚙️ STM32H723 + CAN** — **FDCAN1 / FDCAN2** 分流：舵向（DM3510 类）与推进（**VESC / FOC**）；**IMU + 气压/水压** 反馈支撑姿态与深度估计。
- **👁️ 全景 → NDI → VR** — 在 **瑞莎 Dragon Q6A** 类边缘端完成 **双鱼眼同步与全景拼接**，经 **NDI** 推出，浮标 **Wi‑Fi** 送达头显；接入 **轻量化 YOLO** 检测，并采用「**边缘优先推理、显示端可回退**」策略。
- **🎮 MuJoCo + SB3** — 搭建仿真环境，基于 **Stable-Baselines3** 开展强化学习实验，在状态/动作定义上与实机控制保持可对齐接口。
- **🧰 CAD 与桨叶工具链** — `hardware/` 存放结构相关源文件；`tools/OpenProp/` 提供桨叶/推进分析参考流程。

### 联调侧指标（论文阶段性结果）

全景链路有效帧率约 **20 fps**；「采集—拼接—NDI」段时延约 **&lt;100 ms**（不含头显侧解码渲染的完整闭环）；单推进器推力约 **8–10 N**（估计区间）。

---

## 🏗️ 架构示意（论文插图）

| 系统总体框图（§2.1） | 电控与通信子框图（§2.2） |
|:-:|:-:|
| ![系统总体](assets/diagrams/2-1.svg) | ![电控子系统](assets/diagrams/2-3.svg) |

| 视觉拼接与图传（§2.2） | 姿态闭环与执行器分配（图4-5） |
|:-:|:-:|
| ![视觉图传](assets/diagrams/2-4.svg) | ![姿态控制](assets/diagrams/4-5.svg) |

---

## 🗂️ 仓库结构说明

| 路径 | 说明 |
|------|------|
| `assets/diagrams/` | 论文配套框图（SVG） |
| `assets/img/` | 实物照片、渲染图等 |
| `firmware/` | `CtrBoard-H7_CAN/`（H7 主控）、`F103C8T6/`、`N630/` 等 |
| `hardware/` | 结构/零配件 CAD 与相关资源 |
| `control/MuJoCoSim/` | MuJoCo 场景、SB3 训练脚本、`models` / `logs` 等 |
| `pytest/` | Python 侧测试（可能含局部 `.venv`） |
| `tools/OpenProp/` | OpenProp 示例与源码 |

---

## 🚀 快速上手（指引）

**固件：**使用 **Keil MDK** 打开 `firmware/CtrBoard-H7_CAN/MDK-ARM` 下工程；`Drivers/` 等目录为厂商栈，首次同步勿被体量吓到。

**仿真 / 强化学习：**进入 `control/MuJoCoSim/`，在干净虚拟环境中安装 MuJoCo、Gymnasium、Stable-Baselines3 等依赖后，按 `train/` 等目录内入口脚本运行。

**视觉 / 边缘端：**拼接、推理与 NDI 主推流在 **Q6A** 类设备上运行；头显侧需 NDI/流媒体接收与渲染。**本仓库侧重电控、结构、仿真与系统说明**，边缘镜像与 VR 客户端可能分开发布。

---

## 🎓 毕业设计说明

课题：**《融合视觉感知的水下无人机 VR 平台的设计与实现》**  
侧重 **系统工程与全链路打通**（机械、电控、视觉图传、仿真验证），而非单一算法极限指标。完整公式推导、实验细节与讨论以 **毕业论文正文** 为准（正文不一定随仓库分发）。

---

## TODO

尚不完善或 **尚未开始** 的工作：

- [ ] **强化学习** — 深化 SB3/MuJoCo 训练与评估、奖励与安全约束、日志/回放，并向实机迁移做防护与标定。
- [ ] **双臂（6DOF×2）作业能力** — 水密走线、低层驱动与上层遥操作、人机工程；`hardware/` 有相关设计素材，**整机臂系统闭环未完备**。
- [ ] **部署于 Meta Quest 2 的 VR 客户端** — 面向 Quest 2 的低时延显示与交互、与 NDI/网络链路的工程化对接；**客户端侧基本未形成可交付实现**。

欢迎通过 Issue / PR 参与。

---

## 📄 许可证

本项目采用 **MIT License**，见 [`LICENSE`](LICENSE)。

---

## 🙏 致谢

感谢指导教师与同学在加工、联调与实验上的支持。`firmware/` 内的 STM32 HAL、FreeRTOS、USB 协议栈等仍遵循各自原项目许可证。
