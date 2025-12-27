#include "../cppGUI/chatwindow.h"
#include "../tasks/ai_transfer_task.h"

#include <QApplication>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QLineEdit>
#include <QScrollArea>
#include <QWidget>
#include <QFrame>
#include <QTimer>
#include <QSystemTrayIcon>
#include <QMessageBox>
#include <QIcon>
#include <QClipboard>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <QTextBrowser>
#include <QScrollBar>
#include <QSettings>
#include <QDateTime>
#include <QStatusBar>
#include <QMenu>
#include <QMainWindow>
#include <QGuiApplication>
#include <QPointer>

ChatWindow::ChatWindow(QWidget *parent)
    : QMainWindow(parent)
    , centralWidget(nullptr)
    , mainLayout(nullptr)
    , inputLayout(nullptr)
    , buttonLayout(nullptr)
    , connectionStatusLabel(nullptr)
    , connectionIcon(nullptr)
    , chatScrollArea(nullptr)
    , chatContainer(nullptr)
    , chatLayout(nullptr)
    , messageEdit(nullptr)
    , sendButton(nullptr)
    , returnMainButton(nullptr)
    , updateButton(nullptr)
    , aiTransferTask(nullptr)
    , pythonServerUrl("http://localhost:8000")
    , isConnected(false)
    , connectionTimer(nullptr)
    , useDarkTheme(false)
    , maxChatHistory(1000)
    , connectionTimeout(10000)
    , heartbeatInterval(30000)
    , systemTray(nullptr)
    , parentLauncher(nullptr)
{
    setupUI();
    setupTimers();
    setupSystemTray();
    loadSettings();
}

ChatWindow::~ChatWindow()
{
    // 断开所有信号槽连接
    disconnect();
    
    // 停止所有定时器
    if (connectionTimer) {
        connectionTimer->stop();
    }
    
    // 通知AITransferTask窗口正在关闭
    if (aiTransferTask) {
        // 通过setParent nullptr来断开父子关系，但不清除对象
        aiTransferTask->setParent(nullptr);
    }

    saveChatHistory();
    saveSettings();

    if (connectionTimer) delete connectionTimer;
    if (systemTray) delete systemTray;
}

void ChatWindow::setServerUrl(const QString &serverUrl)
{
    pythonServerUrl = serverUrl;
    updateConnectionStatusDisplay();
}

void ChatWindow::setConnected(bool connected)
{
    isConnected = connected;
    updateConnectionStatusDisplay();

    if (connected) {
        emit connectionStatusChanged(true);
    } else {
        emit connectionStatusChanged(false);
    }
}

void ChatWindow::setParentLauncher(QWidget *parentWindow)
{
    parentLauncher = parentWindow;
}

void ChatWindow::setupUI()
{
    // 创建中央窗口部件
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 主布局
    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 顶部工具栏
    setupToolbar();

    // 聊天显示区域
    setupChatArea();

    // 底部输入区域
    setupInputArea();

    // 状态栏
    setupStatusBar();

    // 设置主题
    if (useDarkTheme) {
        setupDarkTheme();
    } else {
        setupLightTheme();
    }

    // 设置窗口属性
    setWindowTitle("AI助手 - 智能对话");
    setMinimumSize(800, 600);
    resize(1000, 700);
}

void ChatWindow::setupToolbar()
{
    QHBoxLayout *toolbarLayout = new QHBoxLayout();

    // 连接状态指示
    QLabel *statusLabel = new QLabel("连接状态:", this);
    statusLabel->setStyleSheet("font-weight: bold;");
    toolbarLayout->addWidget(statusLabel);

    connectionStatusLabel = new QLabel("未连接", this);
    connectionStatusLabel->setObjectName("connectionStatusLabel");
    toolbarLayout->addWidget(connectionStatusLabel);

    connectionIcon = new QLabel("●", this);
    connectionIcon->setObjectName("connectionIcon");
    connectionIcon->setStyleSheet("color: red; font-size: 16px;");
    toolbarLayout->addWidget(connectionIcon);

    toolbarLayout->addStretch();

    // 功能按钮
    updateButton = new QPushButton("更新知识库", this);
    updateButton->setObjectName("updateButton");
    connect(updateButton, &QPushButton::clicked, this, &ChatWindow::onUpdateButtonClicked);
    toolbarLayout->addWidget(updateButton);

    mainLayout->addLayout(toolbarLayout);
}

