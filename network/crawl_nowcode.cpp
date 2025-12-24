#include "crawl_nowcode.h"
#include "job_crawler.h"
#include <chrono>
#include <sstream>
#include <iostream>
#include <regex>
#include <QDebug>

namespace NowcodeCrawler {

std::map<std::string, std::string> getNowcodeHeaders(int recruitType) {
    return {
        {"accept", "application/json, text/plain, */*"},
        {"accept-language", "zh-CN,zh;q=0.9,en;q=0.8,en-GB;q=0.7,en-US;q=0.6"},
        {"content-type", "application/x-www-form-urlencoded"},
        {"priority", "u=1, i"},
        {"sec-ch-ua", "\"Microsoft Edge\";v=\"143\", \"Chromium\";v=\"143\", \"Not A(Brand\";v=\"24\""},
        {"sec-ch-ua-mobile", "?0"},
        {"sec-ch-ua-platform", "\"Windows\""},
        {"sec-fetch-dest", "empty"},
        {"sec-fetch-mode", "cors"},
        {"sec-fetch-site", "same-origin"},
        {"x-requested-with", "XMLHttpRequest"},
        {"referer", "https://www.nowcoder.com/jobs/intern/center?recruitType=" + std::to_string(recruitType)}
    };
}

std::string buildNowcodeUrl() {
    return "https://www.nowcoder.com/np-api/u/job/square-search?_=" + 
           std::to_string(std::chrono::system_clock::now().time_since_epoch().count() / 1000000);
}

std::string buildNowcodePostData(int pageNo, int pageSize, int recruitType) {
    std::ostringstream post_data_stream;
    post_data_stream << "careerJobId="
                    << "&jobCity="
                    << "&page=" << pageNo
                    << "&query="
                    << "&random=true"
                    << "&recommend=false"
                    << "&recruitType=" << recruitType
                    << "&salaryType=2"
                    << "&pageSize=" << pageSize
                    << "&requestFrom=1"
                    << "&order=0"
                    << "&pageSource=5001"
                    << "&visitorId=8e674683-92df-4ad6-8635-b55ffda66701";
    return post_data_stream.str();
}



std::pair<std::vector<JobInfo>, MappingData> crawlNowcode(int pageNo, int pageSize, int recruitType) {
    try {
        // 初始化CURL（如果还没初始化）
        static bool curl_initialized = false;
        if (!curl_initialized) {
            curl_global_init(CURL_GLOBAL_DEFAULT);
            curl_initialized = true;
        }

        print_debug_info("NowcodeCrawler", "开始爬取牛客网数据", 
                        "页码: " + std::to_string(pageNo) + ", 类型: " + std::to_string(recruitType));

        // 构建请求参数
        std::string url = buildNowcodeUrl();
        std::map<std::string, std::string> headers = getNowcodeHeaders(recruitType);
        std::string post_data = buildNowcodePostData(pageNo, pageSize, recruitType);

        // 1. 爬取数据
        auto json_data_opt = fetch_job_data(url, headers, post_data);

        if (!json_data_opt) {
            qDebug() << "[警告] 牛客网: 未获取到有效数据\n";
            return {{}, {}};
        }

        // 2. 解析数据（将请求的 recruitType 传入解析器，以便回退使用）
        auto [job_info_list, mapping_data] = parseNowcodeResponse(*json_data_opt, recruitType);

        if (job_info_list.empty()) {
            qDebug() << "[警告] 牛客网: 未解析到有效职位数据\n";
            return {{}, mapping_data};
        }

        qDebug() << "[成功] 牛客网: 爬取到 " << job_info_list.size() << " 条职位数据\n";
        return {job_info_list, mapping_data};

    } catch (const std::exception& e) {
        print_debug_info("NowcodeCrawler", "爬虫异常: " + std::string(e.what()), "", DebugLevel::DL_ERROR);
        return {{}, {}};
    }
}

std::pair<std::vector<JobInfo>, MappingData> parseNowcodeResponse(const json& json_data, int requestedRecruitType) {
    std::vector<JobInfo> job_list;
    MappingData mapping;

    try {
        if (!json_data.is_object()) {
            mapping.last_api_code = -1;
            mapping.last_api_message = "invalid json";
            return {job_list, mapping};
        }

        int code = json_data.value("code", -1);
        std::string msg = json_data.value("msg", "");
        mapping.last_api_code = code;
        mapping.last_api_message = msg;

        if (code != 0) {
            return {job_list, mapping};
        }

        if (!json_data.contains("data") || !json_data["data"].is_object()) {
            return {job_list, mapping};
        }

        const auto& data = json_data["data"];
        mapping.currentPage = get_int_safe(data, "currentPage", 0);
        mapping.totalPage = get_int_safe(data, "totalPage", 0);
        mapping.has_more = (mapping.currentPage < mapping.totalPage);

        if (!data.contains("datas") || !data["datas"].is_array()) {
            return {job_list, mapping};
        }

        for (const auto& item : data["datas"]) {
            if (!item.is_object() || !item.contains("data")) continue;
            const auto& d = item["data"];

            JobInfo job;
            job.info_id = get_int64_safe(d, "id", 0);
            job.info_name = get_string_safe(d, "jobName", "");
            if (job.info_name.empty()) job.info_name = get_string_safe(d, "jobTitle", "");

            job.company_name = get_string_safe(d, "companyName", "");
            job.company_id = static_cast<int>(get_int64_safe(d, "companyId", 0));

            job.area_name = get_string_safe(d, "jobCity", "");
            job.area_id = 0;

            job.salary_min = get_double_safe(d, "salaryMin", 0.0);
            job.salary_max = get_double_safe(d, "salaryMax", 0.0);
            if (job.salary_min == 0 && job.salary_max == 0) {
                std::string salaryStr = get_string_safe(d, "salary", "");
                // Extract numeric tokens from salaryStr
                try {
                    std::regex re(R"((\d+(?:\.\d+)?))");
                    std::smatch m;
                    std::string s = salaryStr;
                    std::vector<double> nums;
                    while (std::regex_search(s, m, re)) {
                        try {
                            nums.push_back(std::stod(m.str(1)));
                        } catch (...) {
                        }
                        s = m.suffix().str();
                    }

                    if (nums.size() >= 2) {
                        job.salary_min = nums[0];
                        job.salary_max = nums[1];
                    } else if (nums.size() == 1) {
                        job.salary_min = nums[0];
                        // 如果只有一个数字：若为0或无法解析则设 max=99999，否则 max=min
                        if (job.salary_min == 0.0) {
                            job.salary_max = 99999.0;
                        } else {
                            job.salary_max = job.salary_min;
                        }
                    } else {
                        // 无法解析任何数字，视为非数字/面议 -> 最高设为 99999
                        job.salary_min = 0.0;
                        job.salary_max = 99999.0;
                    }
                } catch (...) {
                    job.salary_min = 0.0;
                    job.salary_max = 99999.0;
                }
            }

            // requirements may be inside ext (JSON string) or description
            std::string req;
            if (d.contains("ext") && d["ext"].is_string()) {
                std::string ext = d["ext"].get<std::string>();
                try {
                    auto extj = json::parse(ext);
                    if (extj.contains("requirements") && extj["requirements"].is_string())
                        req = extj["requirements"].get<std::string>();
                } catch (...) {
                    req = ext;
                }
            } else if (d.contains("description") && d["description"].is_string()) {
                req = d["description"].get<std::string>();
            }
            job.requirements = sanitize_html_to_text(req);

            job.create_time = timestamp_to_datetime(get_int64_safe(d, "createTime", 0));
            job.update_time = timestamp_to_datetime(get_int64_safe(d, "updateTime", 0));

            // 如果返回数据中没有 recruitType，则使用请求时的 recruitType 作为回退值
            int rt = 0;
            if (d.contains("recruitType")) rt = get_int_safe(d, "recruitType", 0);
            if (rt == 0) rt = requestedRecruitType;
            job.type_id = rt;

            if (d.contains("skills") && d["skills"].is_array()) {
                for (const auto& s : d["skills"]) {
                    if (s.is_string()) job.tag_names.push_back(s.get<std::string>());
                }
            }

            job_list.push_back(job);
        }

        mapping.last_api_code = 0;
        mapping.last_api_message = "OK";

    } catch (const std::exception& e) {
        print_debug_info("NowcodeParser", "解析异常: " + std::string(e.what()), "", DebugLevel::DL_ERROR);
    }

    return {job_list, mapping};
}

} // namespace NowcodeCrawler
