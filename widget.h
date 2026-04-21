#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QObject>
#include <QByteArray>
#include <QImage>
#include <QFile>

#include <atomic>

QT_BEGIN_NAMESPACE
namespace Ui { class Widget; }
QT_END_NAMESPACE

class QThread;
class QResizeEvent;

class C2hReaderWorker : public QObject
{
    Q_OBJECT

public:
    // 构造读取工作对象。该对象会被移动到独立线程执行阻塞式读取。
    explicit C2hReaderWorker(QObject *parent = nullptr);

    // 由主线程请求停止读取循环（线程安全）。
    void requestStop();

public slots:
    // 启动读取流程：从c2h句柄持续读取固定大小帧并通过信号上报。
    // c2hHandleValue：以整型传递的HANDLE值，便于跨线程调用。
    // frameBytes：单帧总字节数（例如YUYV422为width*height*2）。
    // chunkBytes：每次底层读取的块大小。
    // throttleMs：帧间节流时间，0表示不主动休眠。
    // width/height：帧尺寸，随frameReady一起回传给UI侧。
    void start(quintptr c2hHandleValue,
               int frameBytes,
               int chunkBytes,
               int throttleMs,
               int width,
               int height);

signals:
    // 成功拼好一帧后发出，payload为原始YUYV数据。
    void frameReady(const QByteArray &payload, int width, int height);

    // 底层读取失败时发出错误码。
    void readError(int code);

    // 读取循环退出时发出（无论正常停止或异常退出）。
    void stopped();

private:
    // 读取循环运行标记，使用原子变量避免线程竞争。
    std::atomic_bool m_running{false};
};

class Widget : public QWidget
{
    Q_OBJECT

public:
    // 主窗口：负责XDMA连接、数据接收、预览显示与原始数据落盘。
    explicit Widget(QWidget *parent = nullptr);

    // 析构时保证线程停止、文件关闭、句柄释放。
    ~Widget();

protected:
    // 窗口尺寸变化时，按新尺寸重新缩放最后一帧预览图。
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // UI按钮槽：打开XDMA并进行基本自检。
    void on_btnOpenXdma_clicked();

    // UI按钮槽：开始接收并显示视频流。
    void on_btnStartReceive_clicked();

    // UI按钮槽：停止接收。
    void on_btnStopReceive_clicked();

    // 工作线程回调：收到完整帧数据。
    void onReaderFrameReady(const QByteArray &payload, int width, int height);

    // 工作线程回调：读取错误。
    void onReaderError(int code);

    // 工作线程回调：读取任务停止。
    void onReaderStopped();

private:
    // 创建并启动读取线程，将worker移动到该线程。
    void setupReaderThread();

    // 请求停止并回收读取线程资源。
    void stopReaderThread();

    // 扫描设备、打开user/c2h通道并执行ready_state自检。
    bool openXdmaAndSelfCheck();

    // 确保c2h通道可用；不可用时尝试重新打开。
    bool ensureC2hChannelOpen();

    // 关闭并清空所有XDMA句柄。
    void closeXdmaHandles();

    // 将单帧YUYV422原始数据转换为RGB888图像。
    bool yuyvToRgbImage(const QByteArray &payload, int width, int height, QImage &outImage) const;

    // 将图像按比例缩放后显示到预览控件。
    void updatePreviewImage(const QImage &image);

    // 在日志窗口追加带时间戳的文本。
    void appendLog(const QString &text);

    // 初始化原始视频落盘文件（.yuv）。
    bool startVideoDump(int width, int height);

    // 结束落盘并输出统计信息。
    void stopVideoDump();

    // 追加写入一帧原始数据到dump文件。
    void writeVideoFrame(const QByteArray &payload, int width, int height);

private:
    // Qt Designer生成的界面对象。
    Ui::Widget *ui;

    // 设备基础路径（用于日志和状态跟踪）。
    QString m_xdmaDevicePath;

    // XDMA user通道句柄（寄存器/控制访问）。
    void *m_userHandle = nullptr;

    // XDMA c2h_0通道句柄（板卡->主机数据接收）。
    void *m_c2h0Handle = nullptr;

    // 读取线程对象。
    QThread *m_readerThread = nullptr;

    // 在线程内运行的读取worker。
    C2hReaderWorker *m_readerWorker = nullptr;

    // 当前是否处于接收状态。
    bool m_receiving = false;

    // 已接收并显示的帧计数。
    int m_receivedFrames = 0;

    // 最近一帧图像（用于窗口resize后重绘）。
    QImage m_lastFrameImage;

    // 原始视频落盘文件句柄。
    QFile m_videoDumpFile;

    // 落盘文件路径。
    QString m_videoDumpPath;

    // 当前落盘分辨率（用于校验尺寸一致性）。
    int m_videoDumpWidth = 0;
    int m_videoDumpHeight = 0;

    // 累计写入字节数与帧数。
    qint64 m_videoDumpBytes = 0;
    int m_videoDumpFrames = 0;
};

#endif // WIDGET_H