void ChatWindow::setupChatArea()
{
    // 创建滚动区域
    chatScrollArea = new QScrollArea(this);
    chatScrollArea->setObjectName("chatScrollArea");
    chatScrollArea->setWidgetResizable(true);
    chatScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    chatScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 聊天内容容器
    chatContainer = new QWidget();
    chatContainer->setObjectName("chatContainer");
    chatLayout = new QVBoxLayout(chatContainer);
    chatLayout->setAlignment(Qt::AlignBottom); // 确保最新消息在底部
    chatLayout->setSpacing(10);

    chatScrollArea->setWidget(chatContainer);
    mainLayout->addWidget(chatScrollArea, 1); // 占剩余空间
}

void ChatWindow::setupInputArea()
{
    // 输入布局
    inputLayout = new QHBoxLayout();

    messageEdit = new QTextEdit(this);
    messageEdit->setObjectName("messageEdit");
    messageEdit->setPlaceholderText("请输入您的问题...");
    messageEdit->setMaximumHeight(100);
    messageEdit->setStyleSheet(R"(
        QTextEdit {
            border: 1px solid #ddd;
            border-radius: 5px;
            padding: 8px;
            font-size: 14px;
        }
    )");

    // 连接回车键发送消息
    connect(messageEdit, &QTextEdit::textChanged, this, [this]() {
        // 限制输入长度
        if (messageEdit->toPlainText().length() > 1000) {
            messageEdit->setPlainText(messageEdit->toPlainText().left(1000));
            QTextCursor cursor = messageEdit->textCursor();
            cursor.movePosition(QTextCursor::End);
            messageEdit->setTextCursor(cursor);
        }
    });

    // 按钮布局
    buttonLayout = new QHBoxLayout();

    sendButton = new QPushButton("发送", this);
    sendButton->setObjectName("sendButton");
    sendButton->setMinimumSize(80, 40);
    sendButton->setStyleSheet(R"(
        QPushButton {
            background-color: #007acc;
            color: white;
            border: none;
            border-radius: 5px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #005a9e;
        }
        QPushButton:pressed {
            background-color: #004578;
        }
        QPushButton:disabled {
            background-color: #cccccc;
            color: #666666;
        }
    )");
    connect(sendButton, &QPushButton::clicked, this, &ChatWindow::onSendButtonClicked);

    returnMainButton = new QPushButton("返回主界面", this);
    returnMainButton->setObjectName("returnMainButton");
    returnMainButton->setMinimumSize(100, 40);
    returnMainButton->setStyleSheet(R"(
        QPushButton {
            background-color: #dc3545;
            color: white;
            border: none;
            border-radius: 5px;
            font-size: 14px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #c82333;
        }
        QPushButton:pressed {
            background-color: #bd2130;
        }
    )");
    connect(returnMainButton, &QPushButton::clicked, this, &ChatWindow::onReturnMainButtonClicked);

    buttonLayout->addWidget(sendButton);
    buttonLayout->addWidget(returnMainButton);
    buttonLayout->addStretch();

    // 布局组合
    QVBoxLayout *inputContainerLayout = new QVBoxLayout();
    inputContainerLayout->addWidget(messageEdit);
    inputContainerLayout->addLayout(buttonLayout);

    inputLayout->addLayout(inputContainerLayout);
    mainLayout->addLayout(inputLayout);
}

void ChatWindow::setupStatusBar()
{
    statusBar()->showMessage("就绪");
    statusBar()->addPermanentWidget(new QLabel("AI助手 v1.0.0", this));
}



void ChatWindow::setupTimers()
{
    connectionTimer = new QTimer(this);
    connect(connectionTimer, &QTimer::timeout, this, &ChatWindow::onUpdateConnectionStatus);
}

void ChatWindow::setupSystemTray()
{
    if (QSystemTrayIcon::isSystemTrayAvailable()) {
        systemTray = new QSystemTrayIcon(this);
        systemTray->setIcon(QIcon(":/icons/app.png"));
        systemTray->setToolTip("AI助手");
        systemTray->show();

        // 创建系统托盘菜单
        QMenu *trayMenu = new QMenu(this);
        trayMenu->addAction("显示主窗口", this, &QMainWindow::showNormal);
        trayMenu->addAction("退出", this, &QApplication::quit);
        systemTray->setContextMenu(trayMenu);

        connect(systemTray, &QSystemTrayIcon::activated, this, &ChatWindow::onSystemTrayActivated);
    }
}

