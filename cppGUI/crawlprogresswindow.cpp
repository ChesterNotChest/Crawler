#include "crawlprogresswindow.h"
#include <QApplication>
#include <QDebug>
#include <cmath>
#include "presenter/presenter.h"

// 新增 m_sessionBrowser 初始化
CrawlProgressWindow::CrawlProgressWindow(const std::vector<std::string>& sources, const std::vector<int>& maxPagesList, QWidget *parent)
    : QMainWindow(parent), m_sources(sources), m_maxPagesList(maxPagesList), m_crawlerTask(nullptr), m_sqlInterface(nullptr), m_crawlThread(nullptr), m_sessionBrowser(nullptr) {
        // 只需创建一次，主线程 new，避免子线程 new QWidget
        m_sessionBrowser = new WebView2BrowserWRL();
    setWindowTitle("爬取进度");
    setFixedSize(400, 200);

    QWidget *central = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(central);

    progressBar = new QProgressBar;
    progressBar->setRange(0, 100);
    progressBar->setValue(0);
    layout->addWidget(progressBar);

    // 子进度条，用于显示当前来源的页码进度
    subProgressBar = new QProgressBar;
    subProgressBar->setRange(0, 100);
    subProgressBar->setValue(0);
    layout->addWidget(subProgressBar);

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
    // 子进度回调： currentPage, expectedPages
    m_crawlerTask->setSubProgressCallback([this](int currentPage, int expectedPages){
        QMetaObject::invokeMethod(this, "updateSubProgress", Qt::QueuedConnection,
                                   Q_ARG(int, currentPage), Q_ARG(int, expectedPages));
    });
    // source-level fractional progress updates (0.0 - 1.0)
    m_sourceFractions.resize(m_sources.size());
    for (int i=0;i<m_sourceFractions.size();++i) m_sourceFractions[i] = 0.0;
    m_crawlerTask->setSourceProgressCallback([this](size_t sourceIndex, double fraction){
        // ensure GUI thread; pass sourceIndex as int
        QMetaObject::invokeMethod(this, "updateSourceProgress", Qt::QueuedConnection,
                                   Q_ARG(int, static_cast<int>(sourceIndex)), Q_ARG(double, fraction));
    });

    m_crawlThread = new QThread;
    m_crawlerTask->moveToThread(m_crawlThread);

    connect(m_crawlThread, &QThread::started, [this]() {
        // ensure m_maxPagesList matches sources size: pad with 0 (no limit) if needed
        std::vector<int> list = m_maxPagesList;
        if (list.size() < m_sources.size()) list.resize(m_sources.size(), 0);
        int stored = m_crawlerTask->crawlAll(m_sources, list, 15);
        qDebug() << "Crawling completed, stored:" << stored;
        // Note: do NOT override the final summary message emitted by CrawlerTask via m_progressCallback.
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
    // keep status message only; overall progress handled by source fractions
    statusLabel->setText(message);
    if (current >= total) {
        pauseResumeButton->setEnabled(false);
        terminateButton->setText("关闭");
        disconnect(terminateButton, &QPushButton::clicked, this, &CrawlProgressWindow::onTerminateClicked);
        connect(terminateButton, &QPushButton::clicked, this, &QWidget::close);
    }
}

void CrawlProgressWindow::updateSubProgress(int currentPage, int expectedPages) {
    if (expectedPages <= 0) {
        // no expectation: show 0..100% based on single page presence (0 or 100)
        subProgressBar->setRange(0, 1);
        subProgressBar->setValue(currentPage > 0 ? 1 : 0);
        return;
    }
    int val = qMin(currentPage, expectedPages);
    // map to 0..100
    int percent = (expectedPages > 0) ? static_cast<int>( (100.0 * val) / expectedPages ) : 0;
    if (val >= expectedPages) percent = 100;
    subProgressBar->setRange(0, 100);
    subProgressBar->setValue(percent);
}

void CrawlProgressWindow::updateSourceProgress(int sourceIndex, double fraction) {
    if (sourceIndex < 0 || sourceIndex >= m_sourceFractions.size()) return;
    m_sourceFractions[sourceIndex] = fraction;
    // update sub progress to reflect this source fraction
    int subPercent = static_cast<int>(std::round(fraction * 100.0));
    subProgressBar->setRange(0, 100);
    subProgressBar->setValue(subPercent);

    // compute overall progress as average of source fractions
    double sum = 0.0;
    for (double v : m_sourceFractions) sum += v;
    double overallFraction = (m_sourceFractions.isEmpty() ? 0.0 : (sum / m_sourceFractions.size()));
    int overallPercent = static_cast<int>(std::round(overallFraction * 100.0));
    progressBar->setValue(overallPercent);
}