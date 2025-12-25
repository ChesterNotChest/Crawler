#ifndef CRAWL_LIEPIN_H
#define CRAWL_LIEPIN_H

#include <vector>
#include <string>
#include <utility>
#include <QEventLoop>
#include <QObject>
#include "../constants/network_types.h"

namespace LiepinCrawler {

// 启动一次WebView2页面，捕获后台API响应并保存原始JSON；
// 当前不做解析，返回空结果。调用者应在收到提示后确认映射，然后请求解析实现。
std::pair<std::vector<JobInfo>, MappingData> crawlLiepin(int pageNo, int pageSize, const std::string& city = "410");

}

#endif // CRAWL_LIEPIN_H
