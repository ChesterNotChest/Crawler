#include "launcherwindow.h"
#include <QApplication>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QPropertyAnimation>
#include <QParallelAnimationGroup>
#include <QTimer>
#include <QIcon>
#include <QPixmap>
#include <QFont>
#include <QPalette>
#include <QColor>
#include <QStyleFactory>
#include <QGuiApplication>
#include <QScreen>
#include <QMessageBox>
#include "../tasks/backend_manager_task.h"
#include "chatwindow.h"
#include "tasks/ai_transfer_task.h"
#include "crawlerwindow.h"

LauncherWindow::LauncherWindow(QWidget *parent)
    : QMainWindow(parent)
    , centralWidget(nullptr)
    , mainLayout(nullptr)
    , titleLabel(nullptr)
    , subtitleLabel(nullptr)
    , versionLabel(nullptr)
    , aiChatButton(nullptr)
    , crawlerButton(nullptr)
    , settingsButton(nullptr)
    , aboutButton(nullptr)
    , exitButton(nullptr)
    , titleAnimation(nullptr)
    , buttonAnimation(nullptr)
    , versionCheckTimer(nullptr)
    , backendManager(nullptr)
    , backendConnected(false)
{
    // 应用信息
    appVersion = "1.1.13";
    updateUrl = "https://github.com/ChesterNotChest/Crawler";
    autoCheckUpdates = true;

    // Python后端服务器配置
    pythonServerUrl = "http://localhost:8000";  // Python服务器地址

    // 初始化后端管理器
    backendManager = new BackendManagerTask(this);
    backendManager->setPythonServerUrl(pythonServerUrl);

    setupUI();
    setupAnimations();
    setupStyle();
    centerWindow();
    loadConfiguration();

    // 连接后端管理器信号
    connect(backendManager, &BackendManagerTask::connectionStatusChanged,
            this, &LauncherWindow::onBackendConnectionChanged);

    // 检查更新
    if (autoCheckUpdates) {
        checkUpdates();
    }

    // 开始按钮动画
    QTimer::singleShot(500, this, &LauncherWindow::animateButtons);
}

LauncherWindow::~LauncherWindow()
{
    // 断开所有信号槽连接，防止析构期间调用
    disconnect();

    // 停止所有定时器
    if (versionCheckTimer) {
        versionCheckTimer->stop();
    }

    // 移除动画目标以防止析构期间调用
    if (titleAnimation) {
        titleAnimation->setTargetObject(nullptr);
    }

    // 删除动画对象
    delete titleAnimation;
    delete buttonAnimation;
    delete versionCheckTimer;

    // BackendManagerTask由Qt自动管理内存（this作为父对象）
}

void LauncherWindow::setupUI()
{
    // 创建中央窗口部件
    centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    // 主布局
    mainLayout = new QVBoxLayout(centralWidget);
    mainLayout->setAlignment(Qt::AlignCenter);
    mainLayout->setSpacing(30);
    mainLayout->setContentsMargins(50, 50, 50, 50);

    // 标题区域
    setupTitleArea();

    // 功能按钮区域
    setupFunctionButtons();

    // 版本信息
    setupVersionInfo();
}

void LauncherWindow::setupTitleArea()
{
    // 主标题
    titleLabel = new QLabel("AI助手", this);
    titleLabel->setAlignment(Qt::AlignCenter);
    titleLabel->setStyleSheet(R"(
        QLabel {
            font-size: 48px;
            font-weight: bold;
            color: #2c3e50;
            margin: 20px 0px;
        }
    )");
    mainLayout->addWidget(titleLabel);

    // 副标题
    subtitleLabel = new QLabel("智能就业助手，助您找到理想工作", this);
    subtitleLabel->setAlignment(Qt::AlignCenter);
    subtitleLabel->setStyleSheet(R"(
        QLabel {
            font-size: 18px;
            color: #7f8c8d;
            margin: 10px 0px 30px 0px;
        }
    )");
    mainLayout->addWidget(subtitleLabel);
}

