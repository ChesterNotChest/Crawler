#ifndef AI_TRANSFER_TASK_H
#define AI_TRANSFER_TASK_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QVector>
#include <QMap>
#include <QVariant>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QMessageBox>
#include <QDialog>
#include <QVBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QProgressBar>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QDebug>

/**
 * @brief AITransferTask - AI数据传输任务
 * 负责处理C++前端与Python后端之间的所有数据传输
 * 包括：数据库数据发送到AI、聊天消息传输、知识库更新等
 */
class AITransferTask : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 构造函数
     * @param parent 父对象
     */
    explicit AITransferTask(QObject *parent = nullptr);

    /**
     * @brief 析构函数
     */
    ~AITransferTask();

    /**
     * @brief 设置Python服务器URL
     * @param url 服务器URL
     */
    void setPythonServerUrl(const QString &url);

    /**
     * @brief 发送聊天消息到AI后端
     * @param message 消息内容
     * @param callback 回调函数(处理AI回复)
     */
    void sendChatMessage(const QString &message, std::function<void(const QString&)> callback = nullptr);

    /**
     * @brief 从数据库获取职位数据并发送到AI后端更新知识库
     * @param parentWidget 父窗口(用于显示进度对话框)
     * @param callback 回调函数(处理发送结果)
     */
    void updateKnowledgeBaseFromDatabase(QWidget *parentWidget, std::function<void(bool, const QString&)> callback = nullptr);

    /**
     * @brief 获取数据库路径
     * @return 数据库文件路径
     */
    QString getDatabasePath() const;

    /**
     * @brief 检查任务是否正在运行
     * @return 是否正在运行
     */
    bool isRunning() const;

    /**
     * @brief 取消当前操作
     */
    void cancelOperation();

signals:
    /**
     * @brief 发送完成信号
     * @param success 是否成功
     * @param message 结果消息
     */
    void transferCompleted(bool success, const QString &message);

    /**
     * @brief 进度更新信号
     * @param current 当前进度
     * @param total 总进度
     * @param status 状态描述
     */
    void progressUpdated(int current, int total, const QString &status);

private slots:
    /**
     * @brief 处理聊天请求完成
     */
    void onChatReplyFinished();

    /**
     * @brief 处理知识库更新请求完成
     */
    void onKnowledgeBaseReplyFinished();

    /**
     * @brief 处理网络错误
     */
    void onNetworkError(const QString &errorDetails = "");

    /**
     * @brief 处理单个数据项完成
     */
    void onSingleDataReplyFinished();

    /**
     * @brief 处理单个数据项网络错误
     */
    void handleSingleDataError();

private:
    /**
     * @brief 连接到数据库
     * @return 连接是否成功
     */
    bool connectToDatabase();

    /**
     * @brief 断开数据库连接
     */
    void disconnectFromDatabase();

    /**
     * @brief 从数据库获取职位数据
     * @return 职位数据向量
     */
    QVector<QMap<QString, QVariant>> fetchJobDataFromDatabase();

    /**
     * @brief 格式化职位数据为API要求的JSON格式（批量处理）
     * @param jobData 原始职位数据
     * @return 格式化后的JSON数组
     */
    QJsonArray formatJobDataForAPI(const QVector<QMap<QString, QVariant>>& jobData);

    /**
     * @brief 格式化职位数据为单个处理格式
     * @param jobData 原始职位数据
     * @return 格式化后的数据向量
     */
    QVector<QMap<QString, QVariant>> formatJobDataForSingleProcessing(const QVector<QMap<QString, QVariant>>& jobData);

    /**
     * @brief 开始单个数据处理循环
     * @param formattedData 格式化后的数据
     */
    void startSingleDataProcessingLoop(const QVector<QMap<QString, QVariant>>& formattedData);

    /**
     * @brief 处理下一个数据项
     */
    void processNextDataItem();

    /**
     * @brief 发送单个数据项到Python后端
     * @param dataItem 要发送的数据项
     */
    void sendSingleDataItemToPython(const QMap<QString, QVariant>& dataItem);

    /**
     * @brief 显示进度对话框
     * @param title 对话框标题
     * @param message 对话框消息
     */
    void showProgressDialog(const QString &title, const QString &message);

    /**
     * @brief 隐藏进度对话框
     */
    void hideProgressDialog();

    /**
     * @brief 验证JSON响应
     * @param data 响应数据
     * @return JSON是否有效
     */
    bool validateJSON(const QByteArray &data);

    /**
     * @brief 更新进度对话框消息
     * @param message 要显示的消息
     */
    void updateProgressMessage(const QString &message);

private:
    QNetworkAccessManager *networkManager;      /**< 网络管理器 */
    QString pythonServerUrl;                    /**< Python服务器URL */
    QDialog *progressDialog;                    /**< 进度对话框 */
    QLabel *progressLabel;                      /**< 进度标签 */
    QTextEdit *progressTextEdit;                /**< 进度文本编辑框，用于显示日志 */
    QProgressBar *progressBar;                  /**< 进度条 */
    void onNetworkError(QNetworkReply::NetworkError error);

    // 回调函数存储
    std::function<void(const QString&)> chatCallback;           /**< 聊天回调函数 */
    std::function<void(bool, const QString&)> kbCallback;       /**< 知识库更新回调函数 */
    QNetworkReply *currentReply;                                 /**< 当前网络回复 */
    bool operationCancelled;                                     /**< 操作取消标志 */

    // 逐个数据处理相关变量
    QVector<QMap<QString, QVariant>> pendingDataItems;          /**< 待处理的数据项 */
    int currentDataIndex;                                        /**< 当前处理的索引 */
    int totalDataItems;                                          /**< 总数据项数 */
    int successCount;                                            /**< 成功处理计数 */
    int skippedCount;                                            /**< 跳过计数 */
    int errorCount;                                              /**< 错误计数 */
};

#endif // AI_TRANSFER_TASK_H
