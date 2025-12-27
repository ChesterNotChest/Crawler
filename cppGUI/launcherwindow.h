#ifndef LAUNCHERWINDOW_H
#define LAUNCHERWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QFrame>
#include <QIcon>
#include <QPixmap>
#include <QFont>
#include <QPropertyAnimation>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QUrl>

class BackendManagerTask;
class ChatWindow;
class AITransferTask;

class LauncherWindow : public QMainWindow
{
    Q_OBJECT

signals:
    void chatWindowClosed();

public:
    explicit LauncherWindow(QWidget *parent = nullptr);
    ~LauncherWindow();

private slots:
    void onAIChatButtonClicked();
    void onCrawlerButtonClicked();
    void onSettingsButtonClicked();
    void onAboutButtonClicked();
    void onExitButtonClicked();
    void animateButtons();
    void onVersionCheckTimer();
    void onBackendConnectionChanged(bool connected);

private:
    void setupUI();
    void setupAnimations();
    void setupStyle();
    void centerWindow();
    void loadConfiguration();
    void checkUpdates();

    // UI设置函数
    void setupTitleArea();
    void setupFunctionButtons();
    void setupVersionInfo();

    // 创建功能卡片
    QPushButton* createFunctionCard(const QString &title, const QString &description, const QString &icon);

    // 窗口管理
    void openChatWindow();
    void openCrawlerInterface();

private:
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;

    // 标题区域
    QLabel *titleLabel;
    QLabel *subtitleLabel;
    QLabel *versionLabel;

    // 功能按钮
    QPushButton *aiChatButton;
    QPushButton *crawlerButton;
    QPushButton *settingsButton;
    QPushButton *aboutButton;
    QPushButton *exitButton;

    // 动画相关
    QPropertyAnimation *titleAnimation;
    QPropertyAnimation *buttonAnimation;
    QTimer *versionCheckTimer;

    // 配置
    QString appVersion;
    QString updateUrl;
    bool autoCheckUpdates;

    // 任务管理
    BackendManagerTask *backendManager;
    QString pythonServerUrl;
    bool backendConnected;
};

#endif // LAUNCHERWINDOW_H
