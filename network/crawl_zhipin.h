#ifndef CRAWL_ZHIPIN_H
#define CRAWL_ZHIPIN_H

#include <string>
#include <map>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>
#include "constants/network_types.h"

using json = nlohmann::json;

/**
 * @file crawl_zhipin.h
 * @brief BOSS直聘爬虫模块
 * 
 * 包含BOSS直聘特定的请求头配置、URL构建和数据解析逻辑
 */

namespace ZhipinCrawler {

/**
 * @brief 获取BOSS直聘请求的HTTP头部配置
 * @return HTTP头部键值对映射
 */
std::map<std::string, std::string> getZhipinHeaders();

/**
 * @brief 构建BOSS直聘API请求URL
 * @param page 页码
 * @param pageSize 每页数量
 * @param city 城市代码（如101010100为北京）
 * @return 完整URL
 */
std::string buildZhipinUrl(int page, int pageSize, const std::string& city);

/**
 * @brief 解析BOSS直聘的JSON响应数据
 * @param json_data JSON数据
 * @return JobInfo列表和映射数据的pair
 */
std::pair<std::vector<JobInfo>, MappingData> parseZhipinResponse(const json& json_data);

/**
 * @brief BOSS直聘爬虫主函数
 * @param page 页码
 * @param pageSize 每页数量
 * @param city 城市代码（默认101010100为北京）
 * @return JobInfo列表和映射数据的pair
 */
std::pair<std::vector<JobInfo>, MappingData> crawlZhipin(int page = 1, int pageSize = 15, 
                                                          const std::string& city = "101010100");

/**
 * @brief 初始化BOSS直聘Cookie（访问主页面获取基础Cookie）
 * @param city 城市代码（默认101010100为北京）
 * @return Cookie字符串
 * @note 此函数仅获取基础Cookie（如ab_guid等），无法获取JS生成的__zp_stoken__等
 */
std::string initZhipinCookies(const std::string& city = "101010100");

} // namespace ZhipinCrawler

#endif // CRAWL_ZHIPIN_H
