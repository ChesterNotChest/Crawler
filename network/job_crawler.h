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

using json = nlohmann::json;

// 调试信息级别
enum class DebugLevel {
    DL_DEBUG,    // 添加前缀避免冲突
    DL_ERROR     // 添加前缀避免冲突
};

// 数据结构定义
struct JobInfo {
    int64_t info_id;
    std::string info_name;
    int type_id;
    int area_id;
    int salary_level_id;
    std::string requirements;
    double salary;
    std::string contact;
    std::string create_date;
};

struct TypeInfo {
    int type_id;
    std::string type_name;
};

struct AreaInfo {
    int area_id;
    std::string area_name;
};

struct SalaryLevelInfo {
    int salary_level_id;
    int upper_limit;
};

struct MappingData {
    std::vector<TypeInfo> type_list;
    std::vector<AreaInfo> area_list;
    std::vector<SalaryLevelInfo> salary_level_list;
};

// 工具函数声明
void print_debug_info(const std::string& stage, const std::string& message,
                      const std::string& data = "", DebugLevel level = DebugLevel::DL_DEBUG);
std::string timestamp_to_datetime(int64_t timestamp);
size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response);
std::optional<json> fetch_job_data(const std::string& url, const std::map<std::string, std::string>& headers,
                                   const std::string& post_data);
std::pair<std::vector<JobInfo>, MappingData> parse_job_data(const json& json_data);
void print_data_formatted(const std::vector<JobInfo>& job_info_list,
                          const std::vector<TypeInfo>& type_list,
                          const std::vector<AreaInfo>& area_list,
                          const std::vector<SalaryLevelInfo>& salary_level_list);

#endif // JOB_CRAWLER_H
