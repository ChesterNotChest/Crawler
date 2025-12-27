#include "backend_manager_task.h"
#include <QApplication>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QProgressDialog>
#include <QTimer>
#include <functional>

BackendManagerTask::BackendManagerTask(QObject *parent)
    : QObject(parent)
    , networkManager(nullptr)
    , backendConnected(false)
    , connectionAttempts(0)
    , connectionProgress(nullptr)
    , connectionTimer(nullptr)
    , healthCheckTimer(nullptr)
    , versionCheckTimer(nullptr)
    , currentReply(nullptr)
    , pythonServerUrl("http://localhost:8000")
{
    // 初始化网络管理器
    networkManager = new QNetworkAccessManager(this);

    // 启动定期健康检查
    healthCheckTimer = new QTimer(this);
    connect(healthCheckTimer, &QTimer::timeout, this, &BackendManagerTask::checkBackendHealth);
}

BackendManagerTask::~BackendManagerTask()
{
    disconnectFromBackend();

    if (networkManager) delete networkManager;
    if (healthCheckTimer) delete healthCheckTimer;
    if (connectionTimer) delete connectionTimer;
    if (connectionProgress) {
        connectionProgress->close();
        delete connectionProgress;
    }
    if (versionCheckTimer) delete versionCheckTimer;
}

void BackendManagerTask::setPythonServerUrl(const QString &url)
{
    pythonServerUrl = url;
}

void BackendManagerTask::connectToBackend(std::function<void(bool, const QString&)> callback)
{
    connectionCallback = callback;

    // 显示连接进度
    showConnectionProgress("连接中", "正在连接AI后端服务器...");

    // 初始化网络管理器
    if (!networkManager) {
        networkManager = new QNetworkAccessManager(this);
    }

    // 设置连接超时定时器
    if (!connectionTimer) {
        connectionTimer = new QTimer(this);
        connect(connectionTimer, &QTimer::timeout, this, &BackendManagerTask::onConnectionTimeout);
        connectionTimer->start(10000); // 10秒连接超时
    }

    // 尝试连接后端
    QNetworkRequest request(QUrl(pythonServerUrl + "/ping"));
    currentReply = networkManager->get(request);

    connect(currentReply, &QNetworkReply::finished, this, &BackendManagerTask::onHealthCheckFinished);
}

void BackendManagerTask::disconnectFromBackend()
{
    backendConnected = false;

    if (healthCheckTimer) {
        healthCheckTimer->stop();
    }

    if (connectionTimer) {
        connectionTimer->stop();
    }

    if (currentReply) {
        currentReply->abort();
        currentReply = nullptr;
    }

    hideConnectionProgress();
    // 不在析构过程中发射信号，避免崩溃
}

void BackendManagerTask::checkBackendHealth()
{
    if (!backendConnected || !networkManager) return;

    QNetworkRequest request(QUrl(pythonServerUrl + "/ping"));
    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, &BackendManagerTask::onPingReplyFinished);
}

void BackendManagerTask::checkUpdates(std::function<void(bool, const QString&)> callback)
{
    updateCallback = callback;

    // 定期版本检查
    if (!versionCheckTimer) {
        versionCheckTimer = new QTimer(this);
        connect(versionCheckTimer, &QTimer::timeout, this, &BackendManagerTask::onVersionCheckTimer);
    }

    // TODO: 实现版本检查逻辑
    if (callback) {
        callback(true, "当前已是最新版本");
    }
}

void BackendManagerTask::showConnectionProgress(const QString &title, const QString &message)
{
    hideConnectionProgress();

    connectionProgress = new QProgressDialog(message, "取消", 0, 0, nullptr);
    connectionProgress->setWindowTitle(title);
    connectionProgress->setWindowModality(Qt::WindowModal);
    connectionProgress->setCancelButton(nullptr);  // 禁用取消按钮
    connectionProgress->show();
    QApplication::processEvents();
}

void BackendManagerTask::hideConnectionProgress()
{
    if (connectionProgress) {
        connectionProgress->close();
        delete connectionProgress;
        connectionProgress = nullptr;
    }
}

