#include "job_crawler.h"
#include <QDebug>
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
#include <algorithm>

// 第三方库包含
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

// 调试信息打印函数
void print_debug_info(const std::string& stage, const std::string& message,
                      const std::string& data, DebugLevel level) {
    QString prefix = (level == DebugLevel::DL_ERROR) ? "[ERROR] " : "[DEBUG] ";
    qDebug() << prefix << "[" << stage.c_str() << "]" << message.c_str();

    if (!data.empty()) {
        qDebug() << "   数据:" << data.c_str();
    }
    qDebug() << "";
}

// 时间戳转换函数
std::string timestamp_to_datetime(int64_t timestamp) {
    try {
        time_t time_value;
        if (timestamp > 1000000000000) { // 毫秒时间戳
            time_value = timestamp / 1000;
        } else { // 秒时间戳
            time_value = timestamp;
        }

        tm* time_info = localtime(&time_value);
        if (!time_info) {
            return "";
        }

        std::stringstream ss;
        ss << std::put_time(time_info, "%Y-%m-%d %H:%M:%S");
        return ss.str();
    } catch (const std::exception& e) {
        print_debug_info("时间转换", std::string("时间转换失败: ") + e.what(),
                         std::to_string(timestamp), DebugLevel::DL_ERROR);
        return "";
    }
}

