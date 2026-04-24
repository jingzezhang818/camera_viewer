#include "widget.h"
#include "ui_widget.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QPixmap>
#include <QResizeEvent>
#include <QThread>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>

#include <limits>
#include <vector>

#include "stream_pipeline.h"
#include "xdmaDLL_public.h"

namespace {

// -------------------- 运行时常量 --------------------
// 线程停止等待时间。超时会触发强制终止兜底。
constexpr int kReaderStopWaitMs = 2000;
// 读失败时的短退避，避免错误日志高频刷屏。
constexpr int kReaderErrorBackoffMs = 5;
// 允许的读取 chunk 下限/上限，保证任意分段输入都能工作。
constexpr int kMinReadChunkBytes = 1;
constexpr int kMaxReadChunkBytes = 8 * 1024 * 1024;

/**
 * @brief isValidHandle
 * 统一判断 Windows HANDLE 是否有效。
 */
bool isValidHandle(HANDLE handle)
{
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}

/**
 * @brief clampToByte
 * 将整数裁剪到 [0,255]，用于 YUV->RGB 转换时防止溢出。
 */
int clampToByte(int value)
{
    if (value < 0) {
        return 0;
    }
    if (value > 255) {
        return 255;
    }
    return value;
}

// 统一输出 32bit 十六进制字符串（大写、固定 8 位）。
// 用于寄存器读写日志与读回值展示，保证格式一致。
QString toHex32(quint32 value)
{
    return QString("0x%1").arg(value, 8, 16, QLatin1Char('0')).toUpper();
}

} // namespace

// ============================================================================
// C2hReaderWorker: 生命周期与主循环
// ============================================================================

C2hReaderWorker::C2hReaderWorker(QObject *parent)
    : QObject(parent)
{
}

void C2hReaderWorker::requestStop()
{
    m_running.store(false);
}

