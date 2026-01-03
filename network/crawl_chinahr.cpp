#include "crawl_chinahr.h"
#include "job_crawler.h"
#include <QDebug>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <thread>
#include <chrono>

namespace ChinahrCrawler {

std::map<std::string, std::string> getChinahrHeaders() {
    return {
        {"accept", "application/json, text/plain, */*"},
        {"accept-language", "zh-CN,zh;q=0.9"},
        {"content-type", "application/json;charset=UTF-8"},
        {"referer", "https://www.chinahr.com/job"},
        {"sec-fetch-site", "same-origin"}
    };
}

std::string buildChinahrUrl() {
    return "https://www.chinahr.com/newchr/open/job/search";
}

std::string buildChinahrPostData(int page, int pageSize, const std::string &localId) {
    std::ostringstream ss;
    ss << "{\"page\":" << page
       << ",\"pageSize\":" << pageSize
       << ",\"localId\":\"" << localId << "\",\"minSalary\":null,\"maxSalary\":null,\"keyWord\":\"\"}";
    return ss.str();
}

// Parser: extract JobInfo list, salary parsing mimics zhipin logic, area info fetched from detail page
static size_t _write_to_string(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    std::string* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), realsize);
    return realsize;
}

static std::string fetch_text_page(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return "";
    std::string resp;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _write_to_string);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) return "";
    return resp;
}