void LauncherWindow::setupFunctionButtons()
{
    // AI聊天按钮
    aiChatButton = createFunctionCard(
        "AI智能对话",
        "与AI助手进行智能对话，获得个性化建议",
        "🤖"
        );
    connect(aiChatButton, &QPushButton::clicked, this, &LauncherWindow::onAIChatButtonClicked);
    mainLayout->addWidget(aiChatButton);

    // 爬虫功能按钮
    crawlerButton = createFunctionCard(
        "职位爬虫",
        "智能爬取各大招聘网站职位信息",
        "🕷️"
        );
    connect(crawlerButton, &QPushButton::clicked, this, &LauncherWindow::onCrawlerButtonClicked);
    mainLayout->addWidget(crawlerButton);

    // 设置按钮
    settingsButton = createFunctionCard(
        "系统设置",
        "配置应用参数和个性化选项",
        "⚙️"
        );
    connect(settingsButton, &QPushButton::clicked, this, &LauncherWindow::onSettingsButtonClicked);
    mainLayout->addWidget(settingsButton);

    // 关于按钮
    aboutButton = createFunctionCard(
        "关于应用",
        "查看版本信息和开发团队",
        "ℹ️"
        );
    connect(aboutButton, &QPushButton::clicked, this, &LauncherWindow::onAboutButtonClicked);
    mainLayout->addWidget(aboutButton);

    // 退出按钮
    exitButton = new QPushButton("退出应用", this);
    exitButton->setFixedSize(200, 50);
    exitButton->setStyleSheet(R"(
        QPushButton {
            background-color: #e74c3c;
            color: white;
            border: none;
            border-radius: 25px;
            font-size: 16px;
            font-weight: bold;
        }
        QPushButton:hover {
            background-color: #c0392b;
        }
        QPushButton:pressed {
            background-color: #a93226;
        }
    )");
    connect(exitButton, &QPushButton::clicked, this, &LauncherWindow::onExitButtonClicked);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    buttonLayout->addWidget(exitButton);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
}

void LauncherWindow::setupVersionInfo()
{
    versionLabel = new QLabel(QString("版本 %1").arg(appVersion), this);
    versionLabel->setAlignment(Qt::AlignCenter);
    versionLabel->setStyleSheet(R"(
        QLabel {
            font-size: 12px;
            color: #95a5a6;
            margin-top: 20px;
        }
    )");
    mainLayout->addWidget(versionLabel);
}

QPushButton* LauncherWindow::createFunctionCard(const QString &title, const QString &description, const QString &icon)
{
    QPushButton *button = new QPushButton(this);
    button->setFixedSize(400, 80);

    // 设置按钮样式
    button->setStyleSheet(R"(
        QPushButton {
            background-color: #ffffff;
            border: 2px solid #ecf0f1;
            border-radius: 15px;
            padding: 15px;
            text-align: left;
        }
        QPushButton:hover {
            background-color: #f8f9fa;
            border-color: #3498db;
            transform: translateY(-2px);
        }
        QPushButton:pressed {
            background-color: #e9ecef;
            border-color: #2980b9;
        }
    )");

    // 创建卡片布局
    QHBoxLayout *cardLayout = new QHBoxLayout(button);
    cardLayout->setContentsMargins(20, 15, 20, 15);
    cardLayout->setSpacing(15);

    // 图标
    QLabel *iconLabel = new QLabel(icon, this);
    iconLabel->setFixedSize(50, 50);
    iconLabel->setAlignment(Qt::AlignCenter);
    iconLabel->setStyleSheet("font-size: 24px;");
    cardLayout->addWidget(iconLabel);

    // 文字内容
    QVBoxLayout *textLayout = new QVBoxLayout();
    textLayout->setSpacing(5);

    QLabel *titleLabel = new QLabel(title, this);
    titleLabel->setStyleSheet(R"(
        QLabel {
            font-size: 18px;
            font-weight: bold;
            color: #2c3e50;
            background: transparent;
        }
    )");

    QLabel *descLabel = new QLabel(description, this);
    descLabel->setStyleSheet(R"(
        QLabel {
            font-size: 12px;
            color: #7f8c8d;
            background: transparent;
        }
    )");

    textLayout->addWidget(titleLabel);
    textLayout->addWidget(descLabel);

    cardLayout->addLayout(textLayout);
    cardLayout->addStretch();

    return button;
}

void LauncherWindow::setupAnimations()
{
    // 标题动画
    titleAnimation = new QPropertyAnimation(titleLabel, "geometry", this);
    titleAnimation->setDuration(1000);
    titleAnimation->setEasingCurve(QEasingCurve::OutCubic);

    // 按钮动画
    buttonAnimation = new QPropertyAnimation();
    buttonAnimation->setDuration(800);
    buttonAnimation->setEasingCurve(QEasingCurve::OutCubic);

    // 版本检查定时器
    versionCheckTimer = new QTimer(this);
    connect(versionCheckTimer, &QTimer::timeout, this, &LauncherWindow::onVersionCheckTimer);
}

void LauncherWindow::setupStyle()
{
    // 设置窗口样式
    setStyleSheet(R"(
        LauncherWindow {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f8f9fa, stop:1 #e9ecef);
        }
    )");

    // 设置窗口属性
    setWindowTitle("AI助手 - 启动器");
    setMinimumSize(600, 700);
    setMaximumSize(800, 900);

    // 居中显示
    centerWindow();
}

void LauncherWindow::centerWindow()
{
    QScreen *screen = QGuiApplication::primaryScreen();
    QRect screenGeometry = screen->availableGeometry();
    int x = (screenGeometry.width() - width()) / 2;
    int y = (screenGeometry.height() - height()) / 2;
    move(x, y);
}

