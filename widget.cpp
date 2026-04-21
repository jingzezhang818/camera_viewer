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

#include <limits>
#include <vector>
#include <cstring>

#include "xdmaDLL_public.h"

namespace {

// 判断Windows句柄是否可用，统一过滤空句柄与INVALID_HANDLE_VALUE。
bool isValidHandle(HANDLE handle)
{
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}

// 将整数值限制在8bit像素范围[0, 255]，避免颜色分量溢出。
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

} // namespace

// 构造函数：仅完成QObject初始化，实际读取逻辑由start()触发。
C2hReaderWorker::C2hReaderWorker(QObject *parent)
    : QObject(parent)
{
}

// 对外暴露的停止接口：将运行标志置为false，读取循环会尽快退出。
void C2hReaderWorker::requestStop()
{
    m_running.store(false);
}

// 线程主函数：持续从c2h读取并拼装完整帧，再通过信号发送给UI线程。
void C2hReaderWorker::start(quintptr c2hHandleValue,
                            int frameBytes,
                            int chunkBytes,
                            int throttleMs,
                            int width,
                            int height)
{
    // 防止重复启动同一个worker。
    if (m_running.exchange(true)) {
        emit readError(-2001);
        return;
    }

    // 校验句柄与帧大小参数，异常时直接结束。
    const HANDLE c2h = reinterpret_cast<HANDLE>(c2hHandleValue);
    if (!isValidHandle(c2h) || frameBytes <= 0) {
        m_running.store(false);
        emit readError(-2002);
        emit stopped();
        return;
    }

    // 块大小限制在[4KB, frameBytes]，兼顾IO调用次数与内存占用。
    const int safeChunkBytes = qBound(4 * 1024, chunkBytes, frameBytes);
    BYTE *chunkBuffer = allocate_buffer(static_cast<size_t>(safeChunkBytes), 0);
    if (!chunkBuffer) {
        m_running.store(false);
        emit readError(-2003);
        emit stopped();
        return;
    }

    // 外层循环按“帧”为单位工作。
    while (m_running.load()) {
        QByteArray framePayload(frameBytes, '\0');
        int offset = 0;
        bool failed = false;

        // 内层循环按“块”读取，直到凑满一帧或出现错误。
        while (offset < frameBytes && m_running.load()) {
            const int req = qMin(safeChunkBytes, frameBytes - offset);
            const int ret = read_device(c2h, 0x00000000, static_cast<DWORD>(req), chunkBuffer);
            if (!m_running.load()) {
                break;
            }
            if (ret <= 0) {
                emit readError(ret);
                failed = true;
                break;
            }
            std::memcpy(framePayload.data() + offset, chunkBuffer, static_cast<size_t>(ret));
            offset += ret;
        }

        if (!m_running.load()) {
            break;
        }

        if (failed) {
            // 读失败后短暂退避，避免高频错误刷屏。
            QThread::msleep(5);
            continue;
        }

        // 将完整帧上报给UI线程做显示与落盘。
        emit frameReady(framePayload, width, height);

        if (throttleMs > 0) {
            // 可选节流：用于限制显示/落盘速率。
            QThread::msleep(static_cast<unsigned long>(throttleMs));
        }
    }

    // 退出前释放缓冲区并发送停止信号。
    free_buffer(chunkBuffer);
    m_running.store(false);
    emit stopped();
}

// 主窗口初始化：构建UI、设置按钮初始状态、启动读取线程基础设施。
Widget::Widget(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget)
{
    ui->setupUi(this);

    ui->btnStopReceive->setEnabled(false);
    ui->btnStartReceive->setEnabled(true);

    setupReaderThread();
    appendLog("App started.");
}

// 析构顺序：先停落盘，再停线程，再关句柄，最后销毁UI。
Widget::~Widget()
{
    stopVideoDump();
    stopReaderThread();
    closeXdmaHandles();
    delete ui;
}

// 窗口尺寸变化时重绘预览，保持画面按比例缩放。
void Widget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (!m_lastFrameImage.isNull()) {
        updatePreviewImage(m_lastFrameImage);
    }
}

// 创建后台读取线程并建立跨线程信号连接。
void Widget::setupReaderThread()
{
    if (m_readerThread) {
        return;
    }

    m_readerThread = new QThread();
    m_readerWorker = new C2hReaderWorker();
    m_readerWorker->moveToThread(m_readerThread);

    connect(m_readerWorker, &C2hReaderWorker::frameReady,
            this, &Widget::onReaderFrameReady, Qt::QueuedConnection);
    connect(m_readerWorker, &C2hReaderWorker::readError,
            this, &Widget::onReaderError, Qt::QueuedConnection);
    connect(m_readerWorker, &C2hReaderWorker::stopped,
            this, &Widget::onReaderStopped, Qt::QueuedConnection);

    connect(m_readerThread, &QThread::finished,
            m_readerWorker, &QObject::deleteLater);

    m_readerThread->start();
}