std::pair<std::vector<JobInfo>, MappingData> parseChinahrResponse(const json &json_data, int pageSize) {
    std::vector<JobInfo> jobs;
    MappingData mapping;
    try {
        if (!json_data.is_object()) {
            mapping.last_api_code = -1;
            mapping.last_api_message = "invalid json";
            mapping.has_more = false;
            return {jobs, mapping};
        }

        int code = json_data.value("code", -1);
        mapping.last_api_code = code;
        if (code != 1) {
            mapping.last_api_message = json_data.value("error", "");
            mapping.has_more = false;
            return {jobs, mapping};
        }

        // 默认类型映射：TypeInfo 列表先只包含社招
        mapping.type_list.push_back({3, "社招"});

        if (!(json_data.contains("data") && json_data["data"].is_object())) {
            mapping.has_more = false;
            return {jobs, mapping};
        }

        const auto &data = json_data["data"];
        int totalCount = data.value("totalCount", 0);
        mapping.totalPage = (pageSize > 0) ? ((totalCount + pageSize - 1) / pageSize) : 0;

        if (!data.contains("jobItems") || !data["jobItems"].is_array() || data["jobItems"].empty()) {
            // 没有职位则认为无更多
            mapping.has_more = false;
            mapping.last_api_message = "OK";
            return {jobs, mapping};
        }

        mapping.has_more = true;
        const auto &items = data["jobItems"];

        // helper: parse numeric from string
        auto parse_double = [](const std::string& s)->double {
            std::string t;
            for (char c : s) {
                if ((c >= '0' && c <= '9') || c == '.') t.push_back(c);
                else if (!t.empty()) break;
            }
            try { return t.empty() ? 0.0 : std::stod(t); } catch (...) { return 0.0; }
        };

        for (const auto &it : items) {
            try {
                JobInfo job;
                std::string jobId = it.value("jobId", "");
                if (!jobId.empty()) job.info_id = std::hash<std::string>{}(jobId);

                job.info_name = it.value("jobName", "");
                job.company_name = it.value("comName", "");
                std::string comId = it.value("comId", "");
                if (!comId.empty()) job.company_id = std::hash<std::string>{}(comId);

                // 薪资解析：优先判断是否含 K/k
                std::string salary_desc = it.value("salary", "");
                bool hasK = (salary_desc.find('K') != std::string::npos) || (salary_desc.find('k') != std::string::npos);
                if (!salary_desc.empty()) {
                    size_t dash = salary_desc.find('-');
                    if (dash != std::string::npos) {
                        std::string min_s = salary_desc.substr(0, dash);
                        std::string max_s = salary_desc.substr(dash+1);
                        // trim after K or non-number
                        size_t kpos = max_s.find('K'); if (kpos==std::string::npos) kpos = max_s.find('k');
                        if (kpos != std::string::npos) max_s = max_s.substr(0, kpos);
                        job.salary_min = parse_double(min_s);
                        job.salary_max = parse_double(max_s);
                    } else {
                        double v = parse_double(salary_desc);
                        job.salary_min = v;
                        job.salary_max = v;
                    }
                    // If parsed max is zero but min is present, use min as max
                    if (job.salary_max == 0.0 && job.salary_min > 0.0) {
                        job.salary_max = job.salary_min;
                    }
                }

                // 若不含K，自动设为实习 (2)
                job.type_id = hasK ? 3 : 2;
                // 如果 min 超过 100，认定为实习（覆盖任何基于 K 的判断）
                if (job.salary_min > 100.0) {
                    job.type_id = 2;
                }

                // 薪资回退规则：如果没有数字或解析均为0，则设为 [0, 99999]（不改变 type_id）
                if (salary_desc.empty() || (job.salary_min == 0.0 && job.salary_max == 0.0)) {
                    job.salary_min = 0.0;
                    job.salary_max = 99999.0;
                }
                job.area_name = it.value("workPlace", "");

                // 尝试请求详情页并解析 area 信息（简单字符串抽取）以及 requirements
                if (!jobId.empty()) {
                    std::string detail_url = std::string("https://www.chinahr.com/detail/") + jobId;
                    // 尝试抓取详情页；若响应提示“请求过于频繁，请稍后重试！”，则等待并重试（最多重试 5 次）
                    std::string html;
                    const std::string throttle_msg = "请求过于频繁，请稍后重试！";
                    const int max_attempts = 5;
                    int attempt = 0;
                    for (; attempt < max_attempts; ++attempt) {
                        html = fetch_text_page(detail_url);
                        if (html.empty()) break;
                        if (html.find(throttle_msg) != std::string::npos) {
                            std::this_thread::sleep_for(std::chrono::seconds(3));
                            continue;
                        }
                        break;
                    }
                    if (html.empty()) {
                        // empty
                    } else {
                        // 更稳健地提取职位要求：尝试多个可能的标记或关键词
                        auto extract_block = [&](const std::vector<std::string>& markers)->std::string {
                            for (const auto &m : markers) {
                                size_t p = html.find(m);
                                if (p == std::string::npos) continue;
                                // 从标记位置向后取一段文本，去除html标签
                                size_t slice_start = (p > 100) ? p - 20 : 0;
                                size_t slice_len = std::min<size_t>(1500, html.size() - slice_start);
                                std::string slice = html.substr(slice_start, slice_len);
                                std::string out;
                                bool in_tag = false;
                                for (char c : slice) {
                                    if (c == '<') in_tag = true;
                                    else if (c == '>') { in_tag = false; continue; }
                                    if (!in_tag) out.push_back(c);
                                }
                                // 搜索中文关键词，优先“职位描述”，并从关键字之后开始（不含关键字）
                                size_t kw_pos = out.find("职位描述");
                                size_t kw_len = 0;
                                if (kw_pos != std::string::npos) {
                                    kw_len = std::string("职位描述").length();
                                } else {
                                    kw_pos = out.find("职位要求");
                                    if (kw_pos != std::string::npos) kw_len = std::string("职位要求").length();
                                }
                                if (kw_pos != std::string::npos) {
                                    size_t start = kw_pos + kw_len;
                                    // 跳过冒号、中文冒号及空白字符
                                    while (start < out.size() && (isspace(static_cast<unsigned char>(out[start])) || out[start] == ':' )) start++;
                                    // 返回接下来最多 800 字符
                                    return out.substr(start, std::min<size_t>(800, out.size()-start));
                                }
                                // 否则返回去掉前后空白的 out
                                auto l = out.find_first_not_of(" \t\n\r");
                                auto r = out.find_last_not_of(" \t\n\r");
                                if (l != std::string::npos && r != std::string::npos && r >= l) {
                                    return out.substr(l, r-l+1);
                                }
                            }
                            return std::string();
                        };

                        std::vector<std::string> req_markers = {"class=\"detail-des\"", "class=\"detail-desc\"", "class=\"job-des\"", "class=\"job-detail\"", "职位要求", "职位描述"};
                        std::string extracted = extract_block(req_markers);
                        if (!extracted.empty()) {
                            // 截断过长内容，保留合理长度
                            if (extracted.size() > 1000) extracted = extracted.substr(0, 1000);
                            job.requirements = extracted;
                            // Trim any trailing location section starting with "工作地点" (including variants)
                            size_t loc_pos = job.requirements.find("工作地点");
                            if (loc_pos == std::string::npos) {
                                // also try fullwidth colon variant
                                loc_pos = job.requirements.find("工作地点：");
                            }
                            if (loc_pos != std::string::npos) {
                                job.requirements = job.requirements.substr(0, loc_pos);
                                // trim trailing whitespace
                                auto rpos = job.requirements.find_last_not_of(" \t\n\r");
                                if (rpos != std::string::npos) job.requirements = job.requirements.substr(0, rpos+1);
                            } else {
                                // extracted
                            }
                        } else {
                            // no extraction
                        }
                    }
                }

                // 要求/描述：从详情页的 detail-des 区段提取（如果可用）
                // 时间使用当前时间
                auto now = std::chrono::system_clock::now();
                auto time_t_now = std::chrono::system_clock::to_time_t(now);
                std::tm tm_now;
                localtime_s(&tm_now, &time_t_now);
                std::ostringstream time_stream;
                time_stream << std::put_time(&tm_now, "%Y-%m-%d %H:%M:%S");
                job.create_time = time_stream.str();
                job.update_time = time_stream.str();

                // 薪资档次留空（后续可按需求映射）
                job.salary_level_id = 0;

                jobs.push_back(job);
            } catch (...) {
                continue;
            }
        }

        mapping.last_api_code = 0;
        mapping.last_api_message = "OK";
    } catch (const std::exception &e) {
        print_debug_info("ChinahrParser", "解析异常: " + std::string(e.what()), "", DebugLevel::DL_ERROR);
    }

    return {jobs, mapping};
}

std::pair<std::vector<JobInfo>, MappingData> crawlChinahr(int page, int pageSize, const std::string &localId) {
    try {
        static bool curl_initialized = false;
        if (!curl_initialized) {
            curl_global_init(CURL_GLOBAL_DEFAULT);
            curl_initialized = true;
        }

        print_debug_info("ChinahrCrawler", "开始爬取 chinahr 数据", "页码: " + std::to_string(page));
        std::string url = buildChinahrUrl();
        auto headers = getChinahrHeaders();
        std::string post_data = buildChinahrPostData(page, pageSize, localId);

        auto json_data_opt = fetch_job_data(url, headers, post_data);
        if (!json_data_opt) {
            qDebug() << "[警告] Chinahr: 未获取到有效数据";
            return {{}, {}};
        }

        auto [jobs, mapping] = parseChinahrResponse(*json_data_opt, pageSize);

        return {jobs, mapping};
    } catch (const std::exception &e) {
        print_debug_info("ChinahrCrawler", "爬虫异常: " + std::string(e.what()), "", DebugLevel::DL_ERROR);
        return {{}, {}};
    }
}

} // namespace ChinahrCrawler
