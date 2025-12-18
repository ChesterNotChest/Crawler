#ifndef NETWORK_TYPES_H
#define NETWORK_TYPES_H

#include <string>
#include <vector>
#include <cstdint>

/**
 * @file network_types.h
 * @brief 网络爬虫模块的数据结构定义
 * 
 * 包含从网络API爬取的职位信息相关的所有数据结构
 */

// 调试信息级别
enum class DebugLevel {
    DL_DEBUG,    // 调试信息
    DL_ERROR     // 错误信息
};

// 职位信息结构体
struct JobInfo {
    int64_t info_id;              // 岗位ID
    std::string info_name;        // 岗位名称
    int type_id;                  // 招聘类型ID (1=校招, 2=实习, 3=社招)
    int area_id;                  // 地区ID
    std::string area_name;        // 地区名称（来自JSON data.jobCity）
    int salary_level_id;          // 薪资档次ID
    std::string requirements;     // 岗位要求
    double salary_min;            // 最低薪资 (校招/社招: K/月; 实习: 元/天)
    double salary_max;            // 最高薪资 (校招/社招: K/月; 实习: 元/天)
    std::string create_time;      // 创建时间 (format: "yyyy-MM-dd hh:mm:ss")
    std::string update_time;      // 更新时间
    std::string hr_last_login;    // HR最后登录时间
    int company_id;               // 公司ID
    std::string company_name;     // 公司名称（来自JSON data.identity.companyName）
    std::vector<std::string> tag_names; // 标签内容列表（来自JSON data.pcTagInfo.jobInfoTagList）
    std::vector<int> tag_ids;     // 标签ID列表（如JSON提供）
};

// 招聘类型信息
struct TypeInfo {
    int type_id;
    std::string type_name;
};

// 地区信息
struct AreaInfo {
    int area_id;
    std::string area_name;
};

// 薪资档次信息
struct SalaryLevelInfo {
    int salary_level_id;
    int upper_limit;
};

// 映射数据（包含分页和辅助信息）
struct MappingData {
    int currentPage = 0;                            // 当前页码
    int totalPage = 0;                              // 总页数
    std::vector<TypeInfo> type_list;                // 招聘类型列表
    std::vector<AreaInfo> area_list;                // 地区列表
    std::vector<SalaryLevelInfo> salary_level_list; // 薪资档次列表
};

#endif // NETWORK_TYPES_H
