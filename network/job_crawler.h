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
    int64_t info_id;              // 岗位ID
    std::string info_name;        // 岗位名称
    int type_id;                  // 招聘类型ID (1=校招, 2=实习, 3=社招)
    int area_id;                  // 地区ID
    std::string area_name;        // 地区名称（来自JSON data.jobCity）
    int salary_level_id;          // 薪资档次ID
    std::string requirements;     // 岗位要求
    double salary_min;            // 最低薪资 (单位: K)
    double salary_max;            // 最高薪资 (单位: K)
    std::string create_time;      // 创建时间 (format: "yyyy-MM-dd hh:mm:ss")
    std::string update_time;      // 更新时间
    std::string hr_last_login;    // HR最后登录时间
    int company_id;               // 公司ID
    std::string company_name;     // 公司名称（来自JSON data.identity.companyName）
    std::vector<std::string> tag_names; // 标签内容列表（来自JSON data.pcTagInfo.jobInfoTagList）
    std::vector<int> tag_ids;     // 标签ID列表（如JSON提供）
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

// 主爬虫函数
std::pair<std::vector<JobInfo>, MappingData> job_crawler_main(int pageNo, int pageSize, int recruitType);

#endif // JOB_CRAWLER_H
