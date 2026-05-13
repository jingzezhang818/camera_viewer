# camera_viewer

`camera_viewer` 是一个基于 Qt Widgets 的 XDMA C2H 视频接收与联调工具，包含实时预览、抓包落盘、协议解包、固定帧重组，以及 `user` 通道 AXI-Lite 寄存器读写。

## 功能概览

- 打开 XDMA 并执行 `ready_state` 自检（`user + c2h_0`）。
- 持续接收 C2H 字节流，支持短读场景下保持监听。
- 按 1024B 协议包解包，恢复真实视频 payload。
- 按固定帧长重组 YUY2 帧并实时预览。
- 同步落盘三类调试数据：
  - 预览帧原始数据：`c2h_dump_*.yuv`
  - 原始 C2H 流：`c2h_raw_*.bin`
  - 解包后连续流：`c2h_raw_*_depacketized.bin`
- 内置协议自测（无需 XDMA 硬件）。
- AXI-Lite 寄存器读写（通过 `user` 通道，支持十六进制/十进制输入）。

## 协议与默认参数

### 协议格式（与发送端对齐）

- 包长：`1024B`
- 头长：`18B`
- 最大 payload：`1006B`
- 包格式：

```text
EB 90 | lengthH | lengthL | dest(6) | source(6) | priority(2) | payload(1006)
```

解析规则：
- `length` 为包总长度（包头 + 有效 payload），合法范围 `18..1024`。
- 仅提取有效 payload，自动忽略 padding 0。
- sync 错位或非法长度时自动重同步。

### 默认视频参数

- 分辨率：`640x360`
- 像素格式：`YUY2`（YUYV 4:2:2）
- `frameBytes = 640 * 360 * 2 = 460800`

## 程序流程

1. 打开设备：枚举并打开 `user` 与 `c2h_0`，执行 `ready_state`。
2. 接收线程：循环 `read_device(c2h_0)` 读取任意分段字节。
3. 协议处理：`StreamDepacketizer` 解包恢复真实视频字节流。
4. 组帧：`Yuy2FrameReassembler` 按固定 `frameBytes` 输出完整帧。
5. UI处理：预览显示、日志输出、调试文件落盘。

链路：

```text
read_device(c2h_0)
  -> StreamDepacketizer
  -> Yuy2FrameReassembler
  -> onReaderFrameReady()
  -> 预览 / dump
```

## UI 使用说明

### 顶部按钮

- `打开 XDMA 并自检`
- `运行协议自测`
- `开始接收 C2H`
- `停止`

### 参数区

- `宽/高`：当前接收链路按固定参数组帧。
- `节流时间(ms)`：控制帧输出节流。
- `Chunk(KB)`：单次 `read_device` 请求大小。

### AXI-Lite 寄存器面板

- 输入项：`寄存器地址`、`写入值`
- 操作项：`读寄存器`、`写寄存器`
- 输出项：`读回值`

行为规则：
- 地址和值支持 `0x..` 或十进制。
- 地址必须 4 字节对齐。
- 范围限制在 `uint32`。
- 若 `user` 通道未打开，会自动尝试打开设备。

## 常用联调步骤

1. 点击 `打开 XDMA 并自检`。
2. 可选点击 `运行协议自测` 验证解包/组帧。
3. 设置 `Chunk(KB)` 和 `节流时间(ms)`。
4. 点击 `开始接收 C2H`，观察预览与日志。
5. 需要寄存器联调时，在 AXI-Lite 面板读写。
6. 点击 `停止`，结束接收并关闭本次落盘。

## 日志关键字

- `XDMA devices detected: ...`
- `[OK] XDMA channels are ready: user + c2h_0.`
- `[RX] received=...`
- `[PROTO] ...`
- `[FRAME] ...`
- `[AXIL] READ addr=... -> value=...`
- `[AXIL] WRITE addr=... <- value=...`
- `[AXIL][ERROR] ...`

## 代码结构

- `widget.h / widget.cpp`：UI、XDMA 打开、自检、线程控制、寄存器读写、落盘。
- `stream_pipeline.h / stream_pipeline.cpp`：协议解包与组帧逻辑。
- `widget.ui`：界面布局。
- `xdmaDLL_public.h`：XDMA 导出接口声明。
- `driver/`：XDMA 动态库与导入库。

## 构建与运行

```bash
qmake camera_viewer.pro
make
```

也可直接使用 Qt Creator 打开 `camera_viewer.pro` 构建运行。

## 寄存器联调备忘

- `0x1000C` 配寄存器（示例值：`0x00102005`）
- `0x15020` 丢包计数
- `0x15018` 丢包计数清零（写 `1`）
- `0x1500C` DMA 大小
