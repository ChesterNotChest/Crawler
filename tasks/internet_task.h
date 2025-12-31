#ifndef INTERNET_TASK_H
#define INTERNET_TASK_H

#include <vector>
#include <map>
#include <string>
#include "network/job_crawler.h"
#include "network/crawl_nowcode.h"
#include "network/crawl_zhipin.h"
#include "network/crawl_chinahr.h"
#include "network/crawl_wuyi.h"
#include "network/webview2_browser_wrl.h"
#include "network/crawl_liepin.h"
#include "config/config_manager.h"

/**
 * @brief InternetTask - 负责网络爬虫操作
 * 封装爬虫调用过程，提供统一的网络数据获取接口
 */
class InternetTask {
public:
    InternetTask();
    
    /**
     * @brief 从网络爬取职位数据（牛客网）
     * @param pageNo 页码
     * @param pageSize 每页数量
     * @param recruitType 招聘类型 (1=校招, 2=实习, 3=社招，默认=1)
     * @return 爬取到的JobInfo列表和映射数据
     */
    std::pair<std::vector<JobInfo>, MappingData> fetchJobData(int pageNo, int pageSize, int recruitType = DEFAULT_RECRUIT_TYPE);
    
    /**
     * @brief 批量爬取多页数据（牛客网）
     * @param startPage 起始页
     * @param endPage 结束页
     * @param pageSize 每页数量
     * @param recruitType 招聘类型 (1=校招, 2=实习, 3=社招，默认=1)
     * @return 所有页的JobInfo列表和合并的映射数据
     */
    std::pair<std::vector<JobInfo>, MappingData> fetchJobDataMultiPage(
        int startPage, int endPage, int pageSize, int recruitType = DEFAULT_RECRUIT_TYPE);
    
    /**
     * @brief 按指定数据来源爬取数据
     * @param sourceCode 数据源代码 ("nowcode" 或 "zhipin")
     * @param pageNo 页码
     * @param pageSize 每页数量
     * @param recruitType 招聘类型（仅对nowcode有效）
     * @return 爬取到的JobInfo列表和映射数据
     */
    std::pair<std::vector<JobInfo>, MappingData> fetchBySource(
        const std::string& sourceCode, int pageNo, int pageSize, int recruitType = DEFAULT_RECRUIT_TYPE,
        const std::string& city = "");

    // 同步版本：为需要保持会话的来源（如 wuyi）提供一个能传入外部 WebView2 实例的重载
    std::pair<std::vector<JobInfo>, MappingData> fetchBySource(
        const std::string& sourceCode, int pageNo, int pageSize, WebView2BrowserWRL* browser,
        int recruitType = DEFAULT_RECRUIT_TYPE, const std::string& city = "");

    /**
     * @brief 根据数据源执行Cookie更新（使用WebView2等方式），并将结果写入config.json
     * @param sourceCode "nowcode" 或 "zhipin"
     * @param timeoutMs 等待cookie获取的超时时间（ms）
     * @return 成功返回true
     */
    bool updateCookieBySource(const std::string& sourceCode, int timeoutMs = 15000);
    // Overload: use an existing WebView2 instance (created in GUI thread) to fetch cookies.
    bool updateCookieBySource(const std::string& sourceCode, WebView2BrowserWRL* browser, int timeoutMs = 15000);
    
    /**
     * @brief 爬取所有启用的数据源
     * @param pageNo 页码
     * @param pageSize 每页数量
     * @return 所有数据源的JobInfo列表和合并的映射数据
     */
    std::pair<std::vector<JobInfo>, MappingData> crawlAll(int pageNo, int pageSize);
    // 返回被永久挂起（hault）的数据源及其失败页码
    const std::map<std::string, std::vector<int>>& getHaltedPages() const { return halted_pages_; }
    
private:
    // 爬虫配置参数可以在这里管理
    static constexpr int DEFAULT_PAGE_SIZE = 15;
    static constexpr int DEFAULT_RECRUIT_TYPE = 1; // 校招
    // 记录被永久挂起的源与失败页码
    std::map<std::string, std::vector<int>> halted_pages_;
};

#endif // INTERNET_TASK_H
