# xdma_c2h_viewer

基于 Qt Widgets 的项目，用于从 XDMA c2h_0 读取视频帧并实时显示。

## 功能特点

- 打开 XDMA 并运行 ready_state 自检
- 在工作线程中从 c2h_0 读取数据
- 通过分块读取重新组装一帧（分块大小可在 UI 中调整）
- 可在 UI 中调整接收循环的节流间隔
- 将接收到的 YUYV 数据转换为 RGB 并实时显示

## 运行环境假设

- FPGA 发送打包的 YUYV 帧
- 每帧大小为 宽 × 高 × 2 字节
- UI 中的宽度/高度设置必须与 FPGA 输出匹配

## 构建说明

- 使用 qmake 项目文件：xdma_c2h_viewer.pro
- 链接本地驱动文件（位于 ./driver 目录）：
  - XDMA_MoreB.lib
  - XDMA_MoreB.dll
- 如果命令行 qmake 不可用，请在 Qt Creator 中打开 .pro 文件，使用与 camera_PC 相同的套件（Qt 5.12.10 MSVC2017 64bit）

## 快速开始

1. 打开 XDMA + 自检
2. 设置宽度、高度、节流间隔、分块大小
3. 点击"开始 C2H 接收"
4. 如果数据流停止或 FPGA 暂停发送数据，点击"停止"然后再点击"开始"