// CURL写回调函数
size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* response) {
    size_t total_size = size * nmemb;
    response->append((char*)contents, total_size);
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
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
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

        // 设置Cookie
        curl_easy_setopt(curl, CURLOPT_COOKIE, "credentials=include");

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

// 数据解析函数
std::pair<std::vector<JobInfo>, MappingData> parse_job_data(const json& json_data) {
    std::vector<JobInfo> job_info_list;
    MappingData mapping_data;

    print_debug_info("数据解析", "开始解析JSON数据");

    // 薪资档次定义
    std::vector<std::pair<int, int>> salary_thresholds = {
        {0, 200}, {201, 500}, {501, 1000},
        {1001, 1500}, {1501, 3000}, {3001, 99999}
    };

    // 使用map来存储映射关系
    std::map<int, std::string> type_dict;
    std::map<std::string, int> area_dict;
    std::map<int, int> salary_level_dict;

    // 初始化自增ID计数器
    int area_id_counter = 1;
    int salary_level_id_counter = 1;

    try {
        // 检查数据结构
        if (!json_data.is_object()) {
            print_debug_info("数据解析", "JSON数据不是对象类型",
                             json_data.type_name(), DebugLevel::DL_ERROR);
            return std::make_pair(job_info_list, mapping_data);
        }

        // 检查返回码
        int code = json_data.value("code", -1);
        std::string message = json_data.value("msg", "");
        print_debug_info("数据解析", "返回码: " + std::to_string(code) + ", 消息: " + message);

        if (code != 0) {
            print_debug_info("数据解析", "API返回错误: " + message, "", DebugLevel::DL_ERROR);
            return std::make_pair(job_info_list, mapping_data);
        }

        // 检查data字段
        if (!json_data.contains("data")) {
            std::vector<std::string> keys;
            for (auto it = json_data.begin(); it != json_data.end(); ++it) {
                keys.push_back(it.key());
            }
            std::string keys_str;
            for (size_t i = 0; i < keys.size(); ++i) {
                if (i > 0) keys_str += ", ";
                keys_str += keys[i];
            }
            print_debug_info("数据解析", "JSON中没有data字段，可用字段: [" + keys_str + "]",
                             "", DebugLevel::DL_ERROR);
            return std::make_pair(job_info_list, mapping_data);
        }

        auto data_field = json_data["data"];
        print_debug_info("数据解析", "data字段类型: " + std::string(data_field.type_name()));

        // 检查data字段类型
        if (!data_field.is_object()) {
            print_debug_info("数据解析", "data字段不是对象类型: " + std::string(data_field.type_name()),
                             "", DebugLevel::DL_ERROR);
            return std::make_pair(job_info_list, mapping_data);
        }

        // 打印data字典键
        std::vector<std::string> data_keys;
        for (auto it = data_field.begin(); it != data_field.end(); ++it) {
            data_keys.push_back(it.key());
        }
        std::string data_keys_str;
        for (size_t i = 0; i < data_keys.size(); ++i) {
            if (i > 0) data_keys_str += ", ";
            data_keys_str += data_keys[i];
        }
        print_debug_info("数据解析", "data字典键: [" + data_keys_str + "]");

        // 检查datas字段
        if (!data_field.contains("datas")) {
            print_debug_info("数据解析", "data字典中没有datas字段", "", DebugLevel::DL_ERROR);
            return std::make_pair(job_info_list, mapping_data);
        }

        auto datas_list = data_field["datas"];
        print_debug_info("数据解析", "datas字段类型: " + std::string(datas_list.type_name()));

        if (!datas_list.is_array()) {
            print_debug_info("数据解析", "datas字段不是数组类型: " + std::string(datas_list.type_name()),
                             "", DebugLevel::DL_ERROR);
            return std::make_pair(job_info_list, mapping_data);
        }

        print_debug_info("数据解析", "datas列表长度: " + std::to_string(datas_list.size()));

        if (datas_list.empty()) {
            print_debug_info("数据解析", "datas列表为空", "", DebugLevel::DL_ERROR);
            return std::make_pair(job_info_list, mapping_data);
        }

        // 提取职位数据
        std::vector<json> job_list;
        for (size_t i = 0; i < datas_list.size(); ++i) {
            auto item = datas_list[i];
            if (item.is_object() && item.contains("data")) {
                auto job_data = item["data"];
                if (job_data.is_object()) {
                    job_list.push_back(job_data);
                } else {
                    print_debug_info("数据解析",
                                     "item.data不是对象: " + std::string(job_data.type_name()),
                                     "", DebugLevel::DL_ERROR);
                }
            } else {
                print_debug_info("数据解析",
                                 "datas元素格式不正确: " + std::string(item.type_name()),
                                 "", DebugLevel::DL_ERROR);
            }
        }

        print_debug_info("数据解析", "提取到的职位数据数量: " + std::to_string(job_list.size()));

        if (job_list.empty()) {
            print_debug_info("数据解析", "没有提取到职位数据", "", DebugLevel::DL_ERROR);
            return std::make_pair(job_info_list, mapping_data);
        }

        // 打印第一个职位的详细信息用于调试
        if (!job_list.empty()) {
            auto first_job = job_list[0];
            std::vector<std::string> first_job_keys;
            for (auto it = first_job.begin(); it != first_job.end(); ++it) {
                first_job_keys.push_back(it.key());
            }
            std::string first_job_keys_str;
            for (size_t i = 0; i < first_job_keys.size(); ++i) {
                if (i > 0) first_job_keys_str += ", ";
                first_job_keys_str += first_job_keys[i];
            }
            print_debug_info("数据解析", "第一个职位数据键: [" + first_job_keys_str + "]");

            std::vector<std::string> important_keys = {"id", "jobName", "recruitType", "jobCity", "salaryMin", "salaryMax"};
            for (const auto& key : important_keys) {
                if (first_job.contains(key)) {
                    std::string value_str;
                    try {
                        if (first_job[key].is_string()) {
                            value_str = first_job[key].get<std::string>();
                        } else if (first_job[key].is_number()) {
                            value_str = std::to_string(first_job[key].get<int>());
                        } else {
                            value_str = "非字符串/数字类型";
                        }
                    } catch (...) {
                        value_str = "获取值失败";
                    }
                    print_debug_info("数据解析", "  " + key + ": " + value_str);
                }
            }
        }

        int processed_count = 0;
        int DL_ERROR_count = 0;

        // 类型名称映射
        std::map<int, std::string> type_name_map = {
            {1, "校招"}, {2, "实习"}, {3, "社招"}
        };

        for (size_t i = 0; i < job_list.size(); ++i) {
            try {
                auto job_data = job_list[i];
                if (!job_data.is_object()) {
                    print_debug_info("数据解析",
                                     "职位数据" + std::to_string(i) + "不是对象类型: " + std::string(job_data.type_name()),
                                     "", DebugLevel::DL_ERROR);
                    DL_ERROR_count++;
                    continue;
                }

                // 1. 提取主表信息
                JobInfo job_info;

                // 信息ID
                job_info.info_id = job_data.value("id", 0);

                // 信息名称
                job_info.info_name = job_data.value("jobName", "");

                // 类型ID
                job_info.type_id = job_data.value("recruitType", 0);

                // 类型名称映射
                std::string type_name = "未知";
                if (type_name_map.find(job_info.type_id) != type_name_map.end()) {
                    type_name = type_name_map[job_info.type_id];
                }
                type_dict[job_info.type_id] = type_name;

                // 地区ID和地区名称
                std::string job_city = job_data.value("jobCity", "");
                if (job_city.empty() && job_data.contains("jobCityList")) {
                    auto job_city_list = job_data["jobCityList"];
                    if (job_city_list.is_array() && !job_city_list.empty()) {
                        auto first_city = job_city_list[0];
                        if (first_city.is_string()) {
                            job_city = first_city.get<std::string>();
                        }
                    }
                }

                if (!job_city.empty() && area_dict.find(job_city) == area_dict.end()) {
                    area_dict[job_city] = area_id_counter;
                    area_id_counter++;
                }
                job_info.area_id = (job_city.empty()) ? 0 : area_dict[job_city];
                job_info.area_name = job_city;

                // 薪资档次ID
                int salary_max = job_data.value("salaryMax", 0);
                job_info.salary_level_id = 0;
                for (size_t level_id = 0; level_id < salary_thresholds.size(); ++level_id) {
                    auto threshold = salary_thresholds[level_id];
                    if (threshold.first <= salary_max && salary_max <= threshold.second) {
                        job_info.salary_level_id = level_id + 1;
                        salary_level_dict[job_info.salary_level_id] = threshold.second;
                        break;
                    }
                }

                // 岗位要求
                job_info.requirements = "";
                if (job_data.contains("ext")) {
                    try {
                        std::string ext_data;
                        if (job_data["ext"].is_string()) {
                            ext_data = job_data["ext"].get<std::string>();
                        } else {
                            ext_data = job_data["ext"].dump();
                        }

                        if (!ext_data.empty()) {
                            try {
                                auto ext_dict = json::parse(ext_data);
                                if (ext_dict.contains("requirements") && ext_dict["requirements"].is_string()) {
                                    job_info.requirements = ext_dict["requirements"].get<std::string>();
                                } else if (ext_dict.contains("requirement") && ext_dict["requirement"].is_string()) {
                                    job_info.requirements = ext_dict["requirement"].get<std::string>();
                                }
                            } catch (const json::parse_error&) {
                                // 如果ext不是JSON，直接使用字符串
                                job_info.requirements = ext_data;
                            }
                        }

                        // 限制为1000字符
                        if (job_info.requirements.length() > 1000) {
                            job_info.requirements = job_info.requirements.substr(0, 1000);
                        }
                    } catch (const std::exception& e) {
                        print_debug_info("数据解析",
                                         "职位" + std::to_string(i) + "的ext字段处理异常: " + std::string(e.what()),
                                         "", DebugLevel::DL_ERROR);
                        job_info.requirements = "";
                    }
                }

// 薪资最小值已在前面处理，这里处理创建时间
				int64_t create_time = job_data.value("createTime", 0);
				job_info.create_time = timestamp_to_datetime(create_time);

                // 更新时间
                int64_t update_time = job_data.value("updateTime", create_time);
                job_info.update_time = timestamp_to_datetime(update_time);

                // HR最后登录时间 + 公司信息 (identity为列表，缺失则报错)
                int64_t hr_login_time = 0;
                try {
                    if (job_data.contains("user") && job_data["user"].is_object()) {
                        auto user_data = job_data["user"];
                        if (user_data.contains("loginTime")) {
                            hr_login_time = user_data.value("loginTime", 0);
                        }

                        if (user_data.contains("identity") && user_data["identity"].is_array()) {
                            auto identity_list = user_data["identity"];
                            for (const auto& identity : identity_list) {
                                if (!identity.is_object()) continue;
                                if (identity.contains("companyId")) {
                                    job_info.company_id = identity.value("companyId", 0);
                                }
                                if (identity.contains("companyName") && identity["companyName"].is_string()) {
                                    job_info.company_name = identity["companyName"].get<std::string>();
                                }
                                if (job_info.company_id > 0 && !job_info.company_name.empty()) break;
                            }
                        } else {
                            print_debug_info("数据解析", "缺少identity列表", "", DebugLevel::DL_ERROR);
                        }
                    }
                } catch (const std::exception& e) {
                    print_debug_info("数据解析",
                                     "职位" + std::to_string(i) + "的HR/公司信息提取异常: " + std::string(e.what()),
                                     "", DebugLevel::DL_ERROR);
                }
                job_info.hr_last_login = timestamp_to_datetime(hr_login_time);

                if (job_info.company_id == 0 || job_info.company_name.empty()) {
                    print_debug_info("数据解析", "公司信息缺失 (identity列表未提供companyId/companyName)", "", DebugLevel::DL_ERROR);
                }

                // 标签提取: data.pcTagInfo.jobInfoTagList.tag.title/内容
                try {
                    if (job_data.contains("pcTagInfo") && job_data["pcTagInfo"].is_object()) {
                        auto tag_info = job_data["pcTagInfo"];
                        if (tag_info.contains("jobInfoTagList") && tag_info["jobInfoTagList"].is_array()) {
                            auto tag_list = tag_info["jobInfoTagList"];
                            for (const auto& tag_item : tag_list) {
                                if (!tag_item.is_object()) continue;

                                // 优先从 tag/title
                                if (tag_item.contains("tag") && tag_item["tag"].is_object()) {
                                    auto tag_obj = tag_item["tag"];
                                    if (tag_obj.contains("title") && tag_obj["title"].is_string()) {
                                        job_info.tag_names.push_back(tag_obj["title"].get<std::string>());
                                    }
                                    if (tag_obj.contains("id")) {
                                        int tag_id = tag_obj.value("id", 0);
                                        if (tag_id > 0) job_info.tag_ids.push_back(tag_id);
                                    }
                                }

                                // 兼容content/name/id直接在元素上
                                if (tag_item.contains("content") && tag_item["content"].is_string()) {
                                    job_info.tag_names.push_back(tag_item["content"].get<std::string>());
                                } else if (tag_item.contains("name") && tag_item["name"].is_string()) {
                                    job_info.tag_names.push_back(tag_item["name"].get<std::string>());
                                }

                                if (tag_item.contains("id")) {
                                    int tag_id = tag_item.value("id", 0);
                                    if (tag_id > 0) job_info.tag_ids.push_back(tag_id);
                                }
                            }
                        }
                    }
                } catch (const std::exception& e) {
                    print_debug_info("数据解析",
                                     "职位" + std::to_string(i) + "的标签信息提取异常: " + std::string(e.what()),
                                     "", DebugLevel::DL_ERROR);
                }

                // 添加到主表数据列表
                job_info_list.push_back(job_info);
                processed_count++;

                // 每处理5个职位打印一次进度
                if (processed_count % 5 == 0) {
                    print_debug_info("数据解析", "已处理 " + std::to_string(processed_count) + " 个职位");
                }

            } catch (const std::exception& e) {
                DL_ERROR_count++;
                print_debug_info("数据解析",
                                 "处理职位" + std::to_string(i) + "时发生异常: " + std::string(e.what()),
                                 "", DebugLevel::DL_ERROR);
            }
        }

        print_debug_info("数据解析",
                         "解析完成: 成功 " + std::to_string(processed_count) + " 个, 失败 " + std::to_string(DL_ERROR_count) + " 个");

        // 整理地区表数据
        for (const auto& area_pair : area_dict) {
            AreaInfo area_info;
            area_info.area_id = area_pair.second;
            area_info.area_name = area_pair.first;
            mapping_data.area_list.push_back(area_info);
        }

        // 整理类型表数据
        for (const auto& type_pair : type_dict) {
            TypeInfo type_info;
            type_info.type_id = type_pair.first;
            type_info.type_name = type_pair.second;
            mapping_data.type_list.push_back(type_info);
        }

        // 整理薪资档次表数据
        for (const auto& salary_pair : salary_level_dict) {
            SalaryLevelInfo salary_info;
            salary_info.salary_level_id = salary_pair.first;
            salary_info.upper_limit = salary_pair.second;
            mapping_data.salary_level_list.push_back(salary_info);
        }

        print_debug_info("数据解析", "地区表数据: " + std::to_string(mapping_data.area_list.size()) + " 条");
        print_debug_info("数据解析", "类型表数据: " + std::to_string(mapping_data.type_list.size()) + " 条");
        print_debug_info("数据解析", "薪资档次表数据: " + std::to_string(mapping_data.salary_level_list.size()) + " 条");

        return std::make_pair(job_info_list, mapping_data);

    } catch (const std::exception& e) {
        print_debug_info("数据解析",
                         "解析过程中发生异常: " + std::string(e.what()),
                         "", DebugLevel::DL_ERROR);
        return std::make_pair(job_info_list, mapping_data);
    }
}

// 数据格式化打印函数
void print_data_formatted(const std::vector<JobInfo>& job_info_list,
                          const std::vector<TypeInfo>& type_list,
                          const std::vector<AreaInfo>& area_list,
                          const std::vector<SalaryLevelInfo>& salary_level_list) {
    try {
        qDebug() << QString(60, '=');
        qDebug() << "主表信息 (职位信息):";
        qDebug() << QString(60, '=');

        if (job_info_list.empty()) {
            qDebug() << "主表信息为空";
        } else {
            size_t display_count = std::min(job_info_list.size(), size_t(5));
            for (size_t i = 0; i < display_count; ++i) {
                const auto& job = job_info_list[i];
                qDebug() << "记录" << i + 1 << ":";
                qDebug() << "PK: 信息ID (" << job.info_id << ")";
                qDebug() << "信息名称:" << job.info_name.c_str();
                qDebug() << "SK: 类型ID (" << job.type_id << ")";
                qDebug() << "SK: 地区ID (" << job.area_id << ")";
                qDebug() << "SK: 薪资档次ID (" << job.salary_level_id << ")";

                if (!job.requirements.empty()) {
                    std::string preview = job.requirements.substr(0, 100);
                    qDebug() << "岗位要求:" << QString::fromStdString(preview) << "...";
                } else {
                    qDebug() << "岗位要求: 无";
                }

                qDebug() << "薪资:" << job.salary_min << "-" << job.salary_max << "K";
                qDebug() << "创建时间:" << job.create_time.c_str();
                qDebug() << QString(40, '-');
            }
            if (job_info_list.size() > 5) {
                qDebug() << "... 还有" << job_info_list.size() - 5 << "条记录未显示";
            }
        }

        qDebug() << "\n" << QString(60, '=');
        qDebug() << "类型表:";
        qDebug() << QString(60, '=');
        if (type_list.empty()) {
            qDebug() << "类型表为空";
        } else {
            for (const auto& type_item : type_list) {
                qDebug() << "PK: 类型ID (" << type_item.type_id << ")";
                qDebug() << "类型名称:" << type_item.type_name.c_str();
                qDebug() << QString(20, '-');
            }
        }

        qDebug() << "\n" << QString(60, '=');
        qDebug() << "地区表:";
        qDebug() << QString(60, '=');
        if (area_list.empty()) {
            qDebug() << "地区表为空";
        } else {
            for (const auto& area_item : area_list) {
                qDebug() << "PK: 地区ID (" << area_item.area_id << ")";
                qDebug() << "地区名称:" << area_item.area_name.c_str();
                qDebug() << QString(20, '-');
            }
        }

        qDebug() << "\n" << QString(60, '=');
        qDebug() << "薪资档次表:";
        qDebug() << QString(60, '=');
        if (salary_level_list.empty()) {
            qDebug() << "薪资档次表为空";
        } else {
            for (const auto& salary_item : salary_level_list) {
                qDebug() << "PK: 薪资档次ID (" << salary_item.salary_level_id << ")";
                qDebug() << "薪资上限:" << salary_item.upper_limit;
                qDebug() << QString(20, '-');
            }
        }

    } catch (const std::exception& e) {
        print_debug_info("数据打印",
                         "打印数据时发生异常: " + std::string(e.what()),
                         "", DebugLevel::DL_ERROR);
    }
}