void C2hReaderWorker::start(quintptr c2hHandleValue,
                            int frameBytes,
                            int chunkBytes,
                            int throttleMs,
                            int width,
                            int height)
{
    // 防止同一 worker 被重复启动。
    if (m_running.exchange(true)) {
        emit readError(-2001);
        return;
    }

    // 先校验输入参数，避免进入读取循环后再失败。
    const HANDLE sourceHandle = reinterpret_cast<HANDLE>(c2hHandleValue);
    const VideoStreamConfig streamConfig = VideoStreamConfig::createYuy2(width, height);
    if (!isValidHandle(sourceHandle) || frameBytes <= 0 || !streamConfig.isValid()) {
        m_running.store(false);
        emit readError(-2002);
        emit stopped();
        return;
    }

    if (frameBytes != streamConfig.frameBytes) {
        emit workerLog(QString("[WARN] frameBytes mismatch: arg=%1, config=%2")
                           .arg(frameBytes)
                           .arg(streamConfig.frameBytes));
    }

    // 在 worker 线程中复制一份句柄，避免 UI 线程关闭原句柄导致并发访问风险。
    HANDLE c2h = INVALID_HANDLE_VALUE;
    if (!DuplicateHandle(GetCurrentProcess(),
                         sourceHandle,
                         GetCurrentProcess(),
                         &c2h,
                         0,
                         FALSE,
                         DUPLICATE_SAME_ACCESS)
        || !isValidHandle(c2h)) {
        m_running.store(false);
        emit readError(-2004);
        emit stopped();
        return;
    }

    // 对 UI 配置的 chunk 做边界约束，确保传给驱动的请求尺寸安全。
    const int safeChunkBytes = qBound(kMinReadChunkBytes, chunkBytes, kMaxReadChunkBytes);
    BYTE *chunkBuffer = allocate_buffer(static_cast<size_t>(safeChunkBytes), 0);
    if (!chunkBuffer) {
        CloseHandle(c2h);
        m_running.store(false);
        emit readError(-2003);
        emit stopped();
        return;
    }

    emit workerLog(QString("[INFO] Reader pipeline: packet=%1B header=%2B payload=%3B frameBytes=%4 chunk=%5B")
                       .arg(VideoPacketParser::kPacketSize)
                       .arg(VideoPacketParser::kHeaderSize)
                       .arg(VideoPacketParser::kPayloadSize)
                       .arg(streamConfig.frameBytes)
                       .arg(safeChunkBytes));

    // 构建两级处理模块：协议解包 + 固定帧长重组。
    StreamDepacketizer depacketizer;
    Yuy2FrameReassembler frameReassembler(streamConfig);

    // 主循环（持续到 requestStop 或读取异常退出）：
    // 1) read_device 读取任意长度分段字节
    // 2) depacketizer 恢复真实视频字节流
    // 3) reassembler 按 frameBytes 输出完整帧
    while (m_running.load()) {
        const int ret = read_device(c2h, 0x00000000, static_cast<DWORD>(safeChunkBytes), chunkBuffer);
        if (!m_running.load()) {
            break;
        }

        if (ret <= 0) {
            emit readError(ret);
            QThread::msleep(static_cast<unsigned long>(kReaderErrorBackoffMs));
            continue;
        }

        const QByteArray incoming(reinterpret_cast<const char *>(chunkBuffer), ret);
        emit workerLog(QString("[RX] received=%1B").arg(ret));

        const StreamDepacketizer::BatchResult dep = depacketizer.pushBytes(incoming);
        emit workerLog(QString("[PROTO] cache=%1B parsed=%2 restored+%3B restoredTotal=%4B "
                               "syncMismatch+%5(total=%6) invalidLength+%7(total=%8) "
                               "resync+%9(total=%10)")
                           .arg(dep.protocolCacheBytes)
                           .arg(dep.parsedPackets)
                           .arg(dep.restoredBytes.size())
                           .arg(dep.totalRestoredBytes)
                           .arg(dep.syncMismatchCount)
                           .arg(dep.totalSyncMismatchCount)
                           .arg(dep.invalidLengthCount)
                           .arg(dep.totalInvalidLengthCount)
                           .arg(dep.resyncCount)
                           .arg(dep.totalResyncCount));

        if (!dep.packetLengths.isEmpty()) {
            emit workerLog(QString("[PROTO] packet_lengths=%1").arg(joinIntValues(dep.packetLengths)));
        }
        if (!dep.resyncPositions.isEmpty()) {
            emit workerLog(QString("[PROTO] resync_positions=%1").arg(joinIntValues(dep.resyncPositions)));
        }

        const Yuy2FrameReassembler::BatchResult frameBatch = frameReassembler.pushBytes(dep.restoredBytes);
        emit workerLog(QString("[FRAME] cache=%1B output_frames+%2(total=%3)")
                           .arg(frameBatch.frameCacheBytes)
                           .arg(frameBatch.framesOutput)
                           .arg(frameBatch.totalFramesOutput));

        // 将完整帧逐个回调给 UI 线程。
        for (const QByteArray &frame : frameBatch.frames) {
            emit frameReady(frame, streamConfig.width, streamConfig.height);
        }

        // 可选节流：仅在实际产出完整帧时才休眠。
        if (throttleMs > 0 && frameBatch.framesOutput > 0) {
            QThread::msleep(static_cast<unsigned long>(throttleMs));
        }
    }

    // 统一资源回收出口。
    free_buffer(chunkBuffer);
    CloseHandle(c2h);
    m_running.store(false);
    emit stopped();
}

// ============================================================================
// Widget: 生命周期
// ============================================================================

Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    // 初始化 AXI-Lite 寄存器读写调试区域。
    initializeAxiLiteControls();

    // 初始状态：未接收，停止按钮禁用。
    setReceivingUiState(false);

    // 提前创建读取线程，后续点击“开始接收”只需触发 start()。
    setupReaderThread();

    appendLog("App started.");
}

Widget::~Widget()
{
    // 析构顺序遵循“先停业务，再停线程，再关句柄”。
    stopVideoDump();
    stopReaderThread();
    closeXdmaHandles();
    delete ui;
}

void Widget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_lastFrameImage.isNull()) {
        // 尺寸变化时按比例重绘最后一帧，避免预览窗口被拉伸后内容错位。
        updatePreviewImage(m_lastFrameImage);
    }
}