// 停止并回收读取线程。
void Widget::stopReaderThread()
{
    if (!m_readerThread) {
        return;
    }

    if (m_readerWorker) {
        m_readerWorker->requestStop();
    }

    closeXdmaHandles();

    m_readerThread->quit();
    if (!m_readerThread->wait(2000)) {
        qWarning() << "Reader thread did not stop in time.";
    }

    delete m_readerThread;
    m_readerThread = nullptr;
    m_readerWorker = nullptr;
}

// 向日志文本框写入带毫秒时间戳的消息。
void Widget::appendLog(const QString &text)
{
    if (!ui || !ui->plainTextEdit) {
        return;
    }

    const QString stamp = QDateTime::currentDateTime().toString("hh:mm:ss.zzz");
    ui->plainTextEdit->appendPlainText(QString("[%1] %2").arg(stamp, text));
}

// 开始原始数据落盘：按时间戳创建.yuv文件并重置统计信息。
bool Widget::startVideoDump(int width, int height)
{
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

// 结束落盘并输出最终统计。
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

// 写入单帧原始数据；若帧尺寸变化则停止落盘避免文件格式混乱。
void Widget::writeVideoFrame(const QByteArray &payload, int width, int height)
{
    if (!m_videoDumpFile.isOpen()) {
        return;
    }

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

void Widget::closeXdmaHandles()
{
    // 统一关闭并清空句柄，避免悬挂句柄导致后续重连失败。
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

// 扫描XDMA设备，打开user和c2h_0通道，并执行ready_state连通性检查。
bool Widget::openXdmaAndSelfCheck()
{
    closeXdmaHandles();

    // 预分配设备路径缓冲区，供驱动层填充设备路径字符串。
    constexpr int kMaxDevices = 16;
    constexpr size_t kPathLength = 1024;

    std::vector<QByteArray> pathBuffers;
    pathBuffers.reserve(kMaxDevices);
    std::vector<char *> pathPtrs(kMaxDevices, nullptr);
    for (int i = 0; i < kMaxDevices; ++i) {
        pathBuffers.push_back(QByteArray(static_cast<int>(kPathLength), '\0'));
        pathPtrs[i] = pathBuffers[i].data();
    }

    // 通过GUID枚举系统中的XDMA设备。
    const int deviceCount = get_devices(GUID_DEVINTERFACE_XDMA, pathPtrs.data(), kPathLength);
    appendLog(QString("XDMA devices detected: %1").arg(deviceCount));
    if (deviceCount <= 0) {
        appendLog("[ERROR] No XDMA device found.");
        return false;
    }

    // 简化策略：选中扫描到的第一个有效设备路径。
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

    const QByteArray basePath = QByteArray(pathPtrs[selectedIndex]);
    if (basePath.isEmpty()) {
        appendLog("[ERROR] Invalid base path.");
        return false;
    }

    m_xdmaDevicePath = QString::fromLocal8Bit(basePath.constData());

    HANDLE userHandle = nullptr;
    {
        // 先打开user通道，用于状态/控制接口访问。
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
        // 再打开c2h_0通道，用于持续接收上行数据。
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

    // 读取设备就绪状态，便于快速判断链路/DDR初始化是否正常。
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

// 若c2h句柄无效则自动尝试重连。
bool Widget::ensureC2hChannelOpen()
{
    const HANDLE c2h = reinterpret_cast<HANDLE>(m_c2h0Handle);
    if (isValidHandle(c2h)) {
        return true;
    }

    appendLog("[INFO] c2h_0 is not open. Trying to open XDMA...");
    return openXdmaAndSelfCheck();
}

// 手动点击“打开XDMA”按钮时执行设备连接与自检。
void Widget::on_btnOpenXdma_clicked()
{
    openXdmaAndSelfCheck();
}

// 开始接收按钮：校验参数、启动落盘、异步触发worker读取。
void Widget::on_btnStartReceive_clicked()
{
    if (m_receiving) {
        return;
    }

    if (!ensureC2hChannelOpen()) {
        appendLog("[ERROR] Cannot start receive because c2h_0 is unavailable.");
        return;
    }

    const int width = ui->spinWidth->value();
    const int height = ui->spinHeight->value();
    const int throttleMs = ui->spinThrottleMs->value();
    const int chunkBytes = ui->spinChunkKB->value() * 1024;

    // YUYV422每像素2字节，先用64位计算避免中间溢出。
    const qint64 frameBytes64 = static_cast<qint64>(width) * static_cast<qint64>(height) * 2;
    if (frameBytes64 <= 0 || frameBytes64 > (std::numeric_limits<int>::max)()) {
        appendLog("[ERROR] Invalid frame size settings.");
        return;
    }

    const int frameBytes = static_cast<int>(frameBytes64);

    if (!startVideoDump(width, height)) {
        appendLog("[ERROR] Cannot start receive because dump file is unavailable.");
        return;
    }

    m_receiving = true;
    m_receivedFrames = 0;

    ui->btnStartReceive->setEnabled(false);
    ui->btnStopReceive->setEnabled(true);

    appendLog(QString("[INFO] Start C2H receive: %1x%2, frameBytes=%3, chunk=%4KB, throttle=%5ms")
              .arg(width)
              .arg(height)
              .arg(frameBytes)
              .arg(ui->spinChunkKB->value())
              .arg(throttleMs));

    // 通过队列连接将start调用投递到worker所属线程执行。
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
        m_receiving = false;
        ui->btnStartReceive->setEnabled(true);
        ui->btnStopReceive->setEnabled(false);
        stopVideoDump();
    }
}

// 停止接收按钮：更新UI状态并请求worker退出。
void Widget::on_btnStopReceive_clicked()
{
    if (!m_receiving) {
        return;
    }

    m_receiving = false;
    ui->btnStartReceive->setEnabled(true);
    ui->btnStopReceive->setEnabled(false);

    appendLog("[INFO] Stop requested.");

    if (m_readerWorker) {
        m_readerWorker->requestStop();
    }

    closeXdmaHandles();
    stopVideoDump();
}

// 收到完整帧后：先落盘，再转RGB，再刷新预览。
void Widget::onReaderFrameReady(const QByteArray &payload, int width, int height)
{
    if (!m_receiving) {
        return;
    }

    writeVideoFrame(payload, width, height);

    QImage image;
    if (!yuyvToRgbImage(payload, width, height, image)) {
        appendLog("[WARN] Frame convert failed.");
        return;
    }

    updatePreviewImage(image);

    ++m_receivedFrames;
    if ((m_receivedFrames % 30) == 0) {
        // 每30帧打一条统计日志，避免日志过于频繁。
        appendLog(QString("[INFO] Frames displayed: %1").arg(m_receivedFrames));
    }
}

// 读取错误回调：停止接收并回收相关资源。
void Widget::onReaderError(int code)
{
    if (!m_receiving) {
        return;
    }

    appendLog(QString("[ERROR] C2H read failed: %1").arg(code));

    m_receiving = false;
    ui->btnStartReceive->setEnabled(true);
    ui->btnStopReceive->setEnabled(false);

    if (m_readerWorker) {
        m_readerWorker->requestStop();
    }

    closeXdmaHandles();
    stopVideoDump();
}

// worker停止回调：统一修正状态并输出日志。
void Widget::onReaderStopped()
{
    if (m_receiving) {
        m_receiving = false;
        ui->btnStartReceive->setEnabled(true);
        ui->btnStopReceive->setEnabled(false);
    }

    stopVideoDump();
    appendLog("[INFO] Reader stopped.");
}

// 将YUYV422原始帧转换为QImage::Format_RGB888，便于Qt控件显示。
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

        // YUYV按2像素打包：Y0 U Y1 V，共享同一组U/V色度分量。
        for (int x = 0; x < width; x += 2) {
            const int y0 = row[x * 2 + 0];
            const int u  = row[x * 2 + 1] - 128;
            const int y1 = row[x * 2 + 2];
            const int v  = row[x * 2 + 3] - 128;

            // 使用整数近似公式完成YUV->RGB变换，避免浮点开销。
            const int rAdd = (359 * v) >> 8;
            const int gAdd = ((88 * u) + (183 * v)) >> 8;
            const int bAdd = (454 * u) >> 8;

            const int r0 = clampToByte(y0 + rAdd);
            const int g0 = clampToByte(y0 - gAdd);
            const int b0 = clampToByte(y0 + bAdd);

            dst[x * 3 + 0] = static_cast<uchar>(r0);
            dst[x * 3 + 1] = static_cast<uchar>(g0);
            dst[x * 3 + 2] = static_cast<uchar>(b0);

            if (x + 1 < width) {
                const int r1 = clampToByte(y1 + rAdd);
                const int g1 = clampToByte(y1 - gAdd);
                const int b1 = clampToByte(y1 + bAdd);

                dst[(x + 1) * 3 + 0] = static_cast<uchar>(r1);
                dst[(x + 1) * 3 + 1] = static_cast<uchar>(g1);
                dst[(x + 1) * 3 + 2] = static_cast<uchar>(b1);
            }
        }
    }

    outImage = image;
    return true;
}

// 更新预览控件：缓存最后一帧，并按控件尺寸平滑缩放显示。
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