void BackendManagerTask::onConnectionTimeout()
{
    connectionAttempts++;

    if (connectionAttempts >= 3) {
        // 连接失败
        hideConnectionProgress();

        backendConnected = false;
        emit connectionStatusChanged(false);

        QString errorMsg = QString("无法连接到AI后端服务器 (%1)\n请检查Python后端是否正在运行").arg(pythonServerUrl);

        if (connectionCallback) {
            connectionCallback(false, errorMsg);
        } else {
            QMessageBox::warning(nullptr, "连接失败", errorMsg);
        }

        if (connectionTimer) {
            connectionTimer->stop();
        }
    } else {
        // 重试连接
        QNetworkRequest request(QUrl(pythonServerUrl + "/ping"));
        currentReply = networkManager->get(request);

        connect(currentReply, &QNetworkReply::finished, this, &BackendManagerTask::onHealthCheckFinished);
        connect(currentReply, &QNetworkReply::errorOccurred, this, &BackendManagerTask::onConnectionTimeout);
    }
}

void BackendManagerTask::onHealthCheckFinished()
{
    if (!currentReply) return;

    if (currentReply->error() == QNetworkReply::NoError) {
        // 连接成功
        onConnectionSuccess();
    } else {
        // 连接失败
        onConnectionTimeout();
    }

    currentReply->deleteLater();
    currentReply = nullptr;
}

void BackendManagerTask::onConnectionError(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)

    connectionAttempts++;

    if (connectionAttempts >= 3) {
        // 连接失败
        hideConnectionProgress();

        backendConnected = false;
        emit connectionStatusChanged(false);

        QString errorMsg = QString("无法连接到AI后端服务器 (%1)\n请检查Python后端是否正在运行").arg(pythonServerUrl);

        if (connectionCallback) {
            connectionCallback(false, errorMsg);
        } else {
            QMessageBox::warning(nullptr, "连接失败", errorMsg);
        }

        if (connectionTimer) {
            connectionTimer->stop();
        }
    } else {
        // 重试连接
        QNetworkRequest request(QUrl(pythonServerUrl + "/health"));
        currentReply = networkManager->get(request);

        connect(currentReply, &QNetworkReply::finished, this, &BackendManagerTask::onHealthCheckFinished);
        connect(currentReply, &QNetworkReply::errorOccurred, this, &BackendManagerTask::onConnectionError);
    }
}

void BackendManagerTask::onConnectionSuccess()
{
    // 停止连接定时器
    if (connectionTimer) {
        connectionTimer->stop();
    }

    // 关闭进度对话框
    hideConnectionProgress();

    // 设置连接状态
    backendConnected = true;
    connectionAttempts = 0;

    // 启动健康检查定时器
    if (healthCheckTimer && !healthCheckTimer->isActive()) {
        healthCheckTimer->start(30000); // 每30秒检查一次
    }

    emit connectionStatusChanged(true);

    if (connectionCallback) {
        connectionCallback(true, "连接成功");
    }
}

void BackendManagerTask::onPingReplyFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    if (reply->error() == QNetworkReply::NoError) {
        if (!backendConnected) {
            backendConnected = true;
            emit connectionStatusChanged(true);
        }
    } else {
        if (backendConnected) {
            backendConnected = false;
            emit connectionStatusChanged(false);
        }

        if (healthCheckTimer) {
            healthCheckTimer->stop();
        }
    }

    reply->deleteLater();
}

void BackendManagerTask::onVersionCheckFinished()
{
    QNetworkReply *reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) return;

    // TODO: 处理版本检查响应
    QString response = reply->readAll();

    if (updateCallback) {
        updateCallback(true, response);
    }

    reply->deleteLater();
}

void BackendManagerTask::onVersionCheckTimer()
{
    // 执行定期版本检查
    QNetworkRequest request(QUrl(pythonServerUrl + "/version"));
    QNetworkReply *reply = networkManager->get(request);

    connect(reply, &QNetworkReply::finished, this, &BackendManagerTask::onVersionCheckFinished);
    connect(reply, &QNetworkReply::errorOccurred, this, &BackendManagerTask::onVersionCheckFinished);
}