void Widget::initializeAxiLiteControls()
{
    // 在参数区和预览区之间插入寄存器读写行，便于联调时快速验证 AXI-Lite。
    QWidget *panel = new QWidget(this);
    panel->setObjectName("axiLiteRegPanel");

    QHBoxLayout *row = new QHBoxLayout(panel);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(8);

    QLabel *addrLabel = new QLabel(QString::fromWCharArray(L"寄存器地址:"), panel);
    m_regAddrEdit = new QLineEdit(panel);
    m_regAddrEdit->setPlaceholderText("0x00000000");
    m_regAddrEdit->setText("0x00000000");
    m_regAddrEdit->setMaximumWidth(130);

    QLabel *writeLabel = new QLabel(QString::fromWCharArray(L"写入值:"), panel);
    m_regWriteValueEdit = new QLineEdit(panel);
    m_regWriteValueEdit->setPlaceholderText("0x00000000");
    m_regWriteValueEdit->setText("0x00000000");
    m_regWriteValueEdit->setMaximumWidth(130);

    QPushButton *readBtn = new QPushButton(QString::fromWCharArray(L"读寄存器"), panel);
    QPushButton *writeBtn = new QPushButton(QString::fromWCharArray(L"写寄存器"), panel);

    QLabel *readbackLabel = new QLabel(QString::fromWCharArray(L"读回值:"), panel);
    m_regReadValueEdit = new QLineEdit(panel);
    m_regReadValueEdit->setReadOnly(true);
    m_regReadValueEdit->setText("0x00000000");
    m_regReadValueEdit->setMaximumWidth(130);

    row->addWidget(addrLabel);
    row->addWidget(m_regAddrEdit);
    row->addWidget(writeLabel);
    row->addWidget(m_regWriteValueEdit);
    row->addWidget(readBtn);
    row->addWidget(writeBtn);
    row->addWidget(readbackLabel);
    row->addWidget(m_regReadValueEdit);
    row->addStretch(1);

    connect(readBtn, &QPushButton::clicked, this, [this]() {
        // 读流程：解析地址 -> read_device(user) -> 显示结果 + 日志。
        quint32 address = 0;
        if (!parseUiRegisterValue(m_regAddrEdit ? m_regAddrEdit->text() : QString(),
                                  address,
                                  QStringLiteral("register address"))) {
            return;
        }

        quint32 value = 0;
        if (!readUserRegister(address, value)) {
            return;
        }

        if (m_regReadValueEdit) {
            m_regReadValueEdit->setText(toHex32(value));
        }

        appendLog(QString("[AXIL] READ  addr=%1 -> value=%2")
                  .arg(toHex32(address))
                  .arg(toHex32(value)));
    });

    connect(writeBtn, &QPushButton::clicked, this, [this]() {
        // 写流程：解析地址/写值 -> write_device(user) -> 日志。
        quint32 address = 0;
        quint32 value = 0;
        if (!parseUiRegisterValue(m_regAddrEdit ? m_regAddrEdit->text() : QString(),
                                  address,
                                  QStringLiteral("register address"))) {
            return;
        }
        if (!parseUiRegisterValue(m_regWriteValueEdit ? m_regWriteValueEdit->text() : QString(),
                                  value,
                                  QStringLiteral("register write value"))) {
            return;
        }

        if (!writeUserRegister(address, value)) {
            return;
        }

        appendLog(QString("[AXIL] WRITE addr=%1 <- value=%2")
                  .arg(toHex32(address))
                  .arg(toHex32(value)));
    });

    // 原布局：0=topButtonLayout, 1=paramLayout, 2=preview。
    // 插入到 index=2，使寄存器调试区位于参数区与预览区之间。
    ui->verticalLayout->insertWidget(2, panel);
}

// ============================================================================
// Widget: UI 槽函数（用户动作）
// ============================================================================

void Widget::on_btnOpenXdma_clicked()
{
    openXdmaAndSelfCheck();
}

