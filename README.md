# camera_viewer Linux GUI

`camera_viewer` 是一个基于 Qt Widgets 的 XDMA C2H 视频接收与显示工具。
当前版本已按同目录 `Camera_PC` 的工程形态迁移为 Linux 版本，并保持原有接收/解包/组帧/AXI-Lite 读写逻辑不变。

## 项目结构

```
camera_viewer/
├── CMakeLists.txt
├── README.md
├── include/
│   ├── stream_pipeline.h
│   ├── widget.h
│   └── xdmaDLL_public_linux.h
└── src/
    ├── XDMA_MoreB_linux.cc
    ├── main.cpp
    ├── stream_pipeline.cpp
    ├── widget.cpp
    └── widget.ui
```

## 构建依赖（Ubuntu）

```bash
sudo apt update
sudo apt install -y \
  build-essential cmake pkg-config \
  qtbase5-dev qtmultimedia5-dev qttools5-dev-tools \
  libqt5multimedia5-plugins libqt5multimediawidgets5
```

## 编译

```bash
cmake -S . -B build
cmake --build build -j
```

## 运行

```bash
./build/camera_viewer_gui
```

## 功能说明

1. 打开 XDMA 并执行 `ready_state` 自检（`user + c2h_0`）。
2. 后台持续读取 C2H 字节流。
3. 按 1024B 协议包解包（去除 padding）。
4. 按固定帧长重组 YUY2 帧并实时预览。
5. 原始 YUYV422 数据落盘。
6. 支持协议自测（不依赖硬件）。
7. 支持 AXI-Lite 寄存器读写（通过 user 通道）。

## XDMA 设备检查

```bash
ls /dev/xdma*
```
