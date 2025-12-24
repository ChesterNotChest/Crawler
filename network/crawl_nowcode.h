#ifndef CRAWL_NOWCODE_H
#define CRAWL_NOWCODE_H

#include <string>
#include <map>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>
#include "constants/network_types.h"

using json = nlohmann::json;

/**
 * @file crawl_nowcode.h
 * @brief 牛客网爬虫模块
 * 
 * 包含牛客网特定的请求头配置、URL构建和数据解析逻辑
 */

namespace NowcodeCrawler {

/**
 * @brief 获取牛客网请求的HTTP头部配置
 * @param recruitType 招聘类型
 * @return HTTP头部键值对映射
 */
std::map<std::string, std::string> getNowcodeHeaders(int recruitType);

/**
 * @brief 构建牛客网API请求URL
 * @return 带时间戳的完整URL
 */
std::string buildNowcodeUrl();

/**
 * @brief 构建牛客网POST请求数据
 * @param pageNo 页码
 * @param pageSize 每页数量
 * @param recruitType 招聘类型 (1=校招, 2=实习, 3=社招)
 * @return POST数据字符串
 */
std::string buildNowcodePostData(int pageNo, int pageSize, int recruitType);

/**
 * @brief 解析牛客网的JSON响应数据
 * @param json_data JSON数据
 * @return JobInfo列表和映射数据的pair
 */
std::pair<std::vector<JobInfo>, MappingData> parseNowcodeResponse(const json& json_data, int requestedRecruitType);

/**
 * @brief 牛客网爬虫主函数
 * @param pageNo 页码
 * @param pageSize 每页数量
 * @param recruitType 招聘类型 (1=校招, 2=实习, 3=社招)
 * @return JobInfo列表和映射数据的pair
 */
std::pair<std::vector<JobInfo>, MappingData> crawlNowcode(int pageNo, int pageSize, int recruitType);

} // namespace NowcodeCrawler

#endif // CRAWL_NOWCODE_H