void Widget::on_btnRunSelfTest_clicked()
{
    // 自测只依赖 stream_pipeline，不依赖 XDMA 设备是否打开。
    const VideoStreamConfig config = VideoStreamConfig::defaultYuy2();
    appendLog(QString("[SELFTEST] Start: %1").arg(config.toString()));

    const StreamPipelineSelfTestReport report = runStreamPipelineSelfTests(config);
    for (const QString &line : report.logs) {
        appendLog(QString("[SELFTEST] %1").arg(line));
    }
    appendLog(report.allPassed ? "[SELFTEST] Result: PASS" : "[SELFTEST] Result: FAIL");
}

void Widget::on_btnStartReceive_clicked()
{
    if (m_receiving) {
        return;
    }

    if (!ensureC2hChannelOpen()) {
        appendLog("[ERROR] Cannot start receive because c2h_0 is unavailable.");
        return;
    }

    // 当前接收端使用固定参数（640x360 YUY2），帧边界由固定 frameBytes 决定。
    const VideoStreamConfig streamConfig = VideoStreamConfig::defaultYuy2();
    if (!streamConfig.isValid()) {
        appendLog("[ERROR] Internal stream config is invalid.");
        return;
    }

    // UI 中 chunk(KB) 转换为字节并做边界校验。
    const qint64 chunkBytes64 = static_cast<qint64>(ui->spinChunkKB->value()) * 1024;
    if (chunkBytes64 <= 0 || chunkBytes64 > (std::numeric_limits<int>::max)()) {
        appendLog("[ERROR] Invalid chunk size.");
        return;
    }

    const int width = streamConfig.width;
    const int height = streamConfig.height;
    const int frameBytes = streamConfig.frameBytes;
    const int chunkBytes = static_cast<int>(chunkBytes64);
    const int throttleMs = ui->spinThrottleMs->value();

    // 将 UI 中宽高同步为固定配置，避免用户误以为当前读取按 UI 宽高生效。
    if (ui->spinWidth->value() != width || ui->spinHeight->value() != height) {
        appendLog(QString("[WARN] UI resolution overridden by fixed stream config: %1x%2 -> %3x%4")
                      .arg(ui->spinWidth->value())
                      .arg(ui->spinHeight->value())
                      .arg(width)
                      .arg(height));
    }
    ui->spinWidth->setValue(width);
    ui->spinHeight->setValue(height);

    if (!startVideoDump(width, height)) {
        appendLog("[ERROR] Cannot start receive because dump file is unavailable.");
        return;
    }

    m_receivedFrames = 0;
    setReceivingUiState(true);

    appendLog(QString("[INFO] Start C2H receive with fixed config: %1, chunk=%2KB(%3B), throttle=%4ms")
                  .arg(streamConfig.toString())
                  .arg(ui->spinChunkKB->value())
                  .arg(chunkBytes)
                  .arg(throttleMs));

    // 使用 QueuedConnection 保证 start() 在 worker 所在线程执行。
    const quintptr handleValue = reinterpret_cast<quintptr>(m_c2h0Handle);
    const bool invokeOk = QMetaObject::invokeMethod(m_readerWorker,
                                                     "start",
                                                     Qt::QueuedConnection,
                                                     Q_ARG(quintptr, handleValue),
                                                     Q_ARG(int, frameBytes),
                                                     Q_ARG(int, chunkBytes),
                                                     Q_ARG(int, throttleMs),
                                                     Q_ARG(int, width),
                                                     Q_ARG(int, height));
    if (!invokeOk) {
        appendLog("[ERROR] Failed to start reader worker.");
        setReceivingUiState(false);
        stopVideoDump();
    }
}

void Widget::on_btnStopReceive_clicked()
{
    if (!m_receiving) {
        return;
    }

    appendLog("[INFO] Stop requested.");

    if (m_readerWorker) {
        m_readerWorker->requestStop();
    }

    setReceivingUiState(false);
    stopVideoDump();
}

// ============================================================================
// Widget: Reader 回调
// ============================================================================

