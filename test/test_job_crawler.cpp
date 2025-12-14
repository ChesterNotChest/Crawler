#include "../network/job_crawler.h"
#include <QDebug>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <map>
#include <optional>

#ifdef _WIN32
    #include <windows.h>
#endif

// Job crawler test - fetch, parse and print data
void test_job_crawler()
{
    // Windows下设置控制台编码为UTF-8
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif

    setlocale(LC_ALL, "zh_CN.UTF-8");

    try {
        // 初始化CURL（必须在任何Qt网络操作之前）
        CURLcode curl_result = curl_global_init(CURL_GLOBAL_DEFAULT);
        if (curl_result != CURLE_OK) {
            qDebug() << "CURL初始化失败:" << curl_easy_strerror(curl_result);
            return;
        }

        // 检查第三方库是否可用
        qDebug() << "检查第三方库...";

        // 测试nlohmann/json
        try {
            json test_json = {{"test", "nlohmann_json_works"}};
            qDebug() << "nlohmann/json库正常";
        } catch (const std::exception& e) {
            qDebug() << "nlohmann/json库异常:" << e.what();
            curl_global_cleanup();
            return;
        }

        // 测试CURL
        CURL* test_curl = curl_easy_init();
        if (!test_curl) {
            qDebug() << "CURL库初始化失败";
            curl_global_cleanup();
            return;
        }
        curl_easy_cleanup(test_curl);
        qDebug() << "CURL库正常";

        // 记录开始时间
        auto start_time = std::chrono::high_resolution_clock::now();
        auto now = std::chrono::system_clock::now();
        time_t now_time = std::chrono::system_clock::to_time_t(now);

        char time_str[100];
        strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S", std::localtime(&now_time));
        qDebug() << "爬虫开始时间:" << time_str;

        qDebug() << QString(60, '=');

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
        qDebug() << "阶段1: 爬取数据...";
        auto json_data_opt = fetch_job_data(url, headers, post_data);

        if (!json_data_opt) {
            qDebug() << "未获取到有效数据";
            curl_global_cleanup();
            return;
        }

        // 2. 解析数据
        qDebug() << "阶段2: 解析数据...";
        auto [job_info_list, mapping_data] = parse_job_data(*json_data_opt);

        if (job_info_list.empty()) {
            qDebug() << "未解析到有效职位数据";
            curl_global_cleanup();
            return;
        }

        // 3. 按格式打印数据
        qDebug() << "阶段3: 格式化打印数据...";
        print_data_formatted(
            job_info_list,
            mapping_data.type_list,
            mapping_data.area_list,
            mapping_data.salary_level_list
        );

        // 计算运行时间
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        qDebug() << QString(60, '=');
        qDebug() << "爬虫任务完成，总耗时:" << duration.count() / 1000.0 << "秒";
        qDebug() << "注意：数据库操作已移除，数据仅显示在控制台";

        // 清理CURL
        curl_global_cleanup();

    } catch (const std::exception& e) {
        qDebug() << "启动异常:" << e.what();
        curl_global_cleanup();
    }
}
