#include "stream_pipeline.h"

#include <cstring>
#include <limits>

namespace {

// ============================================================================
// 通用内部工具
// ============================================================================

/**
 * @brief 判断缓存头部是否是协议 sync（EB 90）。
 *
 * 注意：
 * - 这里只检查前两个字节，不检查 length；
 * - length 合法性由 VideoPacketParser::parsePacket() 再次确认。
 */
bool hasSyncAtHead(const QByteArray &buffer)
{
    if (buffer.size() < 2) {
        return false;
    }
    const uchar *bytes = reinterpret_cast<const uchar *>(buffer.constData());
    return bytes[0] == VideoPacketParser::kSync0 && bytes[1] == VideoPacketParser::kSync1;
}

// ============================================================================
// 自测辅助工具（仅 runStreamPipelineSelfTests 使用）
// ============================================================================

/**
 * @brief 生成固定模式测试数据。
 *
 * 这里刻意生成 1..251 范围的非零字节，目的是在 padding 校验时更容易观察：
 * 如果恢复流中出现多余 0，很容易定位为 padding 泄漏问题。
 */
QByteArray makePatternBytes(int size, int seed)
{
    QByteArray out(size, '\0');
    for (int i = 0; i < size; ++i) {
        out[i] = static_cast<char>(((i * 37) + (seed * 17)) % 251 + 1);
    }
    return out;
}

/**
 * @brief 按发送端协议构造单个 1024B 包。
 */
QByteArray buildProtocolPacket(const QByteArray &payload)
{
    const int payloadLen = qBound(0, payload.size(), VideoPacketParser::kPayloadSize);
    QByteArray packet(VideoPacketParser::kPacketSize, '\0');

    // 固定协议头。
    packet[0] = static_cast<char>(VideoPacketParser::kSync0);
    packet[1] = static_cast<char>(VideoPacketParser::kSync1);
    packet[2] = static_cast<char>((payloadLen >> 8) & 0xFF);
    packet[3] = static_cast<char>(payloadLen & 0xFF);

    // route 字段与发送端默认值保持一致。
    const char dest[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x01};
    const char source[6] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x02};
    const char priority[2] = {0x00, 0x01};
    std::memcpy(packet.data() + 4, dest, sizeof(dest));
    std::memcpy(packet.data() + 10, source, sizeof(source));
    std::memcpy(packet.data() + 16, priority, sizeof(priority));

    // payload 只写入真实 length，剩余保持 0 作为 padding。
    if (payloadLen > 0) {
        std::memcpy(packet.data() + VideoPacketParser::kHeaderSize,
                    payload.constData(),
                    static_cast<size_t>(payloadLen));
    }

    return packet;
}

/**
 * @brief 将任意原始字节流切成协议包并串联。
 */
QByteArray packetizeBytes(const QByteArray &raw)
{
    QByteArray packed;
    int offset = 0;

    while (offset < raw.size()) {
        const int n = qMin(VideoPacketParser::kPayloadSize, raw.size() - offset);
        packed.append(buildProtocolPacket(raw.mid(offset, n)));
        offset += n;
    }

    return packed;
}

/**
 * @brief 按给定模式分段切分输入字节流。
 *
 * 例如 pattern=[300,500,1] 时，会循环按 300/500/1 字节切分，
 * 用于模拟 read() 的任意边界。
 */
QVector<QByteArray> splitBytesByPattern(const QByteArray &input, const QVector<int> &pattern)
{
    QVector<QByteArray> chunks;
    if (pattern.isEmpty()) {
        chunks.push_back(input);
        return chunks;
    }

    int offset = 0;
    int idx = 0;
    while (offset < input.size()) {
        int part = pattern[idx % pattern.size()];
        if (part <= 0) {
            part = 1;
        }

        const int n = qMin(part, input.size() - offset);
        chunks.push_back(input.mid(offset, n));
        offset += n;
        ++idx;
    }

    return chunks;
}

/**
 * @brief 统一记录自测用例结果。
 */