void LauncherWindow::loadConfiguration()
{
    // 加载配置文件
    // TODO: 从配置文件加载设置
}

void LauncherWindow::checkUpdates()
{
    // 检查应用更新
    // TODO: 实现版本检查逻辑
}

void LauncherWindow::animateButtons()
{
    // 启动按钮淡入动画
    QList<QPushButton*> buttons = {aiChatButton, crawlerButton, settingsButton, aboutButton};

    for (int i = 0; i < buttons.size(); ++i) {
        QPropertyAnimation *animation = new QPropertyAnimation(buttons[i], "opacity", this);
        animation->setDuration(600);
        animation->setStartValue(0.0);
        animation->setEndValue(1.0);

        // 使用定时器延迟启动动画，而不是setStartTime
        QTimer *delayTimer = new QTimer();
        delayTimer->setSingleShot(true);
        connect(delayTimer, &QTimer::timeout, [animation, delayTimer]() {
            if (animation) {
                animation->start();
            }
            delayTimer->deleteLater();
        });
        delayTimer->start(i * 200);
    }
}

void LauncherWindow::onAIChatButtonClicked()
{
    // 如果后端未连接，先连接Python服务器
    if (!backendConnected) {
        backendManager->connectToBackend([this](bool success, const QString &message) {
            if (success) {
                openChatWindow();
            } else {
                QMessageBox::warning(this, "连接失败", message);
            }
        });
    } else {
        // 直接打开聊天窗口
        openChatWindow();
    }
}

void LauncherWindow::onBackendConnectionChanged(bool connected)
{
    backendConnected = connected;

    if (connected) {
        QMessageBox::information(this, "连接成功", "已成功连接到AI后端服务器！");
    } else {
        QMessageBox::warning(this, "连接断开",
                             "与AI后端服务器的连接已断开，请检查服务器状态。");
    }
}

void LauncherWindow::openChatWindow()
{
    // 创建并显示聊天窗口
    ChatWindow *chatWindow = new ChatWindow(this);
    chatWindow->setServerUrl(pythonServerUrl);
    chatWindow->setConnected(backendConnected);
    chatWindow->setParentLauncher(this); // 设置父窗口，用于返回主界面

    // 创建并设置AI传输任务
    AITransferTask *aiTask = new AITransferTask(this);
    aiTask->setPythonServerUrl(pythonServerUrl);
    chatWindow->setAITransferTask(aiTask);

    // 连接返回信号，确保ChatWindow关闭时LauncherWindow能正确显示
    connect(chatWindow, &ChatWindow::returnToMainWindow, this, [this]() {
        qDebug() << "收到ChatWindow返回主窗口信号，显示LauncherWindow";
        this->showNormal();
        this->activateWindow();
        this->raise();
    });

    chatWindow->show();
    hide(); // 隐藏启动器窗口
}

void LauncherWindow::onCrawlerButtonClicked()
{
    // 打开爬虫界面
    CrawlerWindow *crawler = new CrawlerWindow(this);
    // 当爬虫窗口关闭或返回时，显示Launcher
    connect(crawler, &CrawlerWindow::returnToLauncher, this, [this, crawler]() {
        this->showNormal();
        this->activateWindow();
        this->raise();
        crawler->deleteLater();
    });

    crawler->show();
    hide();
}

void LauncherWindow::onSettingsButtonClicked()
{
    // TODO: 实现设置窗口
    QMessageBox::information(this, "功能开发中", "系统设置功能正在开发中，敬请期待！");
}

void LauncherWindow::onAboutButtonClicked()
{
    QMessageBox::about(this, "关于AI助手",
                       "AI助手 v1.0.0\n\n"
                       "智能就业助手，助您找到理想工作\n\n"
                       "开发团队：AI开发团队\n"
                       "技术支持：Qt + Python + AI\n"
                       "© 2025 AI开发团队");
}

void LauncherWindow::onExitButtonClicked()
{
    QApplication::quit();
}

void LauncherWindow::onVersionCheckTimer()
{
    // 简单的版本检查逻辑
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);
    QUrl url("https://api.github.com/repos/your-repo/your-project/releases/latest");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "AI-Assistant/1.0");

    QNetworkReply *reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply, manager]() {
        if (reply->error() == QNetworkReply::NoError) {
            QJsonParseError error;
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll(), &error);
            if (error.error == QJsonParseError::NoError && doc.isObject()) {
                QJsonObject obj = doc.object();
                QString latestVersion = obj["tag_name"].toString();
                QString currentVersion = "1.0.0"; // 当前版本

                if (latestVersion > currentVersion) {
                    QMessageBox::information(this, "版本更新",
                                             QString("发现新版本 %1\n当前版本：%2\n请访问下载页面获取最新版本").arg(latestVersion).arg(currentVersion));
                }
            }
        }
        reply->deleteLater();
        manager->deleteLater();
    });
}

