# camera_viewer Linux GUI

`camera_viewer` 是一个基于 Qt Widgets 的 XDMA C2H 视频接收与显示工具。
当前版本按同目录 `Camera_PC` 的工程形态迁移为 Linux 版本，并保留原有接收、解包、组帧、AXI-Lite 读写逻辑。

## 项目结构

```text
camera_viewer/
├── CMakeLists.txt
├── README.md
├── load_xdma.sh
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

## 功能概览

1. 打开 XDMA 并执行 `ready_state` 自检（`user + c2h_0`）。
2. 后台持续读取 C2H 字节流，支持短读与异常退避重试。
3. 按 1024B 协议包解包（支持 `length=payload长度` 与 `length=整包长度(0x0400)` 两种语义）。
4. 按固定帧长重组 YUY2 帧并实时预览。
5. 支持 AXI-Lite 寄存器读写（通过 `user` 通道）。
6. 支持协议自测（不依赖硬件）。
7. 运行时可落盘采样数据（原始接收流与解包流）。

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

1. 加载 XDMA 驱动（脚本会自动处理 sudo/root）：

```bash
./load_xdma.sh
```

2. 启动 GUI：

```bash
./build/camera_viewer_gui
```

## 设备检查

```bash
ls /dev/xdma*
```

## 运行期输出文件

运行接收后，会在项目根目录生成以下文件（带时间戳）：

- `c2h_rx_raw_*.bin`：`read_device` 直接读到的原始字节流
- `c2h_rx_unpacked_*.bin`：协议解包后的连续视频字节流
- `c2h_dump_*_640x360_yuyv422.yuv`：落盘的 YUYV422 帧数据（如启用）

## 常用寄存器说明

- `0x15020`：丢包计数（读）
- `0x15008`：丢包计数清零（写 `1`）
- `0x15018`：接收计数（读）
- `0x1500C`：DMA 长度配置（写）
