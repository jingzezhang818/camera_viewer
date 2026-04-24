# camera_viewer

`camera_viewer` 是一个基于 Qt Widgets 的 XDMA C2H 视频查看工具。项目已经按发送端 `Camera_PC` 的当前协议对齐，实现了从底层字节流到完整 YUY2 帧显示的完整链路。

## 项目功能

- 打开 XDMA 设备并执行 `ready_state` 自检。
- 在后台线程持续读取 `c2h_0` 任意分段字节流。
- 按固定 1024B 协议包解包，恢复真实视频字节流。
- 按固定帧长 `460800B`（`640x360x2`）重组完整 YUY2 帧。
- 将完整帧接入现有显示链路（YUY2 -> RGB888 预览）。
- 同步保存原始 `.yuv` 数据，便于离线分析。
- 提供独立“运行协议自测”按钮，自测与 XDMA 物理链路解耦。

## 当前协议与默认视频参数

### 协议（发送端 Camera_PC）

- 固定包长：`1024B`
- 头部长度：`18B`
- Payload 区域：`1006B`
- 包格式：`EB 90 | lengthH | lengthL | dest(6) | source(6) | priority(2) | payload(1006)`
- `length=(lengthH<<8)|lengthL`，取值 `0..1006`
- payload 后续字节可能为 `0` padding，接收端会丢弃

### 默认视频配置

- 分辨率：`640x360`
- 像素格式：`YUY2`
- `bytesPerPixel=2`
- `rowStride=640*2=1280`
- `frameBytes=640*360*2=460800`

## 端到端处理逻辑

最终调用链如下：

`read_device(c2h_0)` -> `StreamDepacketizer` -> `Yuy2FrameReassembler` -> `Widget::onReaderFrameReady` -> 预览显示/原始落盘

具体规则：

- 不依赖 `read_device` 返回边界。
- 不依赖 1MiB 批次边界。
- 不依赖“短 payload 包”判断帧结束。
- 帧边界只由累计真实字节是否达到 `frameBytes=460800` 决定。

## 模块结构

- `main.cpp`：程序入口，创建 Qt 应用和主窗口。
- `widget.h/.cpp`：UI 控制、线程管理、XDMA 打开/关闭、帧显示、YUV 落盘。
- `stream_pipeline.h/.cpp`：协议解包与固定帧长重组核心逻辑，自测也在此实现。
- `widget.ui`：界面布局与按钮定义。
- `xdmaDLL_public.h`：驱动接口声明。
- `driver/`：XDMA 相关库文件目录（Windows）。

## 关键类说明

- `VideoStreamConfig`
- 统一管理视频配置（宽高、像素格式、stride、frameBytes），当前默认 `640x360 YUY2`。

- `VideoPacketParser`
- 解析单个 1024B 协议包，校验 sync 与 length，只输出真实 payload。

- `StreamDepacketizer`
- 输入任意长度字节流，内部缓存并增量解包。
- sync 错位或 length 非法时自动重同步（搜索下一个 `EB 90`）。

- `FrameAssembler`
- 通用固定帧长累积器，按 `frameBytes` 持续输出完整帧。

- `Yuy2FrameReassembler`
- 基于 `VideoStreamConfig` 的 YUY2 语义化封装，底层复用 `FrameAssembler`。

- `C2hReaderWorker`
- 后台线程执行 `读设备 -> 解包 -> 组帧`，将完整帧发回 UI 线程。

## UI 用法

1. 点击“打开 XDMA 并自检”。
2. 可选点击“运行协议自测”，验证协议解包和组帧逻辑。
3. 设置 `Chunk(KB)` 与 `节流(ms)`。
4. 点击“开始接收 C2H”。
5. 预览窗口显示实时画面，日志窗口显示接收统计。
6. 点击“停止”结束接收并关闭本次落盘文件。

## 协议自测按钮说明

“运行协议自测”不依赖 XDMA 硬件，覆盖以下场景：

- 协议包解包正确性。
- 任意分段输入（300/500/700/2048/1/4096B）一致性。
- padding 0 不泄漏到恢复后字节流。
- 固定帧长 `460800B` 组帧正确性。
- sync 错位与非法 length 的重同步能力。
- 端到端分段输入下解包+组帧一致性。

## 日志重点字段

- `[RX] received=...`：本次从驱动读到的字节数。
- `[PROTO] cache=... parsed=... restored+...`：协议缓存、解析包数、恢复字节数。
- `[PROTO] packet_lengths=...`：本批成功包的 length 列表。
- `[PROTO] resync_positions=...`：本批重同步时丢弃或对齐的位置。
- `[FRAME] cache=... output_frames+...`：组帧缓存和输出帧数。

## 构建说明

- 项目文件：`camera_viewer.pro`
- Qt：Qt5/Qt6 Widgets 均可按环境调整，当前工程使用 qmake。
- Windows 需能找到 `driver/` 目录下 XDMA 库（`XDMA_MoreB`）。

示例：

```bash
qmake camera_viewer.pro
make
```

如果命令行环境未配置，可直接用 Qt Creator 打开 `.pro` 构建。

## 注意事项

- 当前接收链路固定使用 `640x360 YUY2` 配置。
- UI 中宽高会在开始接收时被同步为固定配置，避免与真实链路不一致。
- 若要改分辨率/像素格式，优先从 `VideoStreamConfig` 和相关调用入口统一调整。
