#ifndef WIDGET_H
#define WIDGET_H

#include <QByteArray>
#include <QFile>
#include <QImage>
#include <QObject>
#include <QString>
#include <QWidget>

#include <atomic>

QT_BEGIN_NAMESPACE
namespace Ui {
class Widget;
}
QT_END_NAMESPACE

class QResizeEvent;
class QThread;
class QLineEdit;

/**
 * @brief C2hReaderWorker
 *
 * 该对象运行在独立线程中，负责执行“阻塞式读取 + 协议解包 + 固定帧长重组”。
 *
 * 线程模型：
 * - 通过 Widget::setupReaderThread() 创建并 moveToThread() 到后台线程。
 * - start() 通过 Qt::QueuedConnection 异步触发，避免 UI 线程阻塞。
 * - requestStop() 只修改原子标志，读取循环在下一个检查点退出。
 */
class C2hReaderWorker : public QObject
{
    Q_OBJECT

public:
    explicit C2hReaderWorker(QObject *parent = nullptr);

    /**
     * @brief requestStop 线程安全停止请求
     *
     * 仅设置 m_running=false，不做耗时等待，也不在此关闭句柄。
     * 真正资源回收在 start() 退出路径内执行，保证顺序一致。
     */
    void requestStop();

public slots:
    /**
     * @brief start 后台读取主流程
     * @param c2hHandleValue UI 线程传入的 HANDLE（以整数封装）
     * @param frameBytes 单帧目标字节数（当前默认 640*360*2=460800）
     * @param chunkBytes 每次 read_device 请求字节数
     * @param throttleMs 每次输出完整帧后可选节流（毫秒）
     * @param width 帧宽（像素）
     * @param height 帧高（像素）
     *
     * 处理链路：
     *   read_device(任意分段) -> StreamDepacketizer -> Yuy2FrameReassembler -> frameReady
     */
    void start(quintptr c2hHandleValue,
               int frameBytes,
               int chunkBytes,
               int throttleMs,
               int width,
               int height);

signals:
    /**
     * @brief frameReady 输出完整原始帧
     *
     * payload 为完整 YUY2/YUYV 原始帧字节，不包含协议头/补零。
     */
    void frameReady(const QByteArray &payload, int width, int height);

    /**
     * @brief workerLog 后台日志透传到 UI
     */
    void workerLog(const QString &text);

    /**
     * @brief readError 底层读取错误码
     */
    void readError(int code);

    /**
     * @brief stopped worker 生命周期结束通知
     */
    void stopped();

private:
    // 读取循环运行标志；atomic 用于 UI/worker 跨线程协作。
    std::atomic_bool m_running{false};
};

/**
 * @brief Widget 主界面控制器
 *
 * 负责串联四类职责：
 * 1) XDMA 设备打开/关闭与基本自检
 * 2) 后台读取线程生命周期管理
 * 3) 预览显示（YUY2 -> RGB）
 * 4) 原始帧落盘与日志展示
 */
class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = nullptr);
    ~Widget() override;

protected:
    /**
     * @brief resizeEvent 窗口缩放时重绘最后一帧
     */
    void resizeEvent(QResizeEvent *event) override;

private slots:
    // -------------------- UI 动作 --------------------

    /**
     * @brief 打开 XDMA 并执行 ready_state 自检
     */
    void on_btnOpenXdma_clicked();

    /**
     * @brief 执行协议/组帧自测（与 XDMA 物理链路解耦）
     */
    void on_btnRunSelfTest_clicked();

    /**
     * @brief 开始接收 C2H 流
     */
    void on_btnStartReceive_clicked();

    /**
     * @brief 停止接收
     */
    void on_btnStopReceive_clicked();

    // -------------------- Reader 回调 --------------------

    /**
     * @brief 收到完整帧回调
     */
    void onReaderFrameReady(const QByteArray &payload, int width, int height);

    /**
     * @brief 读取错误回调
     */
    void onReaderError(int code);

    /**
     * @brief 后台读取线程停止回调
     */
    void onReaderStopped();

