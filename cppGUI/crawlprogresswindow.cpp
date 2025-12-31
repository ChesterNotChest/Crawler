#include "crawlprogresswindow.h"
#include <QApplication>
#include <QDebug>
#include "presenter/presenter.h"

// 新增 m_sessionBrowser 初始化
CrawlProgressWindow::CrawlProgressWindow(const std::vector<std::string>& sources, int maxPages, QWidget *parent)
    : QMainWindow(parent), m_sources(sources), m_maxPages(maxPages), m_crawlerTask(nullptr), m_sqlInterface(nullptr), m_crawlThread(nullptr), m_sessionBrowser(nullptr) {
        // 只需创建一次，主线程 new，避免子线程 new QWidget
        m_sessionBrowser = new WebView2BrowserWRL();
    setWindowTitle("爬取进度");
    setFixedSize(400, 200);

    QWidget *central = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(central);

    progressBar = new QProgressBar;
    progressBar->setRange(0, sources.size());
    progressBar->setValue(0);
    layout->addWidget(progressBar);

    statusLabel = new QLabel("准备开始...");
    layout->addWidget(statusLabel);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    pauseResumeButton = new QPushButton("暂停");
    connect(pauseResumeButton, &QPushButton::clicked, this, &CrawlProgressWindow::onPauseResumeClicked);
    buttonLayout->addWidget(pauseResumeButton);

    terminateButton = new QPushButton("终止");
    connect(terminateButton, &QPushButton::clicked, this, &CrawlProgressWindow::onTerminateClicked);
    buttonLayout->addWidget(terminateButton);

    layout->addLayout(buttonLayout);

    setCentralWidget(central);

    startCrawling();
}

// 析构时释放 sessionBrowser
CrawlProgressWindow::~CrawlProgressWindow() {
        if (m_sessionBrowser) {
            delete m_sessionBrowser;
        }
    if (m_crawlThread) {
        m_crawlThread->quit();
        m_crawlThread->wait();
        delete m_crawlThread;
    }
    if (m_crawlerTask) {
        delete m_crawlerTask;
    }
    if (m_sqlInterface) {
        m_sqlInterface->disconnect();
        delete m_sqlInterface;
    }
}

void CrawlProgressWindow::startCrawling() {
    m_sqlInterface = new SQLInterface;
    if (!m_sqlInterface->connectSqlite(Presenter::DEFAULT_DB_PATH)) {
        qDebug() << "Failed to connect to database";
        statusLabel->setText("数据库连接失败");
        return;
    }

    m_crawlerTask = new CrawlerTask(m_sqlInterface, m_sessionBrowser);
    m_crawlerTask->setProgressCallback([this](int current, int total, const std::string& message) {
        // 由于回调在另一个线程，需使用信号槽或invoke
        QMetaObject::invokeMethod(this, "updateProgress", Qt::QueuedConnection,
                                   Q_ARG(int, current), Q_ARG(int, total), Q_ARG(QString, QString::fromStdString(message)));
    });

    m_crawlThread = new QThread;
    m_crawlerTask->moveToThread(m_crawlThread);

    connect(m_crawlThread, &QThread::started, [this]() {
        int stored = m_crawlerTask->crawlAll(m_sources, m_maxPages, 15);
        qDebug() << "Crawling completed, stored:" << stored;
        QMetaObject::invokeMethod(this, "updateProgress", Qt::QueuedConnection,
                       Q_ARG(int, m_sources.size()), Q_ARG(int, m_sources.size()), Q_ARG(QString, QString::fromUtf8("完成")));
    });

    connect(m_crawlThread, &QThread::finished, m_crawlThread, &QThread::deleteLater);

    m_crawlThread->start();
}

void CrawlProgressWindow::onPauseResumeClicked() {
    if (!m_crawlerTask) return;
    if (m_crawlerTask->isPaused()) {
        m_crawlerTask->resume();
        pauseResumeButton->setText("暂停");
    } else {
        m_crawlerTask->pause();
        pauseResumeButton->setText("恢复");
    }
}

void CrawlProgressWindow::onTerminateClicked() {
    if (m_crawlerTask) {
        m_crawlerTask->terminate();
    }
    close();
}

void CrawlProgressWindow::updateProgress(int current, int total, const QString& message) {
    progressBar->setValue(current);
    statusLabel->setText(message);
    if (current >= total) {
        pauseResumeButton->setEnabled(false);
        terminateButton->setText("关闭");
        disconnect(terminateButton, &QPushButton::clicked, this, &CrawlProgressWindow::onTerminateClicked);
        connect(terminateButton, &QPushButton::clicked, this, &QWidget::close);
    }
}