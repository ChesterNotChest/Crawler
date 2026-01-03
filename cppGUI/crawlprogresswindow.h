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
#include <QString>
#include "tasks/crawler_task.h"
#include "db/sqlinterface.h"

class CrawlProgressWindow : public QMainWindow {
    Q_OBJECT

public:
    CrawlProgressWindow(const std::vector<std::string>& sources, const std::vector<int>& maxPagesList, QWidget *parent = nullptr);
    ~CrawlProgressWindow();

signals:
    void crawlFinished(const QString &summary);

private slots:
    void onPauseResumeClicked();
    void onTerminateClicked();
    void updateProgress(int current, int total, const QString& message);
    void updateSubProgress(int currentPage, int expectedPages);
    void updateSourceProgress(int sourceIndex, double fraction);

private:
    void startCrawling();

    QProgressBar *progressBar;
    QProgressBar *subProgressBar; // per-site progress
    QPushButton *pauseResumeButton;
    QPushButton *terminateButton;
    QLabel *statusLabel;

    bool m_emittedFinished;

    std::vector<std::string> m_sources;
    std::vector<int> m_maxPagesList;

    CrawlerTask *m_crawlerTask;
    SQLInterface *m_sqlInterface;
    QThread *m_crawlThread;
    WebView2BrowserWRL *m_sessionBrowser;
    QVector<double> m_sourceFractions;
};

#endif // CRAWLPROGRESSWINDOW_H