void ChatWindow::setupDarkTheme()
{
    setStyleSheet(R"(
        QMainWindow {
            background-color: #2c3e50;
            color: #ecf0f1;
        }
        QScrollArea {
            background-color: #34495e;
            border: none;
        }
        QWidget#chatContainer {
            background-color: #34495e;
        }
        QTextEdit {
            background-color: #34495e;
            border: 1px solid #4a5f7a;
            border-radius: 5px;
            color: #ecf0f1;
            padding: 8px;
        }
        QPushButton {
            background-color: #3498db;
            color: white;
            border: none;
            border-radius: 5px;
            padding: 8px 16px;
        }
        QPushButton:hover {
            background-color: #2980b9;
        }
        QPushButton:pressed {
            background-color: #21618c;
        }
    )");
}

void ChatWindow::setupLightTheme()
{
    setStyleSheet(R"(
        QMainWindow {
            background-color: #f8f9fa;
            color: #2c3e50;
        }
        QScrollArea {
            background-color: #ffffff;
            border: 1px solid #dee2e6;
            border-radius: 8px;
        }
        QWidget#chatContainer {
            background-color: #ffffff;
        }
        QTextEdit {
            background-color: #ffffff;
            border: 1px solid #ced4da;
            border-radius: 8px;
            color: #2c3e50;
            padding: 10px;
        }
        QTextEdit:focus {
            border-color: #3498db;
        }
        QPushButton {
            background-color: #3498db;
            color: white;
            border: none;
            border-radius: 6px;
            padding: 10px 20px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #2980b9;
            transform: translateY(-1px);
        }
        QPushButton:pressed {
            background-color: #21618c;
        }
        QPushButton:disabled {
            background-color: #95a5a6;
        }
    )");
}

void ChatWindow::onSendButtonClicked()
{
    QTextEdit *messageEdit = this->findChild<QTextEdit*>("messageEdit");
    if (!messageEdit) return;

    QString message = messageEdit->toPlainText().trimmed();
    if (message.isEmpty()) return;

    // 清空输入框
    messageEdit->clear();

    // 发送消息
    sendChatMessage(message);
}

void ChatWindow::onUpdateButtonClicked()
{
    if (!isConnected) {
        QMessageBox::warning(this, "连接错误", "没连接到Python的后端服务器");
        return;
    }

    updateKnowledgeBase();
}



void ChatWindow::onSystemTrayActivated(QSystemTrayIcon::ActivationReason reason)
{
    if (reason == QSystemTrayIcon::DoubleClick) {
        showNormal();
        activateWindow();
    }
}



void ChatWindow::updateConnectionStatusDisplay()
{
    if (connectionStatusLabel && connectionIcon) {
        if (isConnected) {
            connectionStatusLabel->setText("已连接");
            connectionStatusLabel->setStyleSheet("color: green; font-weight: bold;");
            connectionIcon->setStyleSheet("color: green; font-size: 16px;");
        } else {
            connectionStatusLabel->setText("未连接");
            connectionStatusLabel->setStyleSheet("color: red; font-weight: bold;");
            connectionIcon->setStyleSheet("color: red; font-size: 16px;");
        }
    }
}

void ChatWindow::loadSettings()
{
    // 从配置文件加载设置
    QSettings settings("AI助手", "ChatWindow");

    useDarkTheme = settings.value("useDarkTheme", false).toBool();
    maxChatHistory = settings.value("maxChatHistory", 1000).toInt();
    connectionTimeout = settings.value("connectionTimeout", 10000).toInt();
    heartbeatInterval = settings.value("heartbeatInterval", 30000).toInt();
}

void ChatWindow::saveSettings()
{
    QSettings settings("AI助手", "ChatWindow");

    settings.setValue("useDarkTheme", useDarkTheme);
    settings.setValue("maxChatHistory", maxChatHistory);
    settings.setValue("connectionTimeout", connectionTimeout);
    settings.setValue("heartbeatInterval", heartbeatInterval);
}

void ChatWindow::showNotification(const QString &title, const QString &message)
{
    if (systemTray) {
        systemTray->showMessage(title, message, QSystemTrayIcon::Information, 3000);
    }
}





void ChatWindow::onUpdateConnectionStatus()
{
    if (isConnected) {
        statusBar()->showMessage("连接正常");
    } else {
        statusBar()->showMessage("连接断开");
    }
}

