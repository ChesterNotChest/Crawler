#ifndef CRAWL_WUYI_H
#define CRAWL_WUYI_H

#include <vector>
#include <string>
#include "network/job_crawler.h"

class WebView2BrowserWRL;

namespace WuyiCrawler {
    // 使用 WebView2 捕获并解析 51Job (wuyi) 列表
    // 如果提供 externalBrowser，则使用该 WebView2 实例进行捕获（不会创建新的实例）
    std::pair<std::vector<JobInfo>, MappingData> crawlWuyi(int pageNo, int pageSize, const std::string& city = "", WebView2BrowserWRL* externalBrowser = nullptr);
}

#endif // CRAWL_WUYI_H
