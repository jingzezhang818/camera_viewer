#ifndef STREAM_PIPELINE_H
#define STREAM_PIPELINE_H

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

// ============================================================================
// 像素格式与流配置
// ============================================================================

/**
 * @brief 当前支持的像素格式。
 *
 * 目前接收链路只实现了 YUY2（YUYV 4:2:2）路径，后续可以在此扩展。
 */
enum class PixelFormat {
    YUY2 = 0
};

/**
 * @brief 像素格式转可读字符串（用于日志）。
 */
QString pixelFormatToString(PixelFormat format);

/**
 * @brief 视频流固定参数集合。
 *
 * 设计目标：
 * - 将 width/height/pixel/frameBytes 等关键参数集中管理。
 * - 当前默认值固定为 640x360 YUY2（2B/pixel，rowStride=1280，frameBytes=460800）。
 * - 后续若改为配置化，仅需在该结构及调用入口扩展。
 */
struct VideoStreamConfig
{
    int width = 640;
    int height = 360;
    PixelFormat pixelFormat = PixelFormat::YUY2;
    int bytesPerPixel = 2;
    int rowStride = 1280;
    int frameBytes = 460800;

    /**
     * @brief 返回默认配置（640x360 YUY2）。
     */
    static VideoStreamConfig defaultYuy2();

    /**
     * @brief 根据分辨率生成 YUY2 配置，并自动计算 stride/frameBytes。
     */
    static VideoStreamConfig createYuy2(int width, int height);

    /**
     * @brief 校验配置内部字段是否自洽。
     *
     * 校验点：
     * - 尺寸与 bytesPerPixel 为正数。
     * - rowStride == width * bytesPerPixel。
     * - frameBytes == rowStride * height。
     * - 计算过程无 int 溢出。
     */
    bool isValid() const;

    /**
     * @brief 输出便于日志展示的配置摘要。
     */
    QString toString() const;
};

// ============================================================================
// 协议包解析（固定 1024B）
// ============================================================================

/**
 * @brief 单个协议包解析器。
 *
 * 发送端协议（Camera_PC）：
 * - 包长固定 1024B
 * - 头部 18B，payload 区域 1006B
 * - 字段布局：
 *   EB 90 | lengthH | lengthL | dest(6) | source(6) | priority(2) | payload(1006)
 * - length 表示 payload 的真实有效字节数，范围 [0,1006]
 * - payload 其余字节可能是 0 padding，必须丢弃
 */
class VideoPacketParser
{
public:
    static constexpr int kPacketSize = 1024;
    static constexpr int kHeaderSize = 18;
    static constexpr int kPayloadSize = kPacketSize - kHeaderSize;
    static constexpr quint8 kSync0 = 0xEB;
    static constexpr quint8 kSync1 = 0x90;

    /**
     * @brief 解析结果状态。
     */
    enum class Status {
        Ok = 0,
        InvalidPacketSize,
        SyncMismatch,
        InvalidLength
    };

    /**
     * @brief 单包解析输出。
     * payload 中只包含前 length 字节有效数据，不包含 padding。
     */
    struct Packet {
        int payloadLength = 0;
        QByteArray payload;
    };

    /**
     * @brief 解析单个 1024B 协议包。
     * @param packet 指向协议包起始地址
     * @param packetSize 当前可访问字节数，至少需要 1024
     * @param outPacket 成功时返回有效载荷
     */
    static Status parsePacket(const char *packet, int packetSize, Packet &outPacket);
};

// ============================================================================
// 增量解包：任意分段输入 -> 恢复真实视频字节流
// ============================================================================

/**
 * @brief 字节流解包器。
 *
 * 输入特性：
 * - 可接收任意长度分段（1B、300B、500B、2KB、1MiB 等）
 * - 不能假设 read 边界等于协议包边界
 *
 * 行为：
 * - 内部维护缓存 m_cache
 * - 当缓存 >= 1024B 时持续尝试解析协议包
 * - sync 或 length 异常时执行重同步（搜索下一个 EB 90）
 * - 只输出真实 payload 字节，不输出 padding
 */
class StreamDepacketizer
{
public:
    /**
     * @brief 单次 push 的结果与统计。
     */
    struct BatchResult {
        // 本次恢复出的真实视频字节流。
        QByteArray restoredBytes;

        // 输入与解析状态（本次）。
        int inputBytes = 0;
        int protocolCacheBytes = 0;
        int parsedPackets = 0;
        QVector<int> packetLengths;

        // 异常与重同步统计（本次）。
        int syncMismatchCount = 0;
        int invalidLengthCount = 0;
        int resyncCount = 0;
        QVector<int> resyncPositions;