void ChatWindow::sendChatMessage(const QString &message)
{
    if (!aiTransferTask) {
        displayMessage("系统", "AI传输任务未初始化");
        return;
    }

    if (!isConnected) {
        displayMessage("系统", "未连接到AI后端服务器，请检查服务器状态");
        return;
    }

    // 显示用户消息
    displayMessage("用户", message);

    // 禁用发送按钮
    QPushButton *sendButton = this->findChild<QPushButton*>("sendButton");
    QPushButton *returnMainButton = this->findChild<QPushButton*>("returnMainButton");
    if (sendButton) sendButton->setEnabled(false);
    if (returnMainButton) returnMainButton->setEnabled(true);

    // 显示"正在思考"提示
    displayMessage("AI", "正在思考中...");

    // 使用AITransferTask发送消息
    QPointer<ChatWindow> safeThis = this;  // 使用QPointer安全检查
    aiTransferTask->sendChatMessage(message, [safeThis](const QString &response) {
        // 检查窗口是否仍然存在
        if (!safeThis) {
            qDebug() << "ChatWindow已销毁，忽略AI响应";
            return;
        }
        
        // 恢复发送按钮
        QPushButton *sendButton = safeThis->findChild<QPushButton*>("sendButton");
        QPushButton *returnMainButton = safeThis->findChild<QPushButton*>("returnMainButton");
        if (sendButton) sendButton->setEnabled(true);
        if (returnMainButton) returnMainButton->setEnabled(false);

        // 移除"正在思考"提示
        safeThis->removeLastMessage();

        // 检查响应是否为错误信息
        if (response.contains("网络连接错误") || response.contains("错误") || response.contains("失败")) {
            safeThis->displayMessage("系统", "AI服务响应: " + response);
        } else {
            safeThis->displayMessage("AI", response);
        }
    });
}
void ChatWindow::updateKnowledgeBase()
{
    if (!aiTransferTask) {
        QMessageBox::warning(this, "错误", "AI传输任务未初始化");
        return;
    }

    // 使用AITransferTask更新知识库
    aiTransferTask->updateKnowledgeBaseFromDatabase(this, [this](bool success, const QString &message) {
        if (success) {
            QMessageBox::information(this, "成功", "知识库更新成功！");
        } else {
            QMessageBox::critical(this, "失败", QString("知识库更新失败: %1").arg(message));
        }
    });
}

void ChatWindow::loadChatHistory()
{
    // TODO: 从文件加载聊天历史
}

void ChatWindow::saveChatHistory()
{
    // TODO: 保存聊天历史到文件
}

void ChatWindow::displayMessage(const QString &sender, const QString &message)
{
    QWidget *chatContainer = this->findChild<QWidget*>("chatContainer");
    if (!chatContainer) return;

    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(chatContainer->layout());
    if (!layout) return;

    // 创建消息气泡
    QFrame *messageFrame = new QFrame(chatContainer);
    messageFrame->setFrameStyle(QFrame::Box);

    QHBoxLayout *messageLayout = new QHBoxLayout(messageFrame);
    messageLayout->setContentsMargins(15, 10, 15, 10);
    messageLayout->setSpacing(10);

    // 发送者标签
    QLabel *senderLabel = new QLabel(sender, messageFrame);
    senderLabel->setStyleSheet(R"(
        QLabel {
            font-weight: bold;
            color: #3498db;
            font-size: 12px;
        }
    )");
    messageLayout->addWidget(senderLabel);

    // 消息内容
    QLabel *contentLabel = new QLabel(formatMessage(sender, message), messageFrame);
    contentLabel->setWordWrap(true);
    contentLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    contentLabel->setStyleSheet(R"(
        QLabel {
            font-size: 14px;
            color: #2c3e50;
            background-color: #ecf0f1;
            border-radius: 8px;
            padding: 8px;
        }
    )");
    messageLayout->addWidget(contentLabel);

    // 根据发送者设置对齐
    if (sender == "用户") {
        messageLayout->addStretch();
        messageLayout->setAlignment(Qt::AlignRight);
    } else {
        messageLayout->setAlignment(Qt::AlignLeft);
    }

    // 添加到布局中（在底部添加，确保消息按时间顺序显示）
    layout->insertWidget(layout->count() - 1, messageFrame);

    // 立即滚动到底部显示最新消息
    QTimer::singleShot(50, [this]() {
        QScrollArea *scrollArea = this->findChild<QScrollArea*>("chatScrollArea");
        if (scrollArea) {
            QScrollBar *scrollBar = scrollArea->verticalScrollBar();
            scrollBar->setValue(scrollBar->maximum());
        }
    });

    // 更新状态
    updateStatus(QString("%1: %2").arg(sender).arg(message));
}

void ChatWindow::updateStatus(const QString &status)
{
    statusBar()->showMessage(status);
}