private:
    // -------------------- UI / 状态 --------------------

    /**
     * @brief 统一刷新“开始/停止”按钮状态与 m_receiving 标志
     */
    void setReceivingUiState(bool receiving);

    /**
     * @brief 写入带时间戳日志
     */
    void appendLog(const QString &text);

    /**
     * @brief 初始化 AXI-Lite 寄存器读写控件
     *
     * 在主界面动态插入“地址/写值输入 + 读写按钮 + 读回显示”调试面板，
     * 与现有 C2H 接收链路解耦，不影响数据接收流程。
     */
    void initializeAxiLiteControls();

    /**
     * @brief 将图像缩放后绘制到 preview 区域
     */
    void updatePreviewImage(const QImage &image);

    /**
     * @brief 将 YUY2 原始帧转换为 RGB888 图像
     * @return true 转换成功；false 参数非法或数据不足
     */
    bool yuyvToRgbImage(const QByteArray &payload, int width, int height, QImage &outImage) const;

    // -------------------- 线程 --------------------

    /**
     * @brief 创建并启动后台读取线程
     */
    void setupReaderThread();

    /**
     * @brief 停止并回收后台读取线程
     */
    void stopReaderThread();

    // -------------------- XDMA 设备 --------------------

    /**
     * @brief 枚举设备并打开 user/c2h_0 通道，随后执行 ready_state 自检
     */
    bool openXdmaAndSelfCheck();

    /**
     * @brief 保证 c2h_0 可用，不可用时自动尝试重连
     */
    bool ensureC2hChannelOpen();

    /**
     * @brief 关闭并清空所有 XDMA 句柄
     */
    void closeXdmaHandles();

    /**
     * @brief 解析寄存器地址/寄存器数值输入
     * @param text UI 文本（支持 0x 前缀十六进制，或十进制）
     * @param outValue 解析后的 32bit 无符号值
     * @param fieldName 字段名（用于日志提示）
     * @return true 解析成功；false 输入为空、格式非法或超出 uint32 范围
     */
    bool parseUiRegisterValue(const QString &text, quint32 &outValue, const QString &fieldName);

    /**
     * @brief 通过 user 通道读取 AXI-Lite 32bit 寄存器
     * @param address 寄存器地址（必须 4 字节对齐）
     * @param value 读回值
     * @return true 读取成功；false 地址非法、通道不可用或底层 read_device 失败
     */
    bool readUserRegister(quint32 address, quint32 &value);

    /**
     * @brief 通过 user 通道写 AXI-Lite 32bit 寄存器
     * @param address 寄存器地址（必须 4 字节对齐）
     * @param value 写入值
     * @return true 写入成功；false 地址非法、通道不可用或底层 write_device 失败
     */
    bool writeUserRegister(quint32 address, quint32 value);

    // -------------------- 原始视频落盘 --------------------

    /**
     * @brief 创建新的 .yuv 落盘文件并重置统计
     */
    bool startVideoDump(int width, int height);

    /**
     * @brief 停止落盘并打印最终统计
     */
    void stopVideoDump();

    /**
     * @brief 追加写入单帧原始数据
     */
    void writeVideoFrame(const QByteArray &payload, int width, int height);

private:
    Ui::Widget *ui = nullptr;

    // -------------------- XDMA 连接状态 --------------------

    QString m_xdmaDevicePath;
    void *m_userHandle = nullptr;
    void *m_c2h0Handle = nullptr;

    // -------------------- 后台读取线程 --------------------

    QThread *m_readerThread = nullptr;
    C2hReaderWorker *m_readerWorker = nullptr;

    // -------------------- 预览状态 --------------------

    bool m_receiving = false;
    int m_receivedFrames = 0;
    QImage m_lastFrameImage;

    // -------------------- AXI-Lite 寄存器调试控件 --------------------

    // 寄存器地址输入（支持 0x.. 或十进制）。
    QLineEdit *m_regAddrEdit = nullptr;
    // 写寄存器输入值。
    QLineEdit *m_regWriteValueEdit = nullptr;
    // 读寄存器结果显示（只读）。
    QLineEdit *m_regReadValueEdit = nullptr;

    // -------------------- 落盘状态 --------------------

    QFile m_videoDumpFile;
    QString m_videoDumpPath;
    int m_videoDumpWidth = 0;
    int m_videoDumpHeight = 0;
    qint64 m_videoDumpBytes = 0;
    int m_videoDumpFrames = 0;
};

#endif // WIDGET_H
