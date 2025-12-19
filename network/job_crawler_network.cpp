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

// 从页面获取Cookie
std::string fetch_cookies_from_page(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) {
        print_debug_info("Cookie获取", "CURL初始化失败", "", DebugLevel::DL_ERROR);
        return "";
    }

    std::string response_data;
    std::string response_headers;
    std::string cookies;

    try {
        print_debug_info("Cookie获取", "访问页面: " + url);

        // 设置CURL选项
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        
        // 启用Cookie引擎
        curl_easy_setopt(curl, CURLOPT_COOKIEFILE, "");  // 启用内存中的cookie引擎

        // SSL配置
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        // User-Agent
        curl_easy_setopt(curl, CURLOPT_USERAGENT, 
            "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");

        // 执行请求
        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            print_debug_info("Cookie获取", 
                           std::string("请求失败: ") + curl_easy_strerror(res),
                           "", DebugLevel::DL_ERROR);
            curl_easy_cleanup(curl);
            return "";
        }

        // 获取HTTP状态码
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);

        if (http_code == 200) {
            // 从响应头中提取Set-Cookie
            std::istringstream header_stream(response_headers);
            std::string line;
            std::vector<std::string> cookie_parts;

            while (std::getline(header_stream, line)) {
                if (line.find("Set-Cookie:") == 0 || line.find("set-cookie:") == 0) {
                    std::string cookie_line = line.substr(11); // 跳过"Set-Cookie:"
                    size_t semicolon = cookie_line.find(';');
                    if (semicolon != std::string::npos) {
                        std::string cookie_value = cookie_line.substr(0, semicolon);
                        // 去除前后空格
                        cookie_value.erase(0, cookie_value.find_first_not_of(" \t\r\n"));
                        cookie_value.erase(cookie_value.find_last_not_of(" \t\r\n") + 1);
                        if (!cookie_value.empty()) {
                            cookie_parts.push_back(cookie_value);
                        }
                    }
                }
            }

            // 组合所有cookie
            for (size_t i = 0; i < cookie_parts.size(); ++i) {
                if (i > 0) cookies += "; ";
                cookies += cookie_parts[i];
            }

            if (!cookies.empty()) {
                print_debug_info("Cookie获取", "成功获取Cookie", 
                               "数量: " + std::to_string(cookie_parts.size()));
            } else {
                print_debug_info("Cookie获取", "未从响应头获取到Cookie", "", DebugLevel::DL_WARN);
            }
        } else {
            print_debug_info("Cookie获取", 
                           "HTTP状态码异常: " + std::to_string(http_code),
                           "", DebugLevel::DL_ERROR);
        }

        curl_easy_cleanup(curl);
    } catch (const std::exception& e) {
        print_debug_info("Cookie获取", 
                       std::string("异常: ") + e.what(),
                       "", DebugLevel::DL_ERROR);
        curl_easy_cleanup(curl);
        return "";
    }

    return cookies;
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

        // 设置Cookie（硬编码示例，后续可改为配置）
        curl_easy_setopt(curl, CURLOPT_COOKIE, "lastCity=101010100; ab_guid=95c3ba09-93ab-41cc-b431-8439fe09ec38; __zp_seo_uuid__=e5b151dd-09f6-47d4-89a3-8cb05b7cc134; Hm_lvt_194df3105ad7148dcf2b98a91b5e727a=1765361919,1766103112; HMACCOUNT=DD39DE392223B288; __g=sem_bingpc; __l=r=https%3A%2F%2Fcn.bing.com%2F&l=%2Fwww.zhipin.com%2Fweb%2Fgeek%2Fjobs%3Fquery%3D&s=3&g=%2Fwww.zhipin.com%2Fsem%2F10.html%3F_ts%3D1766103109907%26sid%3Dsem_bingpc%26qudao%3Dbing_pc_H120003UY5%26plan%3DBOSS-%25E5%25BF%2585%25E5%25BA%2594-%25E5%2593%2581%25E7%2589%258C%26unit%3D%25E7%25B2%25BE%25E5%2587%2586%26keyword%3Dboss%25E7%259B%25B4%25E8%2581%2598%26msclkid%3D4ed31a1298c31e44df4873560081ae29&friend_source=0&s=3&friend_source=0; Hm_lpvt_194df3105ad7148dcf2b98a91b5e727a=1766106094; __zp_stoken__=d78egPjjDn8K7worCuVE%2FGDk%2BLDc4Pj44UTo8PjjCgSpSRTtSUzkXwrLDkVjDt3lww6LDvcK7OylRODo%2BODc4UU8ZUVRSRThSRVLEuMK7OkVSw6XDuFMoGMKewrhxxJbClnbDiyZmJsOwwrsmGcORCcOWw5IHfSorCggJIhUJZXEfcWVYCQkkIgdzWQp0dCMVCiIHcyJXFQcyHlJywrpSw49bw5M7wrhlwrpSw5FSRTtSK1ZFwqs%2BU0U7N1FTU8S4xL7Fj8S%2BxZDEuMS%2Bw5%2FDpsSPw7TEvsWPxL7Dj8S4xL7Fj8O%2Bw6%2FEuMS%2BxY%2FEvsSSw4UtUcKmw5LDrsOExKFjw7%2FEm8KewpfCi8OFwrLCtMK2R8KawqfDu2PCtcKnwonCg8ODYMK%2BTklxSVtwwpPCrMKTXV3DhsOSSMK3wqlqwrwfTXXCucOQZlwfcMKQIFg5I2fDl8OV; __c=1766103112; __a=48526724.1765361919.1765361919.1766103112.38.2.24.24");

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
