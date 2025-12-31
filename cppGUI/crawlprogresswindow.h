#ifndef CRAWLPROGRESSWINDOW_H
#define CRAWLPROGRESSWINDOW_H

#include "network/webview2_browser_wrl.h"
#include <QMainWindow>
#include <QProgressBar>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QThread>
#include "tasks/crawler_task.h"
#include "db/sqlinterface.h"

class CrawlProgressWindow : public QMainWindow {
    Q_OBJECT

public:
    CrawlProgressWindow(const std::vector<std::string>& sources, int maxPages, QWidget *parent = nullptr);
    ~CrawlProgressWindow();

private slots:
    void onPauseResumeClicked();
    void onTerminateClicked();
    void updateProgress(int current, int total, const QString& message);

private:
    void startCrawling();

    QProgressBar *progressBar;
    QPushButton *pauseResumeButton;
    QPushButton *terminateButton;
    QLabel *statusLabel;

    std::vector<std::string> m_sources;
    int m_maxPages;

    CrawlerTask *m_crawlerTask;
    SQLInterface *m_sqlInterface;
    QThread *m_crawlThread;
    WebView2BrowserWRL *m_sessionBrowser;
};

#endif // CRAWLPROGRESSWINDOW_H