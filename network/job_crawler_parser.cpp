#include "job_crawler.h"
#include <iostream>
#include <vector>
#include <map>
#include <cstdint>

// Shared JSON-safe getters are declared in job_crawler.h

// 数据解析函数
std::pair<std::vector<JobInfo>, MappingData> parse_job_data(const json& json_data) {
    std::vector<JobInfo> job_info_list;
    MappingData mapping_data;

    // print_debug_info("数据解析", "开始解析JSON数据");
    // // Raw JSON debug (full text)
    // try {
    //     std::string raw = json_data.dump();
    //     print_debug_info("raw", "json full", raw);
    // } catch (...) {
    //     print_debug_info("raw", "json dump failed", "");
    // }

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
        // 提取分页信息（安全读取）
        mapping_data.currentPage = get_int_safe(data_field, "currentPage", 0);
        mapping_data.totalPage   = get_int_safe(data_field, "totalPage", 0);
        print_debug_info("Pagination", "currentPage: " + std::to_string(mapping_data.currentPage) + 
                         ", totalPage: " + std::to_string(mapping_data.totalPage));

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

                // 信息ID（使用64位，避免溢出）
                job_info.info_id = get_int64_safe(job_data, "id", static_cast<int64_t>(0));
                if (job_info.info_id >= 10000000000000LL) { // extremely large id, dump raw job json
                    try {
                        print_debug_info("HugeJobId", "Encountered very large jobId: " + std::to_string(job_info.info_id));
                        std::string raw_job = job_data.dump();
                        print_debug_info("HugeJobId", "raw job json", raw_job);
                    } catch (...) {
                        print_debug_info("HugeJobId", "raw dump failed");
                    }
                }

                // 信息名称: 优先 jobName，兼容新样式 jobTitle
                job_info.info_name = get_string_safe(job_data, "jobName", "");
                if (job_info.info_name.empty()) {
                    job_info.info_name = get_string_safe(job_data, "jobTitle", "");
                }

                // 类型ID
                job_info.type_id = get_int_safe(job_data, "recruitType", 0);

                // 类型名称映射
                std::string type_name = "未知";
                if (type_name_map.find(job_info.type_id) != type_name_map.end()) {
                    type_name = type_name_map[job_info.type_id];
                }
                type_dict[job_info.type_id] = type_name;

                // 地区ID和地区名称: 先 jobCity，兼容新样式 city；再退而求其次 extraInfo.jobCity_var
                std::string job_city = get_string_safe(job_data, "jobCity", "");
                if (job_city.empty() && job_data.contains("jobCityList")) {
                    auto job_city_list = job_data["jobCityList"];
                    if (job_city_list.is_array() && !job_city_list.empty()) {
                        auto first_city = job_city_list[0];
                        if (first_city.is_string()) {
                            job_city = first_city.get<std::string>();
                        }
                    }
                }
                if (job_city.empty()) {
                    job_city = get_string_safe(job_data, "city", "");
                }
                if (job_city.empty() && job_data.contains("extraInfo") && job_data["extraInfo"].is_object()) {
                    job_city = get_string_safe(job_data["extraInfo"], "jobCity_var", "");
                }

                if (!job_city.empty() && area_dict.find(job_city) == area_dict.end()) {
                    area_dict[job_city] = area_id_counter;
                    area_id_counter++;
                }
                job_info.area_id = (job_city.empty()) ? 0 : area_dict[job_city];
                job_info.area_name = job_city;

                // 薪资档次ID
                int salary_max_for_slab = get_int_safe(job_data, "salaryMax", 0);
                job_info.salary_level_id = 0;
                for (size_t level_id = 0; level_id < salary_thresholds.size(); ++level_id) {
                    auto threshold = salary_thresholds[level_id];
                    if (threshold.first <= salary_max_for_slab && salary_max_for_slab <= threshold.second) {
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

                        if (!job_info.requirements.empty()) {
                            job_info.requirements = sanitize_html_to_text(job_info.requirements);
                            if (job_info.requirements.length() > 5000) {
                                job_info.requirements = job_info.requirements.substr(0, 5000);
                            }
                        }
                    } catch (const std::exception& e) {
                        print_debug_info("数据解析",
                                         "职位" + std::to_string(i) + "的ext字段处理异常: " + std::string(e.what()),
                                         "", DebugLevel::DL_ERROR);
                        job_info.requirements = "";
                    }
                }

                // 薪资最小值/最大值 + 调试输出
                int salary_min = 0;
                int salary_max = 0;
                bool has_range_salary = job_data.contains("salaryMin") || job_data.contains("salaryMax");
                if (has_range_salary) {
                    salary_min = get_int_safe(job_data, "salaryMin", 0);
                    salary_max = get_int_safe(job_data, "salaryMax", 0);
                } else if (job_data.contains("salary")) {
                    // 新样式仅有单一 salary 文本，需求：最高与最低都设为0
                    salary_min = 0;
                    salary_max = 0;
                }
                job_info.salary_min = salary_min;
                job_info.salary_max = salary_max;
                print_debug_info("parse", "Job now: " + job_info.info_name +
                    "salaryMin=" + std::to_string(salary_min) + ", salaryMax=" + std::to_string(salary_max));

                // 创建时间 + 调试输出
                int64_t create_time = get_int64_safe(job_data, "createTime", static_cast<int64_t>(0));
                job_info.create_time = timestamp_to_datetime(create_time);
                print_debug_info("parse", "Job now: " + job_info.info_name +
                    "createTime_ms=" + std::to_string(create_time) + ", parsed=" + job_info.create_time);

                // 更新时间
                int64_t update_time = get_int64_safe(job_data, "updateTime", create_time);
                job_info.update_time = timestamp_to_datetime(update_time);

                // HR最后登录时间
                int64_t hr_login_time = 0;
                try {
                    if (job_data.contains("user") && job_data["user"].is_object()) {
                        auto user_data = job_data["user"];
                        if (user_data.contains("loginTime")) {
                            hr_login_time = get_int64_safe(user_data, "loginTime", static_cast<int64_t>(0));
                        }
                    }
                } catch (const std::exception& e) {
                    print_debug_info("数据解析",
                                     "职位" + std::to_string(i) + "的HR登录时间提取异常: " + std::string(e.what()),
                                     "", DebugLevel::DL_ERROR);
                }
                job_info.hr_last_login = timestamp_to_datetime(hr_login_time);

                // 公司ID与公司名称：优先来自 user.identity 列表；兼容新样式顶层 companyId/companyName
                job_info.company_id = 0;
                try {
                    if (job_data.contains("user") && job_data["user"].is_object()) {
                        auto user_data = job_data["user"];
                        if (user_data.contains("identity") && user_data["identity"].is_array()) {
                            auto identity_list = user_data["identity"];
                            for (const auto& identity : identity_list) {
                                if (!identity.is_object()) continue;
                                if (identity.contains("companyId")) {
                                    job_info.company_id = get_int_safe(identity, "companyId", 0);
                                }
                                if (identity.contains("companyName")) {
                                    job_info.company_name = get_string_safe(identity, "companyName", "");
                                }
                                if (job_info.company_id > 0 && !job_info.company_name.empty()) break;
                            }
                        } else {
                            print_debug_info("数据解析", "缺少identity列表", "", DebugLevel::DL_ERROR);
                        }
                    }
                } catch (const std::exception& e) {
                    print_debug_info("数据解析",
                                     "职位" + std::to_string(i) + "的公司信息提取异常: " + std::string(e.what()),
                                     "", DebugLevel::DL_ERROR);
                }

                // 兼容：若 identity 未提供则尝试顶层 companyId/companyName（新样式）
                if ((job_info.company_id == 0 || job_info.company_name.empty())) {
                    int fallback_company_id = get_int_safe(job_data, "companyId", 0);
                    std::string fallback_company_name = get_string_safe(job_data, "companyName", "");
                    if (fallback_company_id > 0 && !fallback_company_name.empty()) {
                        job_info.company_id = fallback_company_id;
                        job_info.company_name = fallback_company_name;
                    }
                }

                if (job_info.company_id == 0 || job_info.company_name.empty()) {
                    print_debug_info("数据解析", "公司信息缺失 (未提供companyId/companyName)", "", DebugLevel::DL_ERROR);
                }

                // 兼容：若无 ext.requirements，则尝试直接使用 description 作为要求（截断）
                if (job_info.requirements.empty() && job_data.contains("description") && job_data["description"].is_string()) {
                    job_info.requirements = sanitize_html_to_text(job_data["description"].get<std::string>());
                    if (job_info.requirements.length() > 5000) {
                        job_info.requirements = job_info.requirements.substr(0, 5000);
                    }
                }

                // 标签内容列表提取（tag.title 优先）
                try {
                    if (job_data.contains("pcTagInfo") && job_data["pcTagInfo"].is_object()) {
                        auto tag_info = job_data["pcTagInfo"];
                        if (tag_info.contains("jobInfoTagList") && tag_info["jobInfoTagList"].is_array()) {
                            auto tag_list = tag_info["jobInfoTagList"];
                            for (const auto& tag_item : tag_list) {
                                if (!tag_item.is_object()) continue;

                                // tag.title / tag.id
                                if (tag_item.contains("tag") && tag_item["tag"].is_object()) {
                                    auto tag_obj = tag_item["tag"];
                                    if (tag_obj.contains("title") && tag_obj["title"].is_string()) {
                                        job_info.tag_names.push_back(tag_obj["title"].get<std::string>());
                                    }
                                    if (tag_obj.contains("id")) {
                                        int tag_id = get_int_safe(tag_obj, "id", 0);
                                        if (tag_id > 0) job_info.tag_ids.push_back(tag_id);
                                    }
                                }

                                // 兼容 content/name/id 直接在元素上
                                if (tag_item.contains("content") && tag_item["content"].is_string()) {
                                    job_info.tag_names.push_back(tag_item["content"].get<std::string>());
                                } else if (tag_item.contains("name") && tag_item["name"].is_string()) {
                                    job_info.tag_names.push_back(tag_item["name"].get<std::string>());
                                }
                                if (tag_item.contains("id")) {
                                    int tag_id = get_int_safe(tag_item, "id", 0);
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

                // 基本字段完整性校验：缺少关键字段则跳过该职位
                if (job_info.info_id == 0 || job_info.info_name.empty()) {
                    print_debug_info("数据解析", "跳过无效职位: 缺少id或jobName", "", DebugLevel::DL_ERROR);
                    continue;
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