void Widget::onReaderFrameReady(const QByteArray &payload, int width, int height)
{
    if (!m_receiving) {
        return;
    }

    // 同时写入原始 yuv dump，便于离线分析。
    writeVideoFrame(payload, width, height);

    // 在线预览走 YUY2 -> RGB888 转换。
    QImage image;
    if (!yuyvToRgbImage(payload, width, height, image)) {
        appendLog("[WARN] Frame convert failed.");
        return;
    }

    updatePreviewImage(image);

    ++m_receivedFrames;
    if ((m_receivedFrames % 30) == 0) {
        appendLog(QString("[INFO] Frames displayed: %1").arg(m_receivedFrames));
    }
}

void Widget::onReaderError(int code)
{
    if (!m_receiving) {
        return;
    }

    appendLog(QString("[ERROR] C2H read failed: %1").arg(code));

    if (m_readerWorker) {
        m_readerWorker->requestStop();
    }

    setReceivingUiState(false);
    stopVideoDump();
}

void Widget::onReaderStopped()
{
    if (m_receiving) {
        setReceivingUiState(false);
    }

    stopVideoDump();
    closeXdmaHandles();
    appendLog("[INFO] Reader stopped.");
}

// ============================================================================
// Widget: 线程管理
// ============================================================================

void Widget::setupReaderThread()
{
    if (m_readerThread) {
        return;
    }

    m_readerThread = new QThread();
    m_readerWorker = new C2hReaderWorker();
    m_readerWorker->moveToThread(m_readerThread);

    // 所有跨线程信号统一用 QueuedConnection，避免直接跨线程调用。
    connect(m_readerWorker,
            &C2hReaderWorker::frameReady,
            this,
            &Widget::onReaderFrameReady,
            Qt::QueuedConnection);
    connect(m_readerWorker,
            &C2hReaderWorker::workerLog,
            this,
            &Widget::appendLog,
            Qt::QueuedConnection);
    connect(m_readerWorker,
            &C2hReaderWorker::readError,
            this,
            &Widget::onReaderError,
            Qt::QueuedConnection);
    connect(m_readerWorker,
            &C2hReaderWorker::stopped,
            this,
            &Widget::onReaderStopped,
            Qt::QueuedConnection);

    connect(m_readerThread, &QThread::finished, m_readerWorker, &QObject::deleteLater);

    m_readerThread->start();
}

void Widget::stopReaderThread()
{
    if (!m_readerThread) {
        return;
    }

    if (m_readerWorker) {
        m_readerWorker->requestStop();
    }

    m_readerThread->quit();
    if (!m_readerThread->wait(kReaderStopWaitMs)) {
        // 常规退出超时时触发兜底，避免进程退出卡死。
        qWarning() << "Reader thread did not stop in time, forcing termination.";
        m_readerThread->terminate();
        m_readerThread->wait();
    }

    delete m_readerThread;
    m_readerThread = nullptr;
    m_readerWorker = nullptr;

    // 必须在线程完全停止后关闭句柄，避免与 read_device 并发。
    closeXdmaHandles();
}

// ============================================================================
// Widget: XDMA 设备管理
// ============================================================================