void addCaseResult(StreamPipelineSelfTestReport &report,
                   const QString &name,
                   bool passed,
                   const QString &details)
{
    report.logs.push_back(QString("%1 %2: %3")
                              .arg(passed ? "[PASS]" : "[FAIL]")
                              .arg(name)
                              .arg(details));
    if (!passed) {
        report.allPassed = false;
    }
}

} // namespace

// ============================================================================
// 公共小工具
// ============================================================================

QString joinIntValues(const QVector<int> &values)
{
    QStringList parts;
    parts.reserve(values.size());
    for (const int value : values) {
        parts.push_back(QString::number(value));
    }
    return parts.join(",");
}

QString pixelFormatToString(PixelFormat format)
{
    switch (format) {
    case PixelFormat::YUY2:
        return "YUY2";
    }
    return "Unknown";
}

// ============================================================================
// VideoStreamConfig
// ============================================================================

VideoStreamConfig VideoStreamConfig::defaultYuy2()
{
    return createYuy2(640, 360);
}

VideoStreamConfig VideoStreamConfig::createYuy2(int width, int height)
{
    VideoStreamConfig config;
    config.width = width;
    config.height = height;
    config.pixelFormat = PixelFormat::YUY2;
    config.bytesPerPixel = 2;

    if (width <= 0 || height <= 0) {
        config.rowStride = 0;
        config.frameBytes = 0;
        return config;
    }

    const qint64 rowStride64 = static_cast<qint64>(width) * static_cast<qint64>(config.bytesPerPixel);
    const qint64 frameBytes64 = rowStride64 * static_cast<qint64>(height);
    if (rowStride64 <= 0
        || frameBytes64 <= 0
        || rowStride64 > (std::numeric_limits<int>::max)()
        || frameBytes64 > (std::numeric_limits<int>::max)()) {
        config.rowStride = 0;
        config.frameBytes = 0;
        return config;
    }

    config.rowStride = static_cast<int>(rowStride64);
    config.frameBytes = static_cast<int>(frameBytes64);
    return config;
}

bool VideoStreamConfig::isValid() const
{
    if (width <= 0 || height <= 0 || bytesPerPixel <= 0) {
        return false;
    }

    const qint64 expectedRowStride = static_cast<qint64>(width) * static_cast<qint64>(bytesPerPixel);
    const qint64 expectedFrameBytes = expectedRowStride * static_cast<qint64>(height);
    if (expectedRowStride <= 0
        || expectedFrameBytes <= 0
        || expectedRowStride > (std::numeric_limits<int>::max)()
        || expectedFrameBytes > (std::numeric_limits<int>::max)()) {
        return false;
    }

    return rowStride == static_cast<int>(expectedRowStride)
        && frameBytes == static_cast<int>(expectedFrameBytes);
}

QString VideoStreamConfig::toString() const
{
    return QString("%1x%2, pixel=%3, bytesPerPixel=%4, rowStride=%5, frameBytes=%6")
        .arg(width)
        .arg(height)
        .arg(pixelFormatToString(pixelFormat))
        .arg(bytesPerPixel)
        .arg(rowStride)
        .arg(frameBytes);
}

// ============================================================================
// VideoPacketParser
// ============================================================================

VideoPacketParser::Status VideoPacketParser::parsePacket(const char *packet,
                                                         int packetSize,
                                                         Packet &outPacket)
{
    outPacket = Packet{};

    // 这里要求至少可读 1024B，避免越界访问头部/载荷字段。
    if (!packet || packetSize < kPacketSize) {
        return Status::InvalidPacketSize;
    }

    const uchar *bytes = reinterpret_cast<const uchar *>(packet);
    if (bytes[0] != kSync0 || bytes[1] != kSync1) {
        return Status::SyncMismatch;
    }

    // length 取值范围必须落在 [0,1006]。
    const int payloadLength = (static_cast<int>(bytes[2]) << 8) | static_cast<int>(bytes[3]);
    if (payloadLength < 0 || payloadLength > kPayloadSize) {
        return Status::InvalidLength;
    }

    // 只拷贝 length 指定的真实字节，不把 padding 0 带入输出。
    outPacket.payloadLength = payloadLength;
    if (payloadLength > 0) {
        outPacket.payload = QByteArray(packet + kHeaderSize, payloadLength);
    }

    return Status::Ok;
}

