#ifndef BACKEND_MANAGER_TASK_H
#define BACKEND_MANAGER_TASK_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTimer>
#include <QProgressDialog>
#include <QLabel>
#include <QProgressBar>
#include <functional>

class BackendManagerTask : public QObject
{
    Q_OBJECT

public:
    explicit BackendManagerTask(QObject *parent = nullptr);
    ~BackendManagerTask();

    void setPythonServerUrl(const QString &url);
    bool isBackendConnected() const { return backendConnected; }

    // 连接管理
    void connectToBackend(std::function<void(bool, const QString&)> callback = nullptr);
    void disconnectFromBackend();
    void checkBackendHealth();

    // 版本检查
    void checkUpdates(std::function<void(bool, const QString&)> callback = nullptr);

signals:
    void connectionStatusChanged(bool connected);
    void progressUpdated(int current, int total, const QString &message);
    void connectionCompleted(bool success, const QString &message);
    void updateAvailable(const QString &version, const QString &downloadUrl);
    void noUpdateAvailable();

private slots:
    void onConnectionTimeout();
    void onConnectionSuccess();
    void onHealthCheckFinished();
    void onPingReplyFinished();
    void onVersionCheckFinished();
    void onVersionCheckTimer();

private:
    void showConnectionProgress(const QString &title, const QString &message);
    void hideConnectionProgress();

    QNetworkAccessManager *networkManager;
    QString pythonServerUrl;
    bool backendConnected;
    int connectionAttempts;

    // UI组件
    QProgressDialog *connectionProgress;
    QTimer *connectionTimer;
    QTimer *healthCheckTimer;
    QTimer *versionCheckTimer;

    // 回调函数
    std::function<void(bool, const QString&)> connectionCallback;
    std::function<void(bool, const QString&)> updateCallback;

    // 网络请求
    QNetworkReply *currentReply;
    void onConnectionError(QNetworkReply::NetworkError error);
};

#endif // BACKEND_MANAGER_TASK_H
