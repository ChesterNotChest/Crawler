#ifndef JOB_CRAWLER_H
#define JOB_CRAWLER_H

#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <memory>
#include <optional>

// 第三方库包含
#include <curl/curl.h>
#include <nlohmann/json.hpp>

// 数据结构定义
#include "constants/network_types.h"

using json = nlohmann::json;

// 工具函数声明
void print_debug_info(const std::string& stage, const std::string& message,
                      const std::string& data = "", DebugLevel level = DebugLevel::DL_DEBUG);
std::string timestamp_to_datetime(int64_t timestamp);
size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response);
size_t header_callback(char* buffer, size_t size, size_t nitems, std::string* headers);
std::string fetch_cookies_from_page(const std::string& url);
std::optional<json> fetch_job_data(const std::string& url, const std::map<std::string, std::string>& headers,
                                   const std::string& post_data);
std::pair<std::vector<JobInfo>, MappingData> parse_job_data(const json& json_data);
std::string sanitize_html_to_text(const std::string& html);
void print_data_formatted(const std::vector<JobInfo>& job_info_list,
                          const std::vector<TypeInfo>& type_list,
                          const std::vector<AreaInfo>& area_list,
                          const std::vector<SalaryLevelInfo>& salary_level_list);

#endif // JOB_CRAWLER_H