// ============================================================================
// StreamDepacketizer
// ============================================================================

StreamDepacketizer::BatchResult StreamDepacketizer::pushBytes(const QByteArray &data)
{
    return pushBytes(data.constData(), data.size());
}

StreamDepacketizer::BatchResult StreamDepacketizer::pushBytes(const char *data, int size)
{
    BatchResult result;
    if (!data || size <= 0) {
        // 空输入时仍返回当前累计状态，便于外部做统计打印。
        result.protocolCacheBytes = m_cache.size();
        result.totalRestoredBytes = m_totals.restoredBytes;
        result.totalParsedPackets = m_totals.parsedPackets;
        result.totalSyncMismatchCount = m_totals.syncMismatchCount;
        result.totalInvalidLengthCount = m_totals.invalidLengthCount;
        result.totalResyncCount = m_totals.resyncCount;
        return result;
    }

    result.inputBytes = size;
    m_cache.append(data, size);
    m_totals.inputBytes += size;

    // 只要缓存里还有完整 1024B，就持续尝试解包。
    while (m_cache.size() >= VideoPacketParser::kPacketSize) {
        // 头部 sync 不匹配：视为错位，进入重同步。
        if (!hasSyncAtHead(m_cache)) {
            ++result.syncMismatchCount;
            resyncAfterError(result, 1);
            continue;
        }

        VideoPacketParser::Packet packet;
        const VideoPacketParser::Status status =
            VideoPacketParser::parsePacket(m_cache.constData(), m_cache.size(), packet);

        if (status == VideoPacketParser::Status::Ok) {
            // 成功：收集真实 payload，并弹出整包 1024B。
            if (packet.payloadLength > 0) {
                result.restoredBytes.append(packet.payload);
            }
            result.packetLengths.push_back(packet.payloadLength);
            ++result.parsedPackets;
            m_cache.remove(0, VideoPacketParser::kPacketSize);
            continue;
        }

        if (status == VideoPacketParser::Status::InvalidLength) {
            // sync 对齐但 length 非法，也按坏包处理并尝试重同步。
            ++result.invalidLengthCount;
            resyncAfterError(result, 1);
            continue;
        }

        if (status == VideoPacketParser::Status::SyncMismatch) {
            // 理论上前面 hasSyncAtHead 已挡住该情况，保底处理。
            ++result.syncMismatchCount;
            resyncAfterError(result, 1);
            continue;
        }

        // InvalidPacketSize 等不可恢复状态（正常不会命中）时退出循环。
        break;
    }

    m_totals.restoredBytes += result.restoredBytes.size();
    m_totals.parsedPackets += result.parsedPackets;
    m_totals.syncMismatchCount += result.syncMismatchCount;
    m_totals.invalidLengthCount += result.invalidLengthCount;
    m_totals.resyncCount += result.resyncCount;

    result.protocolCacheBytes = m_cache.size();
    result.totalRestoredBytes = m_totals.restoredBytes;
    result.totalParsedPackets = m_totals.parsedPackets;
    result.totalSyncMismatchCount = m_totals.syncMismatchCount;
    result.totalInvalidLengthCount = m_totals.invalidLengthCount;
    result.totalResyncCount = m_totals.resyncCount;
    return result;
}

int StreamDepacketizer::bufferedBytes() const
{
    return m_cache.size();
}

void StreamDepacketizer::reset()
{
    m_cache.clear();
    m_totals = Totals{};
}

const StreamDepacketizer::Totals &StreamDepacketizer::totals() const
{
    return m_totals;
}

