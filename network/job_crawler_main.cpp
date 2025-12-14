#include "job_crawler.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>

/**
 * @brief 爬虫主函数（可配置版本）
 * @param pageNo 页码
 * @param pageSize 每页数量
 * @param recruitType 招聘类型 (1=校招, 2=实习, 3=社招)
 * @return JobInfo列表和映射数据的pair
 */
std::pair<std::vector<JobInfo>, MappingData> job_crawler_main(int pageNo, int pageSize, int recruitType) {
    try {
        // 初始化CURL（如果还没初始化）
        static bool curl_initialized = false;
        if (!curl_initialized) {
            curl_global_init(CURL_GLOBAL_DEFAULT);
            curl_initialized = true;
        }

        // 配置请求参数
        std::string url = "https://www.nowcoder.com/np-api/u/job/square-search?_=" + 
                          std::to_string(std::chrono::system_clock::now().time_since_epoch().count() / 1000000);

        std::map<std::string, std::string> headers = {
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

        // 构造POST数据，使用传入的参数
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
        std::string post_data = post_data_stream.str();

        // 1. 爬取数据
        auto json_data_opt = fetch_job_data(url, headers, post_data);

        if (!json_data_opt) {
            std::cout << "[警告] 未获取到有效数据" << std::endl;
            return {{}, {}}; // 返回空数据
        }

        // 2. 解析数据
        auto [job_info_list, mapping_data] = parse_job_data(*json_data_opt);

        if (job_info_list.empty()) {
            std::cout << "[警告] 未解析到有效职位数据" << std::endl;
            return {{}, mapping_data}; // 返回空JobInfo但保留mapping
        }

        // 返回解析结果
        std::cout << "[成功] 爬取到 " << job_info_list.size() << " 条职位数据" << std::endl;
        return {job_info_list, mapping_data};

    } catch (const std::exception& e) {
        print_debug_info("job_crawler_main",
                         "爬虫异常: " + std::string(e.what()),
                         "", DebugLevel::DL_ERROR);
        return {{}, {}}; // 返回空数据
    }
}