bool Widget::openXdmaAndSelfCheck()
{
    closeXdmaHandles();

    constexpr int kMaxDevices = 16;
    constexpr size_t kPathLength = 260 + 1;

    // 预分配枚举缓冲：driver API 需要 char* 数组供其写入路径。
    std::vector<QByteArray> pathBuffers;
    pathBuffers.reserve(kMaxDevices);
    std::vector<char *> pathPtrs(kMaxDevices, nullptr);

    for (int i = 0; i < kMaxDevices; ++i) {
        pathBuffers.push_back(QByteArray(static_cast<int>(kPathLength), '\0'));
        pathPtrs[i] = pathBuffers[i].data();
    }

    const int deviceCount = get_devices(GUID_DEVINTERFACE_XDMA, pathPtrs.data(), kPathLength);
    appendLog(QString("XDMA devices detected: %1").arg(deviceCount));
    if (deviceCount <= 0) {
        appendLog("[ERROR] No XDMA device found.");
        return false;
    }

    // 简化策略：选第一条非空路径作为目标设备。
    int selectedIndex = -1;
    const int scanCount = qMin(deviceCount, kMaxDevices);
    for (int i = 0; i < scanCount; ++i) {
        if (pathPtrs[i] && pathPtrs[i][0] != '\0') {
            qInfo().noquote() << QString("[XDMA] device[%1]: %2").arg(i).arg(pathPtrs[i]);
            if (selectedIndex < 0) {
                selectedIndex = i;
            }
        }
    }

    if (selectedIndex < 0) {
        appendLog("[ERROR] Device path list is empty.");
        return false;
    }

    const QByteArray basePath(pathPtrs[selectedIndex]);
    if (basePath.isEmpty()) {
        appendLog("[ERROR] Invalid base path.");
        return false;
    }

    m_xdmaDevicePath = QString::fromLocal8Bit(basePath.constData());

    // 先打开 user 通道（控制/状态），再打开 c2h_0（数据接收）。
    HANDLE userHandle = nullptr;
    {
        QByteArray userPath = basePath;
        const int ok = open_devices(&userHandle,
                                    GENERIC_READ | GENERIC_WRITE,
                                    userPath.data(),
                                    XDMA_FILE_USER);
        if (ok != 1 || !isValidHandle(userHandle)) {
            appendLog("[ERROR] Open user channel failed.");
            return false;
        }
    }

    HANDLE c2hHandle = nullptr;
    {
        QByteArray c2hPath = basePath;
        const int ok = open_devices(&c2hHandle,
                                    GENERIC_READ,
                                    c2hPath.data(),
                                    XDMA_FILE_C2H_0);
        if (ok != 1 || !isValidHandle(c2hHandle)) {
            appendLog("[ERROR] Open c2h_0 channel failed.");
            CloseHandle(userHandle);
            return false;
        }
    }

    m_userHandle = reinterpret_cast<void *>(userHandle);
    m_c2h0Handle = reinterpret_cast<void *>(c2hHandle);

    // ready_state 用于快速确认链路/DDR 初始化状态。
    unsigned int opState = 0;
    unsigned int ddrState = 0;
    const int readyRet = ready_state(userHandle, &opState, &ddrState);
    if (readyRet < 0) {
        appendLog(QString("[WARN] ready_state failed: %1").arg(readyRet));
    } else {
        appendLog(QString("[OK] ready_state ret=%1, op=%2, ddr=%3")
                      .arg(readyRet)
                      .arg(opState)
                      .arg(ddrState));
    }

    appendLog("[OK] XDMA channels are ready: user + c2h_0.");
    return true;
}

bool Widget::ensureC2hChannelOpen()
{
    const HANDLE c2h = reinterpret_cast<HANDLE>(m_c2h0Handle);
    if (isValidHandle(c2h)) {
        return true;
    }

    appendLog("[INFO] c2h_0 is not open. Trying to open XDMA...");
    return openXdmaAndSelfCheck();
}

void Widget::closeXdmaHandles()
{
    const HANDLE c2h = reinterpret_cast<HANDLE>(m_c2h0Handle);
    const HANDLE user = reinterpret_cast<HANDLE>(m_userHandle);

    if (isValidHandle(c2h)) {
        CloseHandle(c2h);
    }
    if (isValidHandle(user)) {
        CloseHandle(user);
    }

    m_c2h0Handle = nullptr;
    m_userHandle = nullptr;
}

bool Widget::parseUiRegisterValue(const QString &text,
                                  quint32 &outValue,
                                  const QString &fieldName)
{
    // 支持十六进制（0x...）和十进制输入。
    const QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        appendLog(QString("[AXIL][ERROR] %1 is empty.").arg(fieldName));
        return false;
    }

    bool ok = false;
    const qulonglong raw = trimmed.toULongLong(&ok, 0);
    if (!ok || raw > 0xFFFFFFFFULL) {
        appendLog(QString("[AXIL][ERROR] Invalid %1: %2").arg(fieldName, trimmed));
        return false;
    }

    outValue = static_cast<quint32>(raw);
    return true;
}