int StreamDepacketizer::findNextSync(int fromIndex) const
{
    const int searchStart = qMax(0, fromIndex);
    const int lastStart = m_cache.size() - 2;
    if (searchStart > lastStart) {
        return -1;
    }

    const uchar *bytes = reinterpret_cast<const uchar *>(m_cache.constData());
    for (int i = searchStart; i <= lastStart; ++i) {
        if (bytes[i] == VideoPacketParser::kSync0 && bytes[i + 1] == VideoPacketParser::kSync1) {
            return i;
        }
    }

    return -1;
}

void StreamDepacketizer::resyncAfterError(BatchResult &result, int searchFromIndex)
{
    if (m_cache.isEmpty()) {
        return;
    }

    const int syncPos = findNextSync(searchFromIndex);
    if (syncPos > 0) {
        // 在缓存内部找到了下一个 sync：丢弃前导垃圾字节并对齐。
        m_cache.remove(0, syncPos);
        ++result.resyncCount;
        result.resyncPositions.push_back(syncPos);
        return;
    }

    if (syncPos == 0) {
        // 已对齐到 sync，无需处理。
        return;
    }

    // 没有找到完整 sync（EB 90）时，最多保留末尾一个 EB：
    // - 若下一批首字节是 90，则可跨批次组成完整 sync；
    // - 其余字节全部丢弃，避免缓存无限增长。
    const int keepBytes = (!m_cache.isEmpty()
                           && static_cast<uchar>(m_cache.at(m_cache.size() - 1)) == VideoPacketParser::kSync0)
        ? 1
        : 0;

    const int dropBytes = m_cache.size() - keepBytes;
    if (dropBytes <= 0) {
        return;
    }

    m_cache.remove(0, dropBytes);
    ++result.resyncCount;
    result.resyncPositions.push_back(dropBytes);
}

// ============================================================================
// FrameAssembler
// ============================================================================

FrameAssembler::FrameAssembler(int frameBytes)
    : m_frameBytes(frameBytes)
{
}

void FrameAssembler::setFrameBytes(int frameBytes)
{
    m_frameBytes = frameBytes;
    // 参数切换后直接 reset，避免旧缓存按新帧长误切。
    reset();
}

int FrameAssembler::frameBytes() const
{
    return m_frameBytes;
}

int FrameAssembler::bufferedBytes() const
{
    return m_cache.size();
}

FrameAssembler::BatchResult FrameAssembler::pushBytes(const QByteArray &bytes)
{
    return pushBytes(bytes.constData(), bytes.size());
}

FrameAssembler::BatchResult FrameAssembler::pushBytes(const char *data, int size)
{
    BatchResult result;
    if (!data || size <= 0) {
        result.frameCacheBytes = m_cache.size();
        result.totalInputBytes = m_totalInputBytes;
        result.totalFramesOutput = m_totalFramesOutput;
        return result;
    }

    result.inputBytes = size;
    m_totalInputBytes += size;
    m_cache.append(data, size);

    // 不看“包边界/批次边界”，只按固定 frameBytes 切帧。
    if (m_frameBytes > 0) {
        while (m_cache.size() >= m_frameBytes) {
            result.frames.push_back(m_cache.left(m_frameBytes));
            m_cache.remove(0, m_frameBytes);
            ++result.framesOutput;
            ++m_totalFramesOutput;
        }
    }

    result.frameCacheBytes = m_cache.size();
    result.totalInputBytes = m_totalInputBytes;
    result.totalFramesOutput = m_totalFramesOutput;
    return result;
}

void FrameAssembler::reset()
{
    m_cache.clear();
    m_totalInputBytes = 0;
    m_totalFramesOutput = 0;
}

// ============================================================================
// Yuy2FrameReassembler
// ============================================================================

Yuy2FrameReassembler::Yuy2FrameReassembler(const VideoStreamConfig &config)
    : m_config(config)
    , m_assembler(config.frameBytes)
{
}

void Yuy2FrameReassembler::setConfig(const VideoStreamConfig &config)
{
    m_config = config;
    m_assembler.setFrameBytes(config.frameBytes);
}

const VideoStreamConfig &Yuy2FrameReassembler::config() const
{
    return m_config;
}

