#ifndef CHATWINDOW_H
#define CHATWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QTextEdit>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QWidget>
#include <QFrame>
#include <QMessageBox>
#include <QSettings>
#include <QDateTime>
#include <QDir>
#include <QCloseEvent>
#include <QProgressBar>
#include <functional>

class AITransferTask;

class ChatWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit ChatWindow(QWidget *parent = nullptr);
    ~ChatWindow();

    // 设置AI传输任务对象
    void setAITransferTask(AITransferTask *task);
    // 设置服务器URL
    void setServerUrl(const QString &serverUrl);
    // 设置连接状态
    void setConnected(bool connected);
    // 设置父窗口（用于返回主界面）
    void setParentLauncher(QWidget *parentLauncher);

signals:
    void connectionStatusChanged(bool connected);
    void messageSent(const QString &message);
    void messageReceived(const QString &response);
    void returnToMainWindow();

private slots:
    void onSendButtonClicked();
    void onReturnMainButtonClicked();
    void onUpdateButtonClicked();
    void onSystemTrayActivated(QSystemTrayIcon::ActivationReason reason);
    void onUpdateConnectionStatus();

    void sendChatMessage(const QString &message);
    void updateKnowledgeBase();
    void onAITransferCompleted(bool success, const QString &message);
    void displayMessage(const QString &sender, const QString &message);
    void removeLastMessage();

private:
    void setupUI();
    void setupLeftChatArea();
    void setupRightAIStatusArea();
    void setupTimers();
    void setupSystemTray();
    void setupDarkTheme();
    void loadSettings();
    void saveSettings();
    void loadChatHistory();
    void saveChatHistory();
    void updateConnectionStatusDisplay();
    void showNotification(const QString &title, const QString &message);
    void updateAIStatus(const QString &status, bool isThinking);

    // UI组件设置函数
    void setupToolbar();
    void setupChatArea();
    void setupInputArea();
    void setupStatusBar();
    void setupLightTheme();

    // 事件处理
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *centralWidget;
    QVBoxLayout *mainLayout; // 水平布局
    QHBoxLayout *inputLayout;
    QHBoxLayout *buttonLayout;

    // 连接状态指示器
    QLabel *connectionStatusLabel;
    QLabel *connectionIcon;

    // 聊天显示区域
    QScrollArea *chatScrollArea;
    QWidget *chatContainer;
    QVBoxLayout *chatLayout;

    // 输入区域
    QTextEdit *messageEdit;
    QPushButton *sendButton;
    QPushButton *returnMainButton;
    QPushButton *updateButton;

    // AI状态显示组件
    QLabel *aiStatusLabel;
    QProgressBar *aiProgressBar;

    // AI传输任务
    AITransferTask *aiTransferTask;
    QString pythonServerUrl;
    bool isConnected;
    QString formatMessage(const QString &sender, const QString &message);
    void updateStatus(const QString &status);

    // 定时器
    QTimer *connectionTimer;

    // 设置
    bool useDarkTheme;
    int maxChatHistory;
    int connectionTimeout;
    int heartbeatInterval;

    // 系统托盘
    QSystemTrayIcon *systemTray;

    // 当前请求状态
    bool isWaitingForResponse;

    // 父窗口（用于返回主界面）
    QWidget *parentLauncher;
};

#endif // CHATWINDOW_H