bool Widget::readUserRegister(quint32 address, quint32 &value)
{
    // AXI-Lite 32bit 寄存器要求 4 字节地址对齐。
    if ((address & 0x3u) != 0) {
        appendLog(QString("[AXIL][ERROR] Register address must be 4-byte aligned: %1")
                  .arg(toHex32(address)));
        return false;
    }

    HANDLE user = reinterpret_cast<HANDLE>(m_userHandle);
    if (!isValidHandle(user)) {
        appendLog("[AXIL] user channel is not open. Trying auto-open...");
        if (!openXdmaAndSelfCheck()) {
            appendLog("[AXIL][ERROR] XDMA auto-open failed for register read.");
            return false;
        }
        user = reinterpret_cast<HANDLE>(m_userHandle);
    }

    if (!isValidHandle(user)) {
        appendLog("[AXIL][ERROR] user channel handle is invalid.");
        return false;
    }

    unsigned int raw = 0;
    const int ret = read_device(user,
                                static_cast<long>(address),
                                4,
                                reinterpret_cast<BYTE *>(&raw));
    if (ret != 4) {
        appendLog(QString("[AXIL][ERROR] read_device failed at %1, ret=%2")
                  .arg(toHex32(address))
                  .arg(ret));
        return false;
    }

    value = static_cast<quint32>(raw);
    return true;
}

bool Widget::writeUserRegister(quint32 address, quint32 value)
{
    // AXI-Lite 32bit 寄存器要求 4 字节地址对齐。
    if ((address & 0x3u) != 0) {
        appendLog(QString("[AXIL][ERROR] Register address must be 4-byte aligned: %1")
                  .arg(toHex32(address)));
        return false;
    }

    HANDLE user = reinterpret_cast<HANDLE>(m_userHandle);
    if (!isValidHandle(user)) {
        appendLog("[AXIL] user channel is not open. Trying auto-open...");
        if (!openXdmaAndSelfCheck()) {
            appendLog("[AXIL][ERROR] XDMA auto-open failed for register write.");
            return false;
        }
        user = reinterpret_cast<HANDLE>(m_userHandle);
    }

    if (!isValidHandle(user)) {
        appendLog("[AXIL][ERROR] user channel handle is invalid.");
        return false;
    }

    unsigned int raw = static_cast<unsigned int>(value);
    const int ret = write_device(user,
                                 static_cast<long>(address),
                                 4,
                                 reinterpret_cast<BYTE *>(&raw));
    if (ret != 4) {
        appendLog(QString("[AXIL][ERROR] write_device failed at %1, ret=%2")
                  .arg(toHex32(address))
                  .arg(ret));
        return false;
    }

    return true;
}

// ============================================================================
// Widget: 落盘与显示
// ============================================================================

bool Widget::startVideoDump(int width, int height)
{
    // 每次开始接收前都先重置旧文件，避免写入状态残留。
    stopVideoDump();

    if (width <= 0 || height <= 0) {
        appendLog("[ERROR] Cannot create dump file with invalid frame size.");
        return false;
    }

    const QString sourcePath = QString::fromLocal8Bit(__FILE__);
    const QString projectDirPath = QFileInfo(sourcePath).absolutePath();
    const QString timeTag = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    const QString fileName = QString("c2h_dump_%1_%2x%3_yuyv422.yuv")
                                 .arg(timeTag)
                                 .arg(width)
                                 .arg(height);

    QDir projectDir(projectDirPath);
    m_videoDumpPath = projectDir.filePath(fileName);
    m_videoDumpFile.setFileName(m_videoDumpPath);

    if (!m_videoDumpFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        appendLog(QString("[ERROR] Open dump file failed: %1").arg(m_videoDumpPath));
        m_videoDumpPath.clear();
        return false;
    }

    m_videoDumpWidth = width;
    m_videoDumpHeight = height;
    m_videoDumpBytes = 0;
    m_videoDumpFrames = 0;

    appendLog(QString("[INFO] Video dump started: %1").arg(m_videoDumpPath));
    appendLog("[INFO] Dump format: raw YUYV422 (.yuv)");
    return true;
}

