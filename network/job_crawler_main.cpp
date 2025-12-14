#include "job_crawler.h"
#include <iostream>
#include <chrono>
#include <iomanip>

// 主函数
int main() {
    try {
        // 初始化CURL
        curl_global_init(CURL_GLOBAL_DEFAULT);

        // 记录开始时间
        auto start_time = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::system_clock::now();
        time_t now_time = std::chrono::system_clock::to_time_t(now);

        std::cout << "爬虫开始时间: " << std::put_time(std::localtime(&now_time), "%Y-%m-%d %H:%M:%S") << std::endl;
        std::cout << std::string(60, '=') << std::endl;

        // 配置请求参数
        std::string url = "https://www.nowcoder.com/np-api/u/job/square-search?_=1765365686544";

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
            {"referer", "https://www.nowcoder.com/jobs/intern/center?recruitType=2&page=3&city=%E5%B9%BF%E5%B7%9E&city=%E6%B7%B1%E5%9C%B3&city=%E6%9D%AD%E5%B7%9E&city=%E5%8D%97%E4%BA%AC&city=%E6%88%90%E9%83%BD&city=%E5%8E%A6%E9%97%A8&city=%E6%AD%A6%E6%B1%89&city=%E8%A5%BF%E5%AE%89&careerJob=11006"}
        };

        std::string post_data = "requestFrom=1&page=3&pageSize=20&recruitType=2&pageSource=5001&jobCity=%E5%B9%BF%E5%B7%9E&jobCity=%E6%B7%B1%E5%9C%B3&jobCity=%E6%9D%AD%E5%B7%9E&jobCity=%E5%8D%97%E4%BA%AC&jobCity=%E6%88%90%E9%83%BD&jobCity=%E5%8E%A6%E9%97%A8&jobCity=%E6%AD%A6%E6%B1%89&city=%E8%A5%BF%E5%AE%89&careerJobId=11006&visitorId=8e674683-92df-4ad6-8635-b55ffda66701";

        // 1. 爬取数据
        std::cout << "阶段1: 爬取数据..." << std::endl;
        auto json_data_opt = fetch_job_data(url, headers, post_data);

        if (!json_data_opt) {
            std::cout << "未获取到有效数据，程序退出" << std::endl;
            curl_global_cleanup();
            return 1;
        }

        // 2. 解析数据
        std::cout << "阶段2: 解析数据..." << std::endl;
        auto [job_info_list, mapping_data] = parse_job_data(*json_data_opt);

        if (job_info_list.empty()) {
            std::cout << "未解析到有效职位数据" << std::endl;
            curl_global_cleanup();
            return 1;
        }

        // 3. 按格式打印数据
        std::cout << "阶段3: 格式化打印数据..." << std::endl;
        print_data_formatted(
            job_info_list,
            mapping_data.type_list,
            mapping_data.area_list,
            mapping_data.salary_level_list
            );

        // 计算运行时间
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        std::cout << std::string(60, '=') << std::endl;
        std::cout << "程序运行完成，总耗时: " << duration.count() / 1000.0 << " 秒" << std::endl;
        std::cout << "注意：数据库操作已移除，数据仅显示在控制台" << std::endl;

        curl_global_cleanup();
        return 0;

    } catch (const std::exception& e) {
        print_debug_info("主程序",
                         "主程序发生异常: " + std::string(e.what()),
                         "", DebugLevel::DL_ERROR);
        curl_global_cleanup();
        return 1;
    }
}
