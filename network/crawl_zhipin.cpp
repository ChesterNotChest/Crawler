#include "crawl_zhipin.h"
#include "job_crawler.h"
#include "config/config_manager.h"
#include <chrono>
#include <sstream>
#include <iostream>
#include <iomanip>
#include <QDebug>

namespace ZhipinCrawler {

std::map<std::string, std::string> getZhipinHeaders() {
    // 从配置文件读取Cookie
    QString configCookie = ConfigManager::getZhipinCookie();
    std::string cookie = configCookie.isEmpty() 
        ? "__l=l=%2Fwww.zhipin.com%2Fweb%2Fuser%2Fsafe%2Fverify-slider%3FcallbackUrl%3Dhttps%253A%252F%252Fwww.zhipin.com%252F&r=&g=&s=3&friend_source=0; __g=-; __c=1766141990; __a=18043173.1766141837.1766141837.1766141990.2.2.1.2; ab_guid=b9958ab3-1848-475f-848e-d50cd505c1ef" // 默认最基础的cookie
        : configCookie.toStdString();
    
    return {
        {"accept", "application/json, text/plain, */*"},
        {"accept-language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6"},
        {"user-agent", "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36"},
        {"referer", "https://www.zhipin.com/web/geek/jobs?query=&city=101010100"},
        {"priority", "u=1, i"},
        {"sec-ch-ua", "\"Microsoft Edge\";v=\"143\", \"Chromium\";v=\"143\", \"Not A(Brand\";v=\"24\""},
        {"sec-ch-ua-mobile", "?0"},
        {"sec-ch-ua-platform", "\"Windows\""},
        {"sec-fetch-dest", "empty"},
        {"sec-fetch-mode", "cors"},
        {"sec-fetch-site", "same-origin"},
        {"x-requested-with", "XMLHttpRequest"},
        {"traceid", "F-ab0b91U7L727coLt"},
        {"cookie", cookie}
    };
}

std::string buildZhipinUrl(int page, int pageSize, const std::string& city) {
    // 获取当前时间戳（毫秒）
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count();
    
    std::ostringstream url_stream;
    url_stream << "https://www.zhipin.com/wapi/zpgeek/pc/recommend/job/list.json"
               << "?page=" << page
               << "&pageSize=" << pageSize
               << "&city=" << city
               << "&encryptExpectId="
               << "&mixExpectType="
               << "&expectInfo="
               << "&jobType="
               << "&salary="
               << "&experience="
               << "&degree="
               << "&industry="
               << "&scale="
               << "&_=" << timestamp;
    
    return url_stream.str();
}

std::pair<std::vector<JobInfo>, MappingData> parseZhipinResponse(const json& json_data) {
    std::vector<JobInfo> job_list;
    MappingData mapping_data;
    
    try {
        // 检查响应码
        if (!json_data.contains("code") || json_data["code"].get<int>() != 0) {
            int code = json_data.contains("code") ? json_data["code"].get<int>() : -1;
            std::string message = json_data.contains("message") ? json_data["message"].get<std::string>() : "未知错误";
            std::ostringstream error_info;
            error_info << "code=" << code << ", message=\"" << message << "\"";
            print_debug_info("ZhipinParser", "API返回错误码", error_info.str(), DebugLevel::DL_ERROR);
            // 将API返回码写入mapping_data，供上层做决策（例如检测反爬码37）
            mapping_data.last_api_code = code;
            mapping_data.last_api_message = message;
            return {job_list, mapping_data};
        }
        
        // 检查是否包含zpData
        if (!json_data.contains("zpData") || !json_data["zpData"].contains("jobList")) {
            print_debug_info("ZhipinParser", "响应中缺少jobList数据", "", DebugLevel::DL_ERROR);
            return {job_list, mapping_data};
        }

        // 读取分页/应答提示字段：hasMore 表示是否有更多数据（若缺失，默认继续）
        if (json_data["zpData"].contains("hasMore")) {
            try {
                mapping_data.has_more = json_data["zpData"]["hasMore"].get<bool>();
            } catch (...) {
                mapping_data.has_more = true;
            }
        } else {
            mapping_data.has_more = true; // 不要因为缺少字段而停止
        }

        const auto& job_array = json_data["zpData"]["jobList"];
        print_debug_info("ZhipinParser", "开始解析BOSS直聘数据", "职位数量: " + std::to_string(job_array.size()));
        
        for (const auto& job_item : job_array) {
            try {
                JobInfo job;
                
                // 基本信息
                if (job_item.contains("encryptJobId")) {
                    // 使用encryptJobId的哈希值作为jobId
                    std::string encrypt_id = job_item["encryptJobId"].get<std::string>();
                    job.info_id = std::hash<std::string>{}(encrypt_id);
                }
                
                job.info_name = job_item.value("jobName", "");
                
                // 公司信息
                job.company_name = job_item.value("brandName", "");
                if (job_item.contains("encryptBrandId")) {
                    std::string brand_id = job_item["encryptBrandId"].get<std::string>();
                    job.company_id = std::hash<std::string>{}(brand_id);
                }
                
                // 地区信息
                job.area_name = job_item.value("cityName", "");
                if (job_item.contains("areaDistrict")) {
                    job.area_name += " " + job_item["areaDistrict"].get<std::string>();
                }
                if (job_item.contains("businessDistrict")) {
                    job.area_name += " " + job_item["businessDistrict"].get<std::string>();
                }
                job.area_id = job_item.value("city", 0);
                
                // 薪资信息处理：支持带K和不带K两种格式
                std::string salary_desc = job_item.value("salaryDesc", "");
                if (!salary_desc.empty()) {
                    // 判断是否含有 'K' 单位（如 30-60K·19薪），否则为类似 300-600元/天
                    bool hasK = (salary_desc.find('K') != std::string::npos) || (salary_desc.find('k') != std::string::npos);

                    // 简单安全的数字提取器
                    auto parse_double = [](const std::string& s)->double {
                        std::string t;
                        for (char c : s) {
                            if ((c >= '0' && c <= '9') || c == '.' || c == '-') t.push_back(c);
                            else if (!t.empty()) break; // stop at first non-number after started
                        }
                        try { return t.empty() ? 0.0 : std::stod(t); } catch (...) { return 0.0; }
                    };

                    size_t dash_pos = salary_desc.find('-');
                    if (dash_pos != std::string::npos) {
                        std::string min_str = salary_desc.substr(0, dash_pos);
                        std::string max_str = salary_desc.substr(dash_pos + 1);
                        // If max_str contains 'K', trim after it to avoid trailing tokens
                        size_t k_pos = max_str.find('K');
                        if (k_pos == std::string::npos) k_pos = max_str.find('k');
                        if (k_pos != std::string::npos) max_str = max_str.substr(0, k_pos);

                        job.salary_min = parse_double(min_str);
                        job.salary_max = parse_double(max_str);
                    } else {
                        double v = parse_double(salary_desc);
                        job.salary_min = v;
                        job.salary_max = v;
                    }

                    // 若不含 K，则将招聘类型自动设为实习（2）
                    if (!hasK) {
                        job.type_id = 2;
                    }
                }
                
                // 岗位要求（合并jobLabels和skills）
                std::ostringstream requirements;
                if (job_item.contains("jobLabels") && job_item["jobLabels"].is_array()) {
                    requirements << "要求: ";
                    for (const auto& label : job_item["jobLabels"]) {
                        if (label.is_string()) {
                            requirements << label.get<std::string>() << " ";
                        }
                    }
                }
                
                if (job_item.contains("skills") && job_item["skills"].is_array()) {
                    requirements << " | 技能: ";
                    for (const auto& skill : job_item["skills"]) {
                        if (skill.is_string()) {
                            requirements << skill.get<std::string>() << " ";
                        }
                    }
                }
                
                job.requirements = requirements.str();
                
                // 工作经验和学历要求
                std::string experience = job_item.value("jobExperience", "");
                std::string degree = job_item.value("jobDegree", "");
                if (!experience.empty() || !degree.empty()) {
                    job.requirements += " | " + experience + " " + degree;
                }
                
                // 标签处理
                if (job_item.contains("jobLabels") && job_item["jobLabels"].is_array()) {
                    for (const auto& label : job_item["jobLabels"]) {
                        if (label.is_string()) {
                            job.tag_names.push_back(label.get<std::string>());
                        }
                    }
                }
                
                // 时间信息（BOSS直聘可能没有直接提供，使用当前时间）
                auto now = std::chrono::system_clock::now();
                auto time_t_now = std::chrono::system_clock::to_time_t(now);
                std::tm tm_now;
                localtime_s(&tm_now, &time_t_now);
                
                std::ostringstream time_stream;
                time_stream << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
                job.create_time = time_stream.str();
                job.update_time = time_stream.str();
                job.hr_last_login = "";
                
                // 招聘类型（BOSS直聘没有明确分类，默认设为社招=3）
                if (job.type_id == 0) job.type_id = 3;
                
                // 薪资档次（需要根据薪资范围计算）
                if (job.salary_max > 0) {
                    if (job.salary_max <= 5) job.salary_level_id = 0;
                    else if (job.salary_max <= 10) job.salary_level_id = 1;
                    else if (job.salary_max <= 15) job.salary_level_id = 2;
                    else if (job.salary_max <= 20) job.salary_level_id = 3;
                    else if (job.salary_max <= 30) job.salary_level_id = 4;
                    else if (job.salary_max <= 50) job.salary_level_id = 5;
                    else job.salary_level_id = 6;
                } else {
                    job.salary_level_id = 0;
                }
                
                job_list.push_back(job);
                
            } catch (const std::exception& e) {
                print_debug_info("ZhipinParser", "解析单个职位失败: " + std::string(e.what()), "", DebugLevel::DL_ERROR);
                continue;
            }
        }
        
        mapping_data.last_api_code = 0;
        mapping_data.last_api_message = "OK";
        print_debug_info("ZhipinParser", "成功解析职位数据", "数量: " + std::to_string(job_list.size()));
        
    } catch (const std::exception& e) {
        print_debug_info("ZhipinParser", "解析异常: " + std::string(e.what()), "", DebugLevel::DL_ERROR);
    }
    
    return {job_list, mapping_data};
}

std::pair<std::vector<JobInfo>, MappingData> crawlZhipin(int page, int pageSize, const std::string& city) {
    try {
        // 初始化CURL（如果还没初始化）
        static bool curl_initialized = false;
        if (!curl_initialized) {
            curl_global_init(CURL_GLOBAL_DEFAULT);
            curl_initialized = true;
        }
        
        print_debug_info("ZhipinCrawler", "开始爬取BOSS直聘数据", 
                        "页码: " + std::to_string(page) + ", 城市: " + city);
        
        // 构建请求参数
        std::string url = buildZhipinUrl(page, pageSize, city);
        std::map<std::string, std::string> headers = getZhipinHeaders();
        
        // BOSS直聘使用GET请求，不需要POST数据
        std::string post_data = "";
        
        // 1. 爬取数据（使用fetch_job_data，但传入空POST数据表示GET请求）
        auto json_data_opt = fetch_job_data(url, headers, post_data);
        
        if (!json_data_opt) {
            qDebug() << "[警告] BOSS直聘: 未获取到有效数据\n";
            return {{}, {}};
        }
        
        // 2. 解析数据
        auto [job_info_list, mapping_data] = parseZhipinResponse(*json_data_opt);
        
        if (job_info_list.empty()) {
            qDebug() << "[警告] BOSS直聘: 未解析到有效职位数据\n";
            return {{}, mapping_data};
        }
        
        qDebug() << "[成功] BOSS直聘: 爬取到 " << job_info_list.size() << " 条职位数据\n";
        return {job_info_list, mapping_data};
        
    } catch (const std::exception& e) {
        print_debug_info("ZhipinCrawler", "爬虫异常: " + std::string(e.what()), "", DebugLevel::DL_ERROR);
        return {{}, {}};
    }
}



} // namespace ZhipinCrawler
