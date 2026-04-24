# camera_viewer

`camera_viewer` 是一个基于 Qt Widgets 的 XDMA C2H 视频接收与显示工具。项目已对齐 `camera_PC` 的视频协议，并新增与发送端同款的 user 通道 AXI-Lite 寄存器读写功能。

## 1. 项目思路

`camera_viewer` 把接收端流程拆成三段：

1. 设备与通道层
- 打开 XDMA 设备。
- 打开 `user + c2h_0`。
- 执行 `ready_state` 自检。

2. 数据处理层
- 后台线程持续 `read_device(c2h_0)`。
- 协议解包：1024B 包 -> 恢复真实视频 payload。
- 固定帧长组帧：输出完整 YUY2 帧。

3. UI 与调试层
- 预览显示、日志、原始 `.yuv` 落盘。
- AXI-Lite 面板用于寄存器读写调试。

## 2. 核心功能

1. 打开 XDMA 并自检。
2. 后台持续接收 C2H 字节流。
3. 按协议解包并恢复视频数据。
4. 按固定帧长重组完整帧并实时预览。
5. 原始 YUYV422 数据落盘。
6. 协议自测（不依赖 XDMA 硬件）。
7. AXI-Lite 寄存器读写（通过 user 通道）。

## 3. 协议与默认配置

### 3.1 对齐发送端协议

- 包长：`1024B`
- 头长：`18B`
- payload：`1006B`
- 包格式：`EB 90 | lengthH | lengthL | dest(6) | source(6) | priority(2) | payload(1006)`

解析规则：
- `length` 取 `0..1006`。
- 只提取有效 payload，自动忽略 padding 0。
- sync 错位或非法 length 时自动重同步。

### 3.2 默认视频配置

- 分辨率：`640x360`
- 像素格式：`YUY2`
- `frameBytes = 640 * 360 * 2 = 460800`

## 4. UI 使用说明

### 4.1 顶部按钮

- `打开 XDMA 并自检`
- `运行协议自测`
- `开始接收 C2H`
- `停止`

### 4.2 参数区

- `宽/高`：接收时会按固定配置同步。
- `节流时间(ms)`：控制帧输出节流。
- `Chunk(KB)`：单次 `read_device` 请求大小。

### 4.3 AXI-Lite 寄存器读写（新增）

该面板位于参数区和预览区之间，包含：
- `寄存器地址`
- `写入值`
- `读寄存器`
- `写寄存器`
- `读回值`

行为与 `camera_PC` 一致：
- 地址和值都支持 `0x..` 或十进制。
- 地址必须 4 字节对齐。
- 范围限制在 `uint32`。
- 若 user 通道未打开，会自动尝试 `openXdmaAndSelfCheck()`。

## 5. 调用链

### 5.1 C2H 接收链路

`read_device(c2h_0)` -> `StreamDepacketizer` -> `Yuy2FrameReassembler` -> `onReaderFrameReady()` -> 预览显示/落盘

### 5.2 寄存器读链路

`读寄存器按钮` -> `parseUiRegisterValue()` -> `readUserRegister()` -> `read_device(user, addr, 4, ...)`

### 5.3 寄存器写链路

`写寄存器按钮` -> `parseUiRegisterValue()` -> `writeUserRegister()` -> `write_device(user, addr, 4, ...)`

## 6. 推荐联调流程

1. 点 `打开 XDMA 并自检`。
2. 可选点 `运行协议自测` 验证解包/组帧逻辑。
3. 设定 `Chunk(KB)` 与 `节流时间(ms)`。
4. 点 `开始接收 C2H`，观察预览与日志。
5. 需要硬件寄存器联调时，直接在 AXI-Lite 面板读写。
6. 点 `停止` 结束接收并关闭本次落盘。

## 7. 日志速查

- `XDMA devices detected: ...`
- `[OK] XDMA channels are ready: user + c2h_0.`
- `[RX] received=...`
- `[PROTO] ...`
- `[FRAME] ...`
- `[AXIL] READ addr=... -> value=...`
- `[AXIL] WRITE addr=... <- value=...`
- `[AXIL][ERROR] ...`

## 8. 代码结构

- `widget.h / widget.cpp`：UI、XDMA、线程、寄存器读写。
- `stream_pipeline.h / stream_pipeline.cpp`：协议解包与组帧。
- `widget.ui`：主界面布局。
- `xdmaDLL_public.h`：XDMA 导出接口声明。
- `driver/`：XDMA 动态库与导入库。

## 9. 构建

```bash
qmake camera_viewer.pro
make
```

也可直接使用 Qt Creator 打开 `camera_viewer.pro` 构建运行。
