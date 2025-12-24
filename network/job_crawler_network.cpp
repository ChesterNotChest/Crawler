#include "job_crawler.h"
#include <iostream>
#include <vector>

// 写入数据的回调函数
size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t total_size = size * nmemb;
    response->append(static_cast<char*>(contents), total_size);
    return total_size;
}

// 处理HTTP响应头的回调函数（用于提取Cookie）
size_t header_callback(char* buffer, size_t size, size_t nitems, std::string* headers) {
    size_t total_size = size * nitems;
    headers->append(buffer, total_size);
    return total_size;
}


// 获取职位数据函数
std::optional<json> fetch_job_data(const std::string& url, const std::map<std::string, std::string>& headers,
                                   const std::string& post_data) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        print_debug_info("网络请求", "CURL初始化失败", "", DebugLevel::DL_ERROR);
        return std::nullopt;
    }

    std::string response_data;

    try {
        print_debug_info("网络请求", "开始请求URL: " + url);

        // 设置CURL选项
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        
        // 如果有POST数据，设置为POST请求；否则为GET请求
        if (!post_data.empty()) {
            curl_easy_setopt(curl, CURLOPT_POST, 1L);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        }
        
        // 自动处理 gzip/deflate/br
        curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, "");

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);  // 增加超时时间
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

        // 添加SSL配置
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_TRY);

        // 设置用户代理
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");

        // 设置HTTP头
        struct curl_slist* header_list = nullptr;
        for (const auto& header : headers) {
            std::string header_str = header.first + ": " + header.second;
            header_list = curl_slist_append(header_list, header_str.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header_list);

        // 执行请求
        CURLcode res = curl_easy_perform(curl);

        // 清理头部列表
        curl_slist_free_all(header_list);

        if (res != CURLE_OK) {
            print_debug_info("网络请求",
                             std::string("CURL请求失败: ") + curl_easy_strerror(res),
                             "", DebugLevel::DL_ERROR);
            curl_easy_cleanup(curl);
            return std::nullopt;
        }

        // 获取HTTP状态码
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        print_debug_info("网络请求", "状态码: " + std::to_string(http_code));

        curl_easy_cleanup(curl);

        if (http_code == 200) {
            try {
                json json_data = json::parse(response_data);
                print_debug_info("JSON解析",
                                 "成功解析JSON，数据类型: " + std::string(json_data.type_name()));

                // 打印JSON结构
                std::vector<std::string> keys;
                for (auto it = json_data.begin(); it != json_data.end(); ++it) {
                    keys.push_back(it.key());
                }

                std::string keys_str;
                for (size_t i = 0; i < keys.size(); ++i) {
                    if (i > 0) keys_str += ", ";
                    keys_str += keys[i];
                }
                print_debug_info("JSON结构", "JSON键: [" + keys_str + "]");

                return json_data;
            } catch (const json::parse_error& e) {
                std::string data_preview = response_data.substr(0, 500);
                print_debug_info("JSON解析",
                                 std::string("JSON解析失败: ") + e.what(),
                                 data_preview, DebugLevel::DL_ERROR);
                return std::nullopt;
            }
        } else {
            std::string data_preview = response_data.substr(0, 500);
            print_debug_info("网络请求",
                             "请求失败，状态码: " + std::to_string(http_code),
                             data_preview, DebugLevel::DL_ERROR);
            return std::nullopt;
        }

    } catch (const std::exception& e) {
        print_debug_info("网络请求",
                         std::string("网络请求异常: ") + e.what(),
                         "", DebugLevel::DL_ERROR);
        curl_easy_cleanup(curl);
        return std::nullopt;
    }
}