int Yuy2FrameReassembler::bufferedBytes() const
{
    return m_assembler.bufferedBytes();
}

Yuy2FrameReassembler::BatchResult Yuy2FrameReassembler::pushBytes(const QByteArray &bytes)
{
    const FrameAssembler::BatchResult baseResult = m_assembler.pushBytes(bytes);

    BatchResult result;
    result.frames = baseResult.frames;
    result.inputBytes = baseResult.inputBytes;
    result.frameCacheBytes = baseResult.frameCacheBytes;
    result.framesOutput = baseResult.framesOutput;
    result.totalInputBytes = baseResult.totalInputBytes;
    result.totalFramesOutput = baseResult.totalFramesOutput;
    return result;
}

void Yuy2FrameReassembler::reset()
{
    m_assembler.reset();
}

// ============================================================================
// 自测入口
// ============================================================================

StreamPipelineSelfTestReport runStreamPipelineSelfTests(const VideoStreamConfig &config)
{
    StreamPipelineSelfTestReport report;
    report.allPassed = true;

    if (!config.isValid()) {
        addCaseResult(report,
                      "config_validity",
                      false,
                      QString("invalid config: %1").arg(config.toString()));
        return report;
    }
    addCaseResult(report,
                  "config_validity",
                  true,
                  QString("config=%1").arg(config.toString()));

    // 统一分段模式：覆盖小块、大块、单字节等多种 read 边界。
    const QVector<int> splitPattern = {300, 500, 700, 2048, 1, 4096};

    // 用例1：单次输入，验证解包后字节流与原始流完全一致。
    const QByteArray source1 = makePatternBytes(VideoPacketParser::kPayloadSize * 3 + 517, 1);
    const QByteArray encoded1 = packetizeBytes(source1);
    StreamDepacketizer dep1;
    const StreamDepacketizer::BatchResult dep1Result = dep1.pushBytes(encoded1);
    const bool dep1Pass = dep1Result.restoredBytes == source1
        && dep1Result.parsedPackets == ((source1.size() + VideoPacketParser::kPayloadSize - 1)
                                        / VideoPacketParser::kPayloadSize);
    addCaseResult(report,
                  "depacketize_single_chunk",
                  dep1Pass,
                  QString("input=%1 encoded=%2 restored=%3 packets=%4")
                      .arg(source1.size())
                      .arg(encoded1.size())
                      .arg(dep1Result.restoredBytes.size())
                      .arg(dep1Result.parsedPackets));

    // 用例2：任意分段输入（含 1B）下恢复一致。
    StreamDepacketizer dep2;
    QByteArray dep2Output;
    const QVector<QByteArray> encodedChunks = splitBytesByPattern(encoded1, splitPattern);
    for (const QByteArray &chunk : encodedChunks) {
        dep2Output.append(dep2.pushBytes(chunk).restoredBytes);
    }
    addCaseResult(report,
                  "depacketize_segmented_input",
                  dep2Output == source1,
                  QString("chunks=%1 output=%2 expected=%3")
                      .arg(encodedChunks.size())
                      .arg(dep2Output.size())
                      .arg(source1.size()));

    // 用例3：padding 零字节不进入恢复后的真实流。
    const QByteArray source3 = makePatternBytes(VideoPacketParser::kPayloadSize + 7, 2);
    const QByteArray encoded3 = packetizeBytes(source3);
    StreamDepacketizer dep3;
    const QByteArray restored3 = dep3.pushBytes(encoded3).restoredBytes;
    addCaseResult(report,
                  "padding_not_leaked",
                  restored3 == source3 && restored3.size() == source3.size(),
                  QString("source=%1 restored=%2").arg(source3.size()).arg(restored3.size()));

    // 用例4：固定帧长重组，验证帧内容与残余缓存长度。
    const int extraBytes = 123;
    const QByteArray source4 = makePatternBytes(config.frameBytes * 3 + extraBytes, 3);
    Yuy2FrameReassembler reassembler4(config);
    QVector<QByteArray> frames4;
    const QVector<QByteArray> sourceChunks4 = splitBytesByPattern(source4, splitPattern);
    for (const QByteArray &chunk : sourceChunks4) {
        const Yuy2FrameReassembler::BatchResult r = reassembler4.pushBytes(chunk);
        for (const QByteArray &frame : r.frames) {
            frames4.push_back(frame);
        }
    }

    bool framePayloadMatch = (frames4.size() == 3);
    if (framePayloadMatch) {
        for (int i = 0; i < frames4.size(); ++i) {
            const QByteArray expected = source4.mid(i * config.frameBytes, config.frameBytes);
            if (frames4[i] != expected) {
                framePayloadMatch = false;
                break;
            }
        }
    }
    addCaseResult(report,
                  "frame_reassemble_fixed_size",
                  framePayloadMatch && reassembler4.bufferedBytes() == extraBytes,
                  QString("frames=%1 cache=%2 expectedCache=%3 frameBytes=%4")
                      .arg(frames4.size())
                      .arg(reassembler4.bufferedBytes())
                      .arg(extraBytes)
                      .arg(config.frameBytes));

    // 用例5：sync 错位 + 非法 length 重同步。
    const QByteArray source5a = makePatternBytes(300, 4);
    const QByteArray source5b = makePatternBytes(128, 5);
    QByteArray invalidPacket(VideoPacketParser::kPacketSize, '\0');
    invalidPacket[0] = static_cast<char>(VideoPacketParser::kSync0);
    invalidPacket[1] = static_cast<char>(VideoPacketParser::kSync1);
    invalidPacket[2] = static_cast<char>(0xFF);
    invalidPacket[3] = static_cast<char>(0xFF);

    QByteArray stream5;
    stream5.append('\x12');
    stream5.append('\x34');
    stream5.append('\x56');
    stream5.append(invalidPacket);
    stream5.append(buildProtocolPacket(source5a));
    stream5.append(buildProtocolPacket(source5b));

    StreamDepacketizer dep5;
    const StreamDepacketizer::BatchResult dep5Result = dep5.pushBytes(stream5);
    const QByteArray expected5 = source5a + source5b;
    addCaseResult(report,
                  "resync_on_sync_and_length_error",
                  dep5Result.restoredBytes == expected5
                      && dep5Result.syncMismatchCount > 0
                      && dep5Result.invalidLengthCount > 0
                      && dep5Result.resyncCount > 0,
                  QString("output=%1 expected=%2 syncMismatch=%3 invalidLength=%4 resync=%5")
                      .arg(dep5Result.restoredBytes.size())
                      .arg(expected5.size())
                      .arg(dep5Result.syncMismatchCount)
                      .arg(dep5Result.invalidLengthCount)
                      .arg(dep5Result.resyncCount));

    // 用例6：端到端（解包 + 组帧）在任意分段下保持正确。
    const QByteArray source6 = makePatternBytes(config.frameBytes * 2 + 999, 6);
    const QByteArray encoded6 = packetizeBytes(source6);
    StreamDepacketizer dep6;
    Yuy2FrameReassembler reassembler6(config);
    QByteArray restored6;
    int emittedFrames6 = 0;
    const QVector<QByteArray> encodedChunks6 = splitBytesByPattern(encoded6, splitPattern);
    for (const QByteArray &chunk : encodedChunks6) {
        const StreamDepacketizer::BatchResult dep = dep6.pushBytes(chunk);
        restored6.append(dep.restoredBytes);

        const Yuy2FrameReassembler::BatchResult frame = reassembler6.pushBytes(dep.restoredBytes);
        emittedFrames6 += frame.framesOutput;
    }

    addCaseResult(report,
                  "end_to_end_segmented_pipeline",
                  restored6 == source6 && emittedFrames6 == 2 && reassembler6.bufferedBytes() == 999,
                  QString("restored=%1 expected=%2 frames=%3 cache=%4")
                      .arg(restored6.size())
                      .arg(source6.size())
                      .arg(emittedFrames6)
                      .arg(reassembler6.bufferedBytes()));

    return report;
}
