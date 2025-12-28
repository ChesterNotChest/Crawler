#include "ai_transfer_task.h"
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QProgressDialog>
#include <QTimer>
#include <QDir>
#include <QFile>
#include <functional>
#include <QGuiApplication>
#include <QCoreApplication>
#include <QPointer>
#include <qapplication.h>
#include <qscrollarea.h>
#include "db/sqlinterface.h"
#include "constants/db_types.h"

AITransferTask::AITransferTask(QObject *parent)
    : QObject(parent)
    , networkManager(new QNetworkAccessManager(this))
    , currentReply(nullptr)
    , progressDialog(nullptr)
    , progressLabel(nullptr)
    , progressBar(nullptr)
    , pythonServerUrl("http://localhost:8000")
    , operationCancelled(false)
{
    qDebug() << "AITransferTask 初始化完成";
}

AITransferTask::~AITransferTask()
{
    // 设置取消标志，停止正在进行的所有操作
    operationCancelled = true;
    
    hideProgressDialog();
    
    if (currentReply) {
        currentReply->abort();  // 中断网络请求
        currentReply->deleteLater();
        currentReply = nullptr;
    }
}

void AITransferTask::setPythonServerUrl(const QString &url)
{
    pythonServerUrl = url;
}

void AITransferTask::sendChatMessage(const QString &message, std::function<void(const QString&)> callback)
{
    // 检查操作是否已取消
    if (operationCancelled) {
        qDebug() << "操作已取消，拒绝新的聊天请求";
        if (callback) {
            callback("操作已取消");
        }
        return;
    }
    
    chatCallback = callback;

    QNetworkRequest request(QUrl(pythonServerUrl + "/api/chat"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setHeader(QNetworkRequest::UserAgentHeader, "AITransferClient/1.0");

    QJsonObject json;
    json["message"] = message;

    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    currentReply = networkManager->post(request, data);
    connect(currentReply, &QNetworkReply::finished, this, &AITransferTask::onChatReplyFinished);
    connect(currentReply, &QNetworkReply::errorOccurred, [this](QNetworkReply::NetworkError error) {
        if (operationCancelled) return;  // 检查取消状态
        
        qDebug() << "网络错误:" << error << "描述:" << currentReply->errorString();
        onNetworkError(currentReply->errorString());
    });
}

void AITransferTask::updateKnowledgeBaseFromDatabase(QWidget *parentWidget, std::function<void(bool, const QString&)> callback)
{
    kbCallback = callback;
    operationCancelled = false;  // 重置取消标志

    showProgressDialog("知识库更新", "正在获取职位数据...");

    // 使用 QPointer 来安全地检查父窗口是否仍然存在
    QPointer<QWidget> safeParent = parentWidget;
    
    QTimer::singleShot(100, this, [this, safeParent]() {
        // 检查父窗口是否仍然存在
        if (!safeParent) {
            qDebug() << "父窗口已销毁，取消知识库更新操作";
            hideProgressDialog();
            return;
        }
        
        if (operationCancelled) {
            qDebug() << "操作已被取消";
            hideProgressDialog();
            return;
        }

        // 获取数据库数据
        qInfo() << "开始从数据库获取职位数据...";
        QVector<QMap<QString, QVariant>> jobData = fetchJobDataFromDatabase();
        
        // 更新进度并显示日志
        QString progressMsg = QString("正在获取职位数据...\n获取到 %1 条职位数据\n\n开始逐个处理数据...").arg(jobData.size());
        qInfo() << progressMsg;
        updateProgressMessage(progressMsg);

        if (jobData.isEmpty()) {
            qWarning() << "数据库中没有找到职位数据";
            updateProgressMessage("未找到职位数据");
            hideProgressDialog();
            emit transferCompleted(false, "未找到职位数据");
            return;
        }

        // 格式化数据
        qInfo() << "开始格式化" << jobData.size() << "条职位数据...";
        QVector<QMap<QString, QVariant>> formattedData = formatJobDataForSingleProcessing(jobData);
        QString formatMsg = QString("数据格式化完成，共 %1 条数据\n\n开始逐个发送数据到AI后端...").arg(formattedData.size());
        qInfo() << formatMsg;
        updateProgressMessage(formatMsg);

        // 开始逐个数据处理流程
        startSingleDataProcessingLoop(formattedData);
    });
}

void AITransferTask::onChatReplyFinished()
{
    if (!currentReply) return;

    QByteArray responseData = currentReply->readAll();
    currentReply->deleteLater();
    currentReply = nullptr;

    // 解析JSON响应，提取message字段
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
    QString displayMessage;
    bool isSuccess = false;
    
    if (responseDoc.isNull() || !responseDoc.isObject()) {
        // JSON解析失败，显示原始响应
        displayMessage = QString::fromUtf8(responseData);
        qWarning() << "AI聊天响应JSON解析失败，显示原始响应";
    } else {
        QJsonObject responseObj = responseDoc.object();
        isSuccess = responseObj["success"].toBool();
        displayMessage = responseObj["message"].toString();
        
        // 如果message字段为空或不存在，使用原始响应
        if (displayMessage.isEmpty()) {
            displayMessage = QString::fromUtf8(responseData);
            qWarning() << "AI聊天响应message字段为空，使用原始响应";
        }
    }

    if (chatCallback) {
        chatCallback(displayMessage);
    }

    emit transferCompleted(true, "聊天消息发送成功");
}

void AITransferTask::onKnowledgeBaseReplyFinished()
{
    if (!currentReply) {
        qWarning() << "网络回复为空";
        return;
    }

    QByteArray responseData = currentReply->readAll();
    qInfo() << "收到服务器响应，原始数据长度:" << responseData.length();
    qInfo() << "原始响应数据:" << responseData.left(500); // 只显示前500字符
    qInfo() << "原始响应数据（UTF-8解码）:" << QString::fromUtf8(responseData);

    // 解析响应
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
    if (responseDoc.isNull() || !responseDoc.isObject()) {
        qWarning() << "响应格式无效";
        hideProgressDialog();
        emit transferCompleted(false, "服务器响应格式无效");
        return;
    }

    QJsonObject responseObj = responseDoc.object();
    qInfo() << "JSON解析成功，响应对象键数量:" << responseObj.keys().size();
    qInfo() << "所有响应键:" << responseObj.keys();
    
    bool success = responseObj["success"].toBool();
    QString message = responseObj["message"].toString();
    qInfo() << "解析结果 - 成功:" << success << ", 消息:" << message;
    
    int skippedCount = 0;
    
    // 检查是否有跳过计数字段
    if (responseObj.contains("skipped_count") && responseObj["skipped_count"].isDouble()) {
        skippedCount = responseObj["skipped_count"].toInt();
        qInfo() << "跳过计数字段存在，值:" << skippedCount;
    } else {
        qInfo() << "跳过计数字段不存在或类型错误";
    }

    QString decodedLogInfo; // 在作用域内声明变量
    
    // 检查是否有日志信息
    qInfo() << "检查日志信息字段:" << responseObj.keys();
    if (responseObj.contains("log_info") && responseObj["log_info"].isString()) {
        QString logInfo = responseObj["log_info"].toString();
        qInfo() << "原始日志信息长度:" << logInfo.length();
        
        // 转换编码以正确显示中文
        QByteArray logInfoBytes = logInfo.toUtf8();
        decodedLogInfo = QString::fromUtf8(logInfoBytes);
        
        // 输出完整日志到控制台，方便调试
        qInfo() << "解码后日志信息:" << decodedLogInfo;
        
        // 直接显示完整的日志信息到进度对话框
        updateProgressMessage(decodedLogInfo);
        
        // 强制刷新UI
        if (progressDialog) {
            progressDialog->repaint();
            QApplication::processEvents();
        }
    } else {
        qWarning() << "未找到日志信息字段或类型不正确";
        qInfo() << "响应对象的所有键:" << responseObj.keys();
    }

    // 检查是否有错误
    if (!success) {
        qWarning() << "知识库更新失败:" << message;
        updateProgressMessage("知识库更新失败: " + message);
        hideProgressDialog();
        emit transferCompleted(false, message);
        return;
    }

    // 更新UI显示完成信息，但保留详细日志
    QString completionMsg = QString("\n知识库更新完成\n%1\n%2").arg(message).arg(
        skippedCount > 0 ? QString("已跳过 %1 条已存在数据").arg(skippedCount) : "");
    
    // 将完成信息追加到现有日志后面，而不是覆盖
    QString finalMessage;
    if (!decodedLogInfo.isEmpty()) {
        finalMessage = decodedLogInfo + completionMsg;
    } else {
        finalMessage = completionMsg;
    }
    updateProgressMessage(finalMessage);
    
    // 延迟关闭进度对话框，但保持聊天界面活跃状态
    QTimer::singleShot(3000, this, [this]() {
        hideProgressDialog();
        qDebug() << "进度对话框已隐藏，聊天界面应保持活跃状态";
    });
    
    // 发出完成信号
    emit transferCompleted(true, message);

    // 调用回调函数
    if (kbCallback) {
        kbCallback(true, QString::fromUtf8(responseData));
    }

    // 清理当前回复
    currentReply->deleteLater();
    currentReply = nullptr;
}

void AITransferTask::onNetworkError(const QString &errorDetails)
{
    hideProgressDialog();

    if (currentReply) {
        currentReply->deleteLater();
        currentReply = nullptr;
    }

    QString errorMsg = QString("网络连接错误");
    if (!errorDetails.isEmpty()) {
        errorMsg += QString(" - %1").arg(errorDetails);
    }

    emit transferCompleted(false, errorMsg);
}



QVector<QMap<QString, QVariant>> AITransferTask::fetchJobDataFromDatabase()
{
    QVector<QMap<QString, QVariant>> result;
    
    // 使用SQLInterface类获取数据
    SQLInterface sqlInterface;
    
    // 首先需要连接数据库
    QString dbPath = QCoreApplication::applicationDirPath() + "/crawler.db";
    if (!sqlInterface.connectSqlite(dbPath)) {
        qWarning() << "无法连接到数据库:" << dbPath;
        return result;
    }
    
    // 查询职位信息（包含解析后的名称和标签）
    QVector<SQLNS::JobInfoPrint> jobs = sqlInterface.queryAllJobsPrint();
    
    // 将数据转换为QMap格式
    for (const SQLNS::JobInfoPrint& job : jobs) {
        QMap<QString, QVariant> jobMap;
        
<<<<<<< HEAD
        // 基本信息
        jobMap["jobId"] = QVariant::fromValue(job.jobId);
        jobMap["title"] = QVariant(job.jobName);
        jobMap["company"] = QVariant(job.companyName.isEmpty() ? "未知公司" : job.companyName);
        jobMap["location"] = QVariant(job.cityName.isEmpty() ? "未知地点" : job.cityName);
        
        // 薪资信息
=======
        // 基本信息 - 确保所有数据库字段都被包含
        jobMap["jobId"] = QVariant::fromValue(job.jobId);
        jobMap["jobName"] = QVariant(job.jobName);
        jobMap["title"] = QVariant(job.jobName);  // 保留兼容性
        jobMap["companyId"] = QVariant::fromValue(job.companyId);
        jobMap["companyName"] = QVariant(job.companyName.isEmpty() ? "未知公司" : job.companyName);
        jobMap["company"] = QVariant(job.companyName.isEmpty() ? "未知公司" : job.companyName);  // 保留兼容性
        jobMap["cityId"] = QVariant::fromValue(job.cityId);
        jobMap["cityName"] = QVariant(job.cityName.isEmpty() ? "未知地点" : job.cityName);
        jobMap["location"] = QVariant(job.cityName.isEmpty() ? "未知地点" : job.cityName);  // 保留兼容性
        jobMap["recruitTypeId"] = QVariant::fromValue(job.recruitTypeId);
        jobMap["recruitTypeName"] = QVariant(job.recruitTypeName);
        jobMap["recruitType"] = QVariant(job.recruitTypeName);  // 保留兼容性
        
        // 薪资信息
        jobMap["salaryMin"] = QVariant::fromValue(job.salaryMin);
        jobMap["salaryMax"] = QVariant::fromValue(job.salaryMax);
        jobMap["salarySlabId"] = QVariant::fromValue(job.salarySlabId);
        
>>>>>>> 48027ee (优化了AI提示词，删去了对话内容存入知识库导致污染的问题，优化了数据库更新至知识库的功能，强化了回答检索知识库的能力)
        QString salaryStr;
        if (job.salaryMin > 0 && job.salaryMax > 0) {
            salaryStr = QString("%1-%2元").arg(job.salaryMin).arg(job.salaryMax);
        } else if (job.salaryMin > 0) {
            salaryStr = QString("最低%1元").arg(job.salaryMin);
        } else if (job.salaryMax > 0) {
            salaryStr = QString("最高%1元").arg(job.salaryMax);
        } else {
            salaryStr = "薪资面议";
        }
        jobMap["salary"] = QVariant(salaryStr);
        
        // 职位描述
        jobMap["description"] = QVariant(job.requirements);
<<<<<<< HEAD
        
        // 招聘类型
        jobMap["recruitType"] = QVariant(job.recruitTypeName);
=======
        jobMap["requirements"] = QVariant(job.requirements);  // 保留兼容性
        
        // 时间信息
        jobMap["createTime"] = QVariant(job.createTime);
        jobMap["updateTime"] = QVariant(job.updateTime);
>>>>>>> 48027ee (优化了AI提示词，删去了对话内容存入知识库导致污染的问题，优化了数据库更新至知识库的功能，强化了回答检索知识库的能力)
        
        // 来源
        jobMap["source"] = QVariant(job.sourceName);
        
        // 标签
        QString tagList;
        if (!job.tagNames.isEmpty()) {
            tagList = job.tagNames.join(", ");
        }
        jobMap["tags"] = QVariant(tagList);
        
        // 创建时间和更新时间
        jobMap["createTime"] = QVariant(job.createTime);
        jobMap["updateTime"] = QVariant(job.updateTime);
        
        result.append(jobMap);
    }
    
    return result;
}

QVector<QMap<QString, QVariant>> AITransferTask::formatJobDataForSingleProcessing(const QVector<QMap<QString, QVariant>>& jobData)
{
    QVector<QMap<QString, QVariant>> result;

    for (const auto& job : jobData) {
        QMap<QString, QVariant> formattedJob;
        
        // 生成jobId（使用jobId作为标识）
        QString jobId = QString::number(job.value("jobId").toLongLong());
        if (jobId.isEmpty()) {
            jobId = "未知职位";
        }
        
        // 创建info字段，包含所有相关信息
        QString info;
<<<<<<< HEAD
        info += "职位标题: " + job.value("title").toString() + "\n";
        info += "公司: " + job.value("company").toString() + "\n";
        info += "地点: " + job.value("location").toString() + "\n";
        info += "招聘类型: " + job.value("recruitType").toString() + "\n";
        info += "薪资: " + job.value("salary").toString() + "\n";
        info += "来源: " + job.value("source").toString() + "\n";
        info += "标签: " + job.value("tags").toString() + "\n";
        info += "职位描述: " + job.value("description").toString() + "\n";
        info += "发布时间: " + job.value("createTime").toString() + "\n";
        info += "更新时间: " + job.value("updateTime").toString();
=======
        info += "【职位标题】: " + job.value("title").toString() + "\n";
        info += "【公司名称】: " + job.value("company").toString() + "\n";
        info += "【工作地点】: " + job.value("location").toString() + "\n";
        info += "【招聘类型】: " + job.value("recruitType").toString() + "\n";
        info += "【薪资待遇】: " + job.value("salary").toString() + "\n";
        info += "【数据来源】: " + job.value("source").toString() + "\n";
        info += "【职位标签】: " + job.value("tags").toString() + "\n";
        info += "【职位描述和要求】: " + job.value("description").toString() + "\n";
        info += "【发布时间】: " + job.value("createTime").toString() + "\n";
        info += "【最后更新时间】: " + job.value("updateTime").toString() + "\n";
        info += "【数据提取时间】: " + QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
>>>>>>> 48027ee (优化了AI提示词，删去了对话内容存入知识库导致污染的问题，优化了数据库更新至知识库的功能，强化了回答检索知识库的能力)
        
        // 设置为单个数据处理格式
        formattedJob["jobId"] = QVariant(jobId);
        formattedJob["info"] = QVariant(info);
        
        result.append(formattedJob);
    }

    return result;
}

void AITransferTask::startSingleDataProcessingLoop(const QVector<QMap<QString, QVariant>>& formattedData)
{
    // 存储所有数据以供循环处理
    pendingDataItems = formattedData;
    currentDataIndex = 0;
    totalDataItems = formattedData.size();
    successCount = 0;
    skippedCount = 0;
    errorCount = 0;

    if (totalDataItems == 0) {
        updateProgressMessage("没有需要处理的数据");
        hideProgressDialog();
        emit transferCompleted(true, "没有需要处理的数据");
        return;
    }

    // 开始处理第一个数据项
    processNextDataItem();
}

void AITransferTask::processNextDataItem()
{
    // 检查是否完成所有数据处理
    if (currentDataIndex >= totalDataItems) {
        // 所有数据处理完成
        QString completionMsg = QString("知识库更新完成！\n\n总处理: %1 条数据\n成功: %2 条\n跳过: %3 条\n错误: %4 条")
            .arg(totalDataItems)
            .arg(successCount)
            .arg(skippedCount)
            .arg(errorCount);
        
        qInfo() << completionMsg;
        updateProgressMessage(completionMsg);
        
        // 延迟关闭对话框
        QTimer::singleShot(2000, this, [this]() {
            hideProgressDialog();
        });
        
        emit transferCompleted(true, QString("成功处理 %1 条数据").arg(successCount));
        return;
    }

    // 检查操作是否被取消
    if (operationCancelled) {
        QString cancelledMsg = QString("操作已取消\n\n已处理: %1/%2 条数据\n成功: %3 条\n跳过: %4 条\n错误: %5 条")
            .arg(currentDataIndex)
            .arg(totalDataItems)
            .arg(successCount)
            .arg(skippedCount)
            .arg(errorCount);
        
        updateProgressMessage(cancelledMsg);
        hideProgressDialog();
        emit transferCompleted(false, "操作被用户取消");
        return;
    }

    // 获取当前要处理的数据项
    const QMap<QString, QVariant>& currentDataItem = pendingDataItems[currentDataIndex];
    QString jobId = currentDataItem.value("jobId").toString();
    QString info = currentDataItem.value("info").toString();

    // 更新进度显示
    QString progressMsg = QString("正在处理第 %1/%2 条数据\n职位ID: %3\n内容预览: %4...")
        .arg(currentDataIndex + 1)
        .arg(totalDataItems)
        .arg(jobId)
        .arg(info.left(50));
    
    qInfo() << "处理数据项" << (currentDataIndex + 1) << "/" << totalDataItems << "ID:" << jobId;
    updateProgressMessage(progressMsg);

    // 发送单个数据项到Python后端
    sendSingleDataItemToPython(currentDataItem);
}

void AITransferTask::sendSingleDataItemToPython(const QMap<QString, QVariant>& dataItem)
{
    QNetworkRequest request(QUrl(pythonServerUrl + "/process_single_data"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    QJsonObject json;
    json["data_item"] = QJsonObject::fromVariantMap(dataItem);

    QJsonDocument doc(json);
    QByteArray data = doc.toJson();

    currentReply = networkManager->post(request, data);
    connect(currentReply, &QNetworkReply::finished, this, &AITransferTask::onSingleDataReplyFinished);
    connect(currentReply, &QNetworkReply::errorOccurred, [this](QNetworkReply::NetworkError error) {
        Q_UNUSED(error)
        handleSingleDataError();
    });
}

void AITransferTask::onSingleDataReplyFinished()
{
    if (!currentReply) {
        qWarning() << "单个数据处理响应为空";
        handleSingleDataError();
        return;
    }

    QByteArray responseData = currentReply->readAll();
    qInfo() << "收到单个数据处理响应，长度:" << responseData.length();

    // 解析响应
    QJsonDocument responseDoc = QJsonDocument::fromJson(responseData);
    if (responseDoc.isNull() || !responseDoc.isObject()) {
        qWarning() << "单个数据处理响应格式无效";
        errorCount++;
        currentDataIndex++;
        processNextDataItem();  // 继续处理下一个
        return;
    }

    QJsonObject responseObj = responseDoc.object();
    bool success = responseObj["success"].toBool();
    QString message = responseObj["message"].toString();
    QString jobId = responseObj["job_id"].toString();
    bool skipped = responseObj["skipped"].toBool();

    // 获取处理日志
    QString logInfo;
    if (responseObj.contains("log_info") && responseObj["log_info"].isString()) {
        logInfo = responseObj["log_info"].toString();
    }

    // 更新计数
    if (success && !skipped) {
        successCount++;
        qInfo() << "成功处理数据项:" << jobId;
    } else if (skipped) {
        skippedCount++;
        qInfo() << "跳过数据项:" << jobId << "(已存在)";
    } else {
        errorCount++;
        qWarning() << "处理数据项失败:" << jobId << "错误:" << message;
    }

    // 显示处理结果
    QString resultMsg = QString("第 %1/%2 条数据处理结果:\n%3\n%4")
        .arg(currentDataIndex + 1)
        .arg(totalDataItems)
        .arg(message)
        .arg(!logInfo.isEmpty() ? QString("\n详细日志:\n%1").arg(logInfo) : "");
    
    updateProgressMessage(resultMsg);

    // 清理当前回复
    currentReply->deleteLater();
    currentReply = nullptr;

    // 移动到下一个数据项
    currentDataIndex++;
    
    // 短暂延迟以显示处理结果，然后继续下一个
    QTimer::singleShot(500, this, &AITransferTask::processNextDataItem);
}

void AITransferTask::handleSingleDataError()
{
    qWarning() << "单个数据处理网络错误";
    errorCount++;
    currentDataIndex++;

    if (currentReply) {
        currentReply->deleteLater();
        currentReply = nullptr;
    }

    // 继续处理下一个数据项
    processNextDataItem();
}

QJsonArray AITransferTask::formatJobDataForAPI(const QVector<QMap<QString, QVariant>>& jobData)
{
    QJsonArray array;

    for (const auto& job : jobData) {
        QJsonObject obj;
        
        // 生成jobId（使用jobId作为标识）
        QString jobId = QString::number(job.value("jobId").toLongLong());
        if (jobId.isEmpty()) {
            jobId = "未知职位";
        }
        
        // 创建info字段，包含所有相关信息
        QString info;
        info += "职位标题: " + job.value("title").toString() + "\n";
        info += "公司: " + job.value("company").toString() + "\n";
        info += "地点: " + job.value("location").toString() + "\n";
        info += "招聘类型: " + job.value("recruitType").toString() + "\n";
        info += "薪资: " + job.value("salary").toString() + "\n";
        info += "来源: " + job.value("source").toString() + "\n";
        info += "标签: " + job.value("tags").toString() + "\n";
        info += "职位描述: " + job.value("description").toString() + "\n";
        info += "发布时间: " + job.value("createTime").toString() + "\n";
        info += "更新时间: " + job.value("updateTime").toString();
        
        // 设置Python后端期望的格式
        obj["jobId"] = jobId;
        obj["info"] = info;
        
        array.append(obj);
    }

    return array;
}

void AITransferTask::showProgressDialog(const QString &title, const QString &message)
{
    hideProgressDialog();

    progressDialog = new QDialog();
    progressDialog->setWindowTitle(title);
    progressDialog->setModal(true);
    progressDialog->resize(700, 500); // 增大对话框尺寸以容纳更多日志

    QVBoxLayout *layout = new QVBoxLayout(progressDialog);

    // 使用QTextEdit替代QLabel以支持长文本和滚动
    progressTextEdit = new QTextEdit();
    progressTextEdit->setReadOnly(true); // 设置为只读
    progressTextEdit->setPlainText(message);
    progressTextEdit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    progressTextEdit->setWordWrapMode(QTextOption::WordWrap); // 启用自动换行
    
    progressBar = new QProgressBar();
    progressBar->setRange(0, 0); // 不确定进度

    layout->addWidget(progressTextEdit);
    layout->addWidget(progressBar);

    progressDialog->show();
}

void AITransferTask::hideProgressDialog()
{
    if (progressDialog) {
        progressDialog->close();
        delete progressDialog;
        progressDialog = nullptr;
        progressLabel = nullptr;
        progressTextEdit = nullptr;
        progressBar = nullptr;
    }
}

void AITransferTask::updateProgressMessage(const QString &message)
{
    qInfo() << "更新进度消息:" << message;
    
    // 优先使用QTextEdit来显示日志
    if (progressTextEdit) {
        progressTextEdit->setPlainText(message);
        progressTextEdit->moveCursor(QTextCursor::End); // 滚动到末尾
        progressTextEdit->ensureCursorVisible(); // 确保光标可见
        progressTextEdit->repaint(); // 强制重绘
        
        qInfo() << "更新进度消息到文本框，长度:" << message.length();
        
        // 强制刷新UI
        if (progressDialog) {
            progressDialog->repaint();
        }
        QApplication::processEvents();
    }
    // 如果没有QTextEdit，则使用QLabel作为备选
    else if (progressLabel) {
        progressLabel->setText(message);
        progressLabel->repaint(); // 强制重绘
        progressLabel->update(); // 强制立即更新UI
        
        // 处理UI事件，确保消息立即显示
        QApplication::processEvents();
        
        qInfo() << "更新进度消息到标签，长度:" << message.length();
    } else {
        qWarning() << "无法更新进度消息：没有可用的UI控件";
    }
}

bool AITransferTask::validateJSON(const QByteArray &data)
{
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(data, &error);
    return error.error == QJsonParseError::NoError;
}

QString AITransferTask::getDatabasePath() const
{
    return QCoreApplication::applicationDirPath() + "/crawler.db";
}

bool AITransferTask::isRunning() const
{
    return !operationCancelled && currentReply != nullptr;
}

void AITransferTask::cancelOperation()
{
    qDebug() << "开始取消当前操作";
    
    // 设置取消标志
    operationCancelled = true;
    
    // 中断当前的网络请求
    if (currentReply && currentReply->isRunning()) {
        qDebug() << "中断当前网络请求";
        currentReply->abort();
        currentReply->deleteLater();
        currentReply = nullptr;
    }
    
    // 隐藏进度对话框
    hideProgressDialog();
    
    // 发出取消完成的信号
    emit transferCompleted(false, "操作已取消");
    
    qDebug() << "操作已成功取消";
}
