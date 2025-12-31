#ifndef CRAWLER_TASK_H
#define CRAWLER_TASK_H

#include "internet_task.h"
#include "sql_task.h"
#include <vector>
#include <atomic>
#include <functional>
#include <QObject>

class WebView2BrowserWRL;

/**
 * @brief CrawlerTask - 总任务协调器
 * 协调网络爬取(InternetTask)和数据存储(SqlTask)
 * 提供完整的"爬取→存储"一站式服务
 */
class CrawlerTask : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param sqlInterface SQL接口指针
     * @param sessionBrowser WebView2BrowserWRL指针（主线程创建，wuyi专用，可为nullptr）
     */
    CrawlerTask(SQLInterface *sqlInterface, WebView2BrowserWRL *sessionBrowser = nullptr);
    
    /**
     * @brief 暂停爬取
     */
    void pause();
    
    /**
     * @brief 恢复爬取
     */
    void resume();
    
    /**
     * @brief 终止爬取
     */
    void terminate();
    
    /**
     * @brief 检查是否已暂停
     */
    bool isPaused() const;
    
    /**
     * @brief 检查是否已终止
     */
    bool isTerminated() const;
    
    /**
     * @brief 设置进度回调
     * @param callback 回调函数，参数：current, total, message
     */
    void setProgressCallback(std::function<void(int, int, const std::string&)> callback);
    
    

    
    /**
     * @brief 按来源爬取所有数据（两层循环），会调用internet_task.updateCookieBySource并在失败时重试
     * @param sources 要爬取的来源列表
     * @param maxPagesPerSource 每个来源的最大页数上限（避免无限循环），0 表示无限制
     * @param pageSize 每页大小
     */
    int crawlAll(const std::vector<std::string>& sources, int maxPagesPerSource = 0, int pageSize = 15);
    
private:
    SQLInterface *m_sqlInterface;
    InternetTask m_internetTask;
    SqlTask m_sqlTask;
    std::atomic<bool> m_isPaused;
    std::atomic<bool> m_isTerminated;
    std::function<void(int, int, const std::string&)> m_progressCallback;
    WebView2BrowserWRL *m_sessionBrowser; // 非拥有指针，主线程创建
};

#endif // CRAWLER_TASK_H