void Widget::stopVideoDump()
{
    if (!m_videoDumpFile.isOpen()) {
        return;
    }

    m_videoDumpFile.flush();
    m_videoDumpFile.close();

    appendLog(QString("[INFO] Video dump saved: %1, frames=%2, bytes=%3")
                  .arg(m_videoDumpPath)
                  .arg(m_videoDumpFrames)
                  .arg(m_videoDumpBytes));

    m_videoDumpPath.clear();
    m_videoDumpWidth = 0;
    m_videoDumpHeight = 0;
    m_videoDumpBytes = 0;
    m_videoDumpFrames = 0;
}

void Widget::writeVideoFrame(const QByteArray &payload, int width, int height)
{
    if (!m_videoDumpFile.isOpen()) {
        return;
    }

    // 防止运行中分辨率变更导致同一文件混入不同尺寸帧。
    if (width != m_videoDumpWidth || height != m_videoDumpHeight) {
        appendLog(QString("[WARN] Dump stopped due to frame size change: %1x%2 -> %3x%4")
                      .arg(m_videoDumpWidth)
                      .arg(m_videoDumpHeight)
                      .arg(width)
                      .arg(height));
        stopVideoDump();
        return;
    }

    const qint64 written = m_videoDumpFile.write(payload);
    if (written != payload.size()) {
        appendLog("[ERROR] Dump write failed, stop dumping.");
        stopVideoDump();
        return;
    }

    ++m_videoDumpFrames;
    m_videoDumpBytes += written;
}

bool Widget::yuyvToRgbImage(const QByteArray &payload, int width, int height, QImage &outImage) const
{
    if (width <= 0 || height <= 0) {
        return false;
    }

    const int rowStride = width * 2;
    if (payload.size() < rowStride * height) {
        return false;
    }

    QImage image(width, height, QImage::Format_RGB888);
    if (image.isNull()) {
        return false;
    }

    const uchar *src = reinterpret_cast<const uchar *>(payload.constData());
    for (int y = 0; y < height; ++y) {
        const uchar *row = src + y * rowStride;
        uchar *dst = image.scanLine(y);

        // YUYV 每 4 字节描述 2 个像素：Y0 U Y1 V。
        // 两个像素共享同一组 U/V 色度分量。
        for (int x = 0; x < width; x += 2) {
            const int y0 = row[x * 2 + 0];
            const int u = row[x * 2 + 1] - 128;
            const int y1 = row[x * 2 + 2];
            const int v = row[x * 2 + 3] - 128;

            // 整数近似 YUV->RGB，减少浮点开销。
            const int rAdd = (359 * v) >> 8;
            const int gAdd = ((88 * u) + (183 * v)) >> 8;
            const int bAdd = (454 * u) >> 8;

            dst[x * 3 + 0] = static_cast<uchar>(clampToByte(y0 + rAdd));
            dst[x * 3 + 1] = static_cast<uchar>(clampToByte(y0 - gAdd));
            dst[x * 3 + 2] = static_cast<uchar>(clampToByte(y0 + bAdd));

            if (x + 1 < width) {
                dst[(x + 1) * 3 + 0] = static_cast<uchar>(clampToByte(y1 + rAdd));
                dst[(x + 1) * 3 + 1] = static_cast<uchar>(clampToByte(y1 - gAdd));
                dst[(x + 1) * 3 + 2] = static_cast<uchar>(clampToByte(y1 + bAdd));
            }
        }
    }

    outImage = image;
    return true;
}

void Widget::updatePreviewImage(const QImage &image)
{
    if (!ui || !ui->labelPreview) {
        return;
    }

    m_lastFrameImage = image;
    const QPixmap px = QPixmap::fromImage(image);
    ui->labelPreview->setPixmap(px.scaled(ui->labelPreview->size(),
                                          Qt::KeepAspectRatio,
                                          Qt::SmoothTransformation));
}

// ============================================================================
// Widget: UI 状态与日志
// ============================================================================

void Widget::setReceivingUiState(bool receiving)
{
    m_receiving = receiving;
    ui->btnStartReceive->setEnabled(!receiving);
    ui->btnStopReceive->setEnabled(receiving);
}

void Widget::appendLog(const QString &text)
{
    if (!ui || !ui->plainTextEdit) {
        return;
    }

    const QString stamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    ui->plainTextEdit->appendPlainText(QString("[%1] %2").arg(stamp, text));
}