        // 累计统计（总计）。
        qint64 totalRestoredBytes = 0;
        qint64 totalParsedPackets = 0;
        qint64 totalSyncMismatchCount = 0;
        qint64 totalInvalidLengthCount = 0;
        qint64 totalResyncCount = 0;
    };

    /**
     * @brief 累计统计快照。
     */
    struct Totals {
        qint64 inputBytes = 0;
        qint64 restoredBytes = 0;
        qint64 parsedPackets = 0;
        qint64 syncMismatchCount = 0;
        qint64 invalidLengthCount = 0;
        qint64 resyncCount = 0;
    };

    /**
     * @brief 输入增量字节并尝试解析。
     */
    BatchResult pushBytes(const char *data, int size);
    BatchResult pushBytes(const QByteArray &data);

    /**
     * @brief 当前未解析缓存长度。
     */
    int bufferedBytes() const;

    /**
     * @brief 清空缓存与累计统计。
     */
    void reset();

    /**
     * @brief 获取累计统计。
     */
    const Totals &totals() const;

private:
    /**
     * @brief 在缓存中查找下一个 sync（EB 90）起点。
     */
    int findNextSync(int fromIndex) const;

    /**
     * @brief 发生同步/长度异常后的重同步处理。
     */
    void resyncAfterError(BatchResult &result, int searchFromIndex);

    QByteArray m_cache;
    Totals m_totals;
};

// ============================================================================
// 固定帧长组帧：真实字节流 -> 完整帧
// ============================================================================

/**
 * @brief 通用固定帧长组帧器。
 *
 * 行为：
 * - 持续累积输入字节
 * - 每达到 m_frameBytes 就输出一帧
 * - 不依赖 transport/batch 边界
 */
class FrameAssembler
{
public:
    /**
     * @brief 单次 push 的组帧结果。
     */
    struct BatchResult {
        QVector<QByteArray> frames;
        int inputBytes = 0;
        int frameCacheBytes = 0;
        int framesOutput = 0;
        qint64 totalInputBytes = 0;
        qint64 totalFramesOutput = 0;
    };

    explicit FrameAssembler(int frameBytes = 0);

    /**
     * @brief 设置固定帧长（字节）。设置后会 reset 缓存。
     */
    void setFrameBytes(int frameBytes);

    int frameBytes() const;
    int bufferedBytes() const;

    BatchResult pushBytes(const QByteArray &bytes);
    BatchResult pushBytes(const char *data, int size);

    /**
     * @brief 清空组帧缓存与累计计数。
     */
    void reset();

private:
    int m_frameBytes = 0;
    QByteArray m_cache;
    qint64 m_totalInputBytes = 0;
    qint64 m_totalFramesOutput = 0;
};

/**
 * @brief YUY2 语义化组帧器。
 *
 * 该类是 FrameAssembler 的轻封装，附带 VideoStreamConfig 语义，
 * 便于上层表达“按某个视频配置做组帧”。
 */
class Yuy2FrameReassembler
{
public:
    struct BatchResult {
        QVector<QByteArray> frames;
        int inputBytes = 0;
        int frameCacheBytes = 0;
        int framesOutput = 0;
        qint64 totalInputBytes = 0;
        qint64 totalFramesOutput = 0;
    };

    explicit Yuy2FrameReassembler(const VideoStreamConfig &config = VideoStreamConfig::defaultYuy2());

    /**
     * @brief 更新配置并同步重置底层 frameBytes。
     */
    void setConfig(const VideoStreamConfig &config);

    const VideoStreamConfig &config() const;
    int bufferedBytes() const;

    BatchResult pushBytes(const QByteArray &bytes);
    void reset();

private:
    VideoStreamConfig m_config;
    FrameAssembler m_assembler;
};

// ============================================================================
// 管线自测
// ============================================================================

/**
 * @brief 管线自测报告。
 */
struct StreamPipelineSelfTestReport
{
    bool allPassed = false;
    QStringList logs;
};

/**
 * @brief 运行协议解包 + 固定帧长组帧自测。
 *
 * 用例覆盖：
 * - 单次输入解包一致性
 * - 任意分段输入解包一致性
 * - padding 不泄漏
 * - 固定帧长组帧
 * - sync 错位 / 非法 length 重同步
 * - 端到端任意分段链路一致性
 */
StreamPipelineSelfTestReport runStreamPipelineSelfTests(const VideoStreamConfig &config);

/**
 * @brief 将整数数组拼接为逗号分隔文本（日志辅助）。
 */
QString joinIntValues(const QVector<int> &values);

#endif // STREAM_PIPELINE_H