QString ChatWindow::formatMessage(const QString &sender, const QString &message)
{
    // 简单的消息格式化，支持基本的表情符号和换行
    QString formatted = message;

    // 替换常见表情符号
    formatted.replace(":)", "😊");
    formatted.replace(":(", "😢");
    formatted.replace(":D", "😃");
    formatted.replace(";)", "😉");
    formatted.replace(":(", "😔");

    // 处理换行符
    formatted.replace("\n", "<br>");

    // 处理长文本的换行
    if (formatted.length() > 50 && !formatted.contains("<br>")) {
        // 在50个字符后添加软换行
        QStringList lines;
        QString currentLine;
        for (int i = 0; i < formatted.length(); i++) {
            currentLine += formatted[i];
            if (currentLine.length() >= 50 && formatted[i] == ' ') {
                lines.append(currentLine.trimmed());
                currentLine.clear();
            }
        }
        if (!currentLine.isEmpty()) {
            lines.append(currentLine);
        }
        formatted = lines.join("<br>");
    }

    return formatted;
}



void ChatWindow::removeLastMessage()
{
    QWidget *chatContainer = this->findChild<QWidget*>("chatContainer");
    if (!chatContainer) return;

    QVBoxLayout *layout = qobject_cast<QVBoxLayout*>(chatContainer->layout());
    if (!layout) return;

    if (layout->count() > 1) {
        QLayoutItem *lastItem = layout->itemAt(layout->count() - 2);
        if (lastItem && lastItem->widget()) {
            delete lastItem->widget();
        }
        layout->removeItem(lastItem);
    }
}

void ChatWindow::onReturnMainButtonClicked()
{
    qDebug() << "返回主界面按钮被点击，准备返回主界面";
    
    // 如果正在进行知识库更新，先取消操作
    if (aiTransferTask && aiTransferTask->isRunning()) {
        qDebug() << "正在取消知识库更新操作";
        aiTransferTask->cancelOperation();
    }
    
    // 确保在关闭前先显示父窗口
    if (parentLauncher) {
        qDebug() << "显示父窗口（LauncherWindow）";
        parentLauncher->show();
        parentLauncher->activateWindow();
        parentLauncher->raise();
    }
    
    // 发出返回主窗口的信号
    emit returnToMainWindow();
    
    // 延迟关闭当前窗口，确保父窗口有时间显示
    QTimer::singleShot(100, this, [this]() {
        close();
    });
}

void ChatWindow::closeEvent(QCloseEvent *event)
{
    qDebug() << "ChatWindow::closeEvent 被调用";

    // 如果用户点击了窗口关闭按钮(X)，则返回主界面而不是退出程序
    if (parentLauncher) {
        qDebug() << "用户点击关闭按钮，返回主界面";
        event->ignore(); // 忽略关闭事件

        // 显示父窗口
        parentLauncher->show();
        parentLauncher->activateWindow();
        parentLauncher->raise();

        // 隐藏当前窗口而不是关闭
        hide();

        // 发出返回主窗口的信号
        emit returnToMainWindow();
    } else {
        // 如果没有父窗口，则正常关闭
        qDebug() << "没有父窗口，正常关闭应用";
        QMainWindow::closeEvent(event);
    }
}

void ChatWindow::setAITransferTask(AITransferTask *task)
{
    aiTransferTask = task;
    if (task) {
        // 连接AITransferTask的信号
        connect(task, &AITransferTask::transferCompleted, this, &ChatWindow::onAITransferCompleted);
        connect(task, &AITransferTask::progressUpdated, this, [this](int current, int total, const QString &status) {
            statusBar()->showMessage(status);
        });
    }
}

void ChatWindow::onAITransferCompleted(bool success, const QString &message)
{
    qDebug() << "AI传输任务完成 - 成功:" << success << ", 消息:" << message;
    
    // 知识库更新完成后，确保保持在当前聊天界面
    if (success) {
        qDebug() << "知识库更新成功，保持在AI聊天界面";
        statusBar()->showMessage("知识库更新完成！您可以继续与AI对话", 5000);
        
        // 启用输入框和发送按钮，允许用户继续聊天
        messageEdit->setEnabled(true);
        sendButton->setEnabled(true);
        returnMainButton->setEnabled(false);
        updateButton->setEnabled(true);
        
        
    } else {
        qDebug() << "知识库更新失败，保持在当前界面";
        statusBar()->showMessage("知识库更新失败: " + message, 5000);
        
        // 启用输入框和按钮，允许用户重试
        messageEdit->setEnabled(true);
        sendButton->setEnabled(true);
        returnMainButton->setEnabled(false);
        updateButton->setEnabled(true);

    }
    
    qDebug() << "AI传输任务处理完成，聊天界面保持活跃状态";
}
