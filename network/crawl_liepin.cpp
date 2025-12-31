#include "crawl_liepin.h"
#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QEventLoop>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include "webview2_browser_wrl.h"
#include <curl/curl.h>
#include <thread>
#include <chrono>
#include <memory>

// helper: curl write
static size_t _write_to_string_liepin(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    std::string* s = static_cast<std::string*>(userp);
    s->append(static_cast<char*>(contents), realsize);
    return realsize;
}

static std::string fetch_text_page_liepin(const std::string& url) {
    CURL* curl = curl_easy_init();
    if (!curl) return std::string();
    std::string resp;
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, _write_to_string_liepin);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Windows NT 10.0; Win64; x64)");
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    if (res != CURLE_OK) return std::string();
    return resp;
}

using namespace LiepinCrawler;

std::pair<std::vector<JobInfo>, MappingData> LiepinCrawler::crawlLiepin(int pageNo, int pageSize, const std::string& city, WebView2BrowserWRL* browser) {
    qDebug() << "[LiepinCrawler] Starting capture for page" << pageNo << "pageSize" << pageSize << "city" << QString::fromStdString(city);

    std::unique_ptr<WebView2BrowserWRL> ownedBrowser;
    if (!browser) {
        // 如果未提供浏览器且当前不是 GUI 线程，则返回提示，避免在子线程创建 QWidget 导致断言。
        if (QCoreApplication::instance() && QThread::currentThread() != QCoreApplication::instance()->thread()) {
            qDebug() << "[LiepinCrawler] crawlLiepin 在非GUI线程被调用且未提供 browser，需由主线程提供 WebView2 实例";
            MappingData md;
            md.last_api_code = 38;
            md.last_api_message = "requires_gui_browser";
            return {{}, md};
        }
        ownedBrowser.reset(new WebView2BrowserWRL());
        browser = ownedBrowser.get();
    }
    browser->enableRequestCapture(true);

    QEventLoop loop;
    bool got = false;
    QString rawJson;

    QObject::connect(browser, &WebView2BrowserWRL::responseCaptured, &loop, [&](const QString& url, const QString& body){
        Q_UNUSED(url);
        if (!body.isEmpty()) {
            rawJson = body;
            got = true;
            qDebug() << "[LiepinCrawler] Captured raw API response length:" << rawJson.length();
            if (loop.isRunning()) loop.quit();
        }
    });
    QObject::connect(browser, &WebView2BrowserWRL::navigationFailed, &loop, [&](const QString& reason){
        qDebug() << "[LiepinCrawler] navigationFailed:" << reason;
        if (loop.isRunning()) loop.quit();
    });

    // 构造列表页URL（Liepin API 使用 0-based 页码，因此将传入的 1-based pageNo 转为 apiPage）
    int apiPage = pageNo > 0 ? (pageNo - 1) : 0;
    QString url = QString("https://www.liepin.com/zhaopin/?city=%1&currentPage=%2&pageSize=%3").arg(QString::fromStdString(city)).arg(apiPage).arg(pageSize);

    // 使用 invokeMethod 如果 browser 属于 GUI 线程
    if (QThread::currentThread() != qApp->thread()) {
        QMetaObject::invokeMethod(browser, "fetchCookies", Qt::QueuedConnection, Q_ARG(QString, url));
    } else {
        browser->fetchCookies(url);
    }

    // 等待若干秒以捕获后台API响应
    QTimer::singleShot(12000, &loop, &QEventLoop::quit);
    loop.exec();

    if (!got || rawJson.isEmpty()) {
        qDebug() << "[LiepinCrawler] 未捕获到API响应，请尝试增加等待时间或检查页面加载";
        MappingData md;
        md.last_api_code = 37;
        md.last_api_message = "no_response_captured";
        return {{}, md};
    }

    // 原始响应已由 WebView2 wrapper 持久化为 webmsg_*.json，避免重复写入，这里只记录长度供调试
    qDebug() << "[LiepinCrawler] Captured raw response length:" << rawJson.length() << "(webview2 also saved webmsg_*.json)";

    // 解析 JSON 并构造 JobInfo 列表
    QString payload = rawJson;
    // 有时上层收到的消息是封装对象：{type:'api_response', url:..., body: '...json...' }
    // 如果是这种情况，先提取 body 字段的字符串作为真实负载再解析。
    QJsonDocument topDoc = QJsonDocument::fromJson(payload.toUtf8());
    if (topDoc.isObject()) {
        QJsonObject topObj = topDoc.object();
        if (topObj.contains("body") && topObj.value("body").isString()) {
            QString inner = topObj.value("body").toString();
            qDebug() << "[LiepinCrawler] Detected wrapper message, extracting inner body length:" << inner.length();
            payload = inner;
        } else {
            // not a wrapper, treat payload as-is
        }
    }

    QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8());
    if (!doc.isObject()) {
        qDebug() << "[LiepinCrawler] 捕获内容不是JSON对象，无法解析";
        MappingData md;
        md.last_api_code = 37;
        md.last_api_message = "payload_not_json";
        return {{}, md};
    }
    QJsonObject root = doc.object();
    QJsonObject data = root.value("data").toObject();
    QJsonObject inner = data.value("data").toObject();
    QJsonArray list = inner.value("jobCardList").toArray();
    if (list.isEmpty()) {
        qDebug() << "[LiepinCrawler] jobCardList 为空";
        MappingData md;
        md.last_api_code = 37;
        md.last_api_message = "jobCardList_empty";
        return {{}, md};
    }

    std::vector<JobInfo> jobs;
    jobs.reserve(list.size());

    QRegularExpression numRe("(\\d+(?:\\.\\d+)?)");

    for (const QJsonValue& v : list) {
        QJsonObject item = v.toObject();
        QJsonObject comp = item.value("comp").toObject();
        QJsonObject job = item.value("job").toObject();

        JobInfo ji{};
        // default recruit type to 社招 (3) unless present
        ji.type_id = job.value("jobType").toInt(3);
        // title, id
        ji.info_name = job.value("title").toString().toStdString();
        QString jobIdStr = job.value("jobId").toString();
        if (jobIdStr.isEmpty()) jobIdStr = job.value("jobId").toVariant().toString();
        ji.info_id = jobIdStr.toLongLong();

        // company
        ji.company_name = comp.value("compName").toString().toStdString();
        ji.company_id = comp.value("compId").toInt();

        // area
        ji.area_name = job.value("dq").toString().toStdString();

        // salary parsing (e.g. "35-50k·19薪") -> extract two numbers
        QString salaryStr = job.value("salary").toString();
        ji.salary_min = 0.0;
        ji.salary_max = 0.0;
        if (!salaryStr.isEmpty()) {
            QRegularExpressionMatchIterator it = numRe.globalMatch(salaryStr);
            QVector<double> nums;
            while (it.hasNext()) {
                QRegularExpressionMatch m = it.next();
                nums.append(m.captured(1).toDouble());
            }
            if (nums.size() >= 1) ji.salary_min = nums.at(0);
            if (nums.size() >= 2) ji.salary_max = nums.at(1);
        }

        // 初始化 requirements 为空，若解析详情页失败将回退为结构化摘要（学历 + 工作经验 + 公司行业）
        ji.requirements.clear();

        // times
        QString refresh = job.value("refreshTime").toString();
        if (refresh.length() >= 14) {
            QString ct = refresh.mid(0,4) + '-' + refresh.mid(4,2) + '-' + refresh.mid(6,2) + ' ' + refresh.mid(8,2) + ':' + refresh.mid(10,2) + ':' + refresh.mid(12,2);
            ji.create_time = ct.toStdString();
        }

        // 不再采集 HR 上线时间（保留字段为空）
        ji.hr_last_login.clear();

        // tags (来源于 job.labels)
        ji.tag_names.clear();
        QJsonArray labelsArr = job.value("labels").toArray();
        for (const QJsonValue &lv : labelsArr) {
            if (lv.isString()) {
                ji.tag_names.push_back(lv.toString().toStdString());
            } else if (lv.isObject()) {
                QJsonObject lobj = lv.toObject();
                if (lobj.contains("name")) ji.tag_names.push_back(lobj.value("name").toString().toStdString());
                else if (lobj.contains("label")) ji.tag_names.push_back(lobj.value("label").toString().toStdString());
            }
        }

        // 尝试抓取详情页并解析职位介绍（job-intro-container）
        QString link = item.value("job").toObject().value("link").toString();
        if (!link.isEmpty()) {
            std::string detail_url = link.toStdString();
            // 简单的节奏控制，避免请求过快
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
            std::string html = fetch_text_page_liepin(detail_url);
            if (!html.empty()) {
                // 查找<section class="job-intro-container"> ... </section>
                size_t pos = html.find("<section class=\"job-intro-container\"");
                if (pos != std::string::npos) {
                    size_t end = html.find("</section>", pos);
                    if (end != std::string::npos && end > pos) {
                        std::string slice = html.substr(pos, end - pos + 10);
                        // robustly convert html to text
                        auto html_to_text = [](const std::string& s)->std::string{
                            std::string out;
                            bool in_tag = false;
                            for (size_t i=0;i<s.size();++i) {
                                char c = s[i];
                                if (c == '<') { in_tag = true; continue; }
                                if (c == '>') { in_tag = false; continue; }
                                if (!in_tag) out.push_back(c);
                            }
                            // decode basic HTML entities
                            auto replace_all = [](std::string& str, const std::string& from, const std::string& to){
                                size_t start_pos = 0;
                                while((start_pos = str.find(from, start_pos)) != std::string::npos) {
                                    str.replace(start_pos, from.length(), to);
                                    start_pos += to.length();
                                }
                            };
                            replace_all(out, "&nbsp;", " ");
                            replace_all(out, "&lt;", "<");
                            replace_all(out, "&gt;", ">");
                            replace_all(out, "&amp;", "&");
                            replace_all(out, "&quot;", '"'==0?"\"":"\"");
                            return out;
                        };
                        std::string out = html_to_text(slice);
                        // 查找“职位介绍”关键字，并截取其后的内容
                        size_t kw = out.find("职位介绍");
                        if (kw == std::string::npos) kw = out.find("职位描述");
                        if (kw != std::string::npos) {
                            size_t start = kw;
                            // move past label
                            if (out.compare(start, 4, "职位介绍") == 0) start += 4;
                            else if (out.compare(start, 4, "职位描述") == 0) start += 4;
                            // 跳过冒号/空白
                            while (start < out.size() && (isspace((unsigned char)out[start]) || out[start]==':'|| out[start]=='：')) start++;
                            std::string req = out.substr(start);
                            // 截断到合理长度
                            if (req.size() > 2000) req = req.substr(0,2000);
                            // 去掉首尾空白
                            auto l = req.find_first_not_of(" \t\n\r");
                            auto r = req.find_last_not_of(" \t\n\r");
                            if (l != std::string::npos && r != std::string::npos && r>=l) req = req.substr(l, r-l+1);
                            ji.requirements = req;
                        } else {
                            // 无关键字，使用全部文本片段（去除多余空白）
                            auto l = out.find_first_not_of(" \t\n\r");
                            auto r = out.find_last_not_of(" \t\n\r");
                            if (l != std::string::npos && r != std::string::npos && r>=l) {
                                std::string req = out.substr(l, r-l+1);
                                if (req.size() > 2000) req = req.substr(0,2000);
                                ji.requirements = req;
                            }
                        }
                    }
                } else {
                    // try alternative markers: div.job-intro-container or dd[data-selector="job-intro-content"]
                    size_t pos2 = html.find("<div class=\"job-intro-container\"");
                    if (pos2 != std::string::npos) {
                        size_t end = html.find("</div>", pos2);
                        if (end != std::string::npos && end > pos2) {
                            std::string slice = html.substr(pos2, end - pos2 + 6);
                            auto html_to_text = [](const std::string& s)->std::string{
                                std::string out;
                                bool in_tag = false;
                                for (char c : s) {
                                    if (c == '<') in_tag = true;
                                    else if (c == '>') { in_tag = false; continue; }
                                    if (!in_tag) out.push_back(c);
                                }
                                // basic entities
                                size_t p;
                                while((p = out.find("&nbsp;"))!=std::string::npos) out.replace(p,6," ");
                                while((p = out.find("&amp;"))!=std::string::npos) out.replace(p,5,"&");
                                return out;
                            };
                            std::string out = html_to_text(slice);
                            auto l = out.find_first_not_of(" \t\n\r");
                            auto r = out.find_last_not_of(" \t\n\r");
                            if (l != std::string::npos && r != std::string::npos && r>=l) {
                                std::string req = out.substr(l, r-l+1);
                                if (req.size() > 2000) req = req.substr(0,2000);
                                ji.requirements = req;
                            }
                        }
                    } else {
                        // dd[data-selector="job-intro-content"]
                        std::string marker = "data-selector=\"job-intro-content\"";
                        size_t pos3 = html.find(marker);
                        if (pos3 != std::string::npos) {
                            // find the enclosing tag start and its closing </dd>
                            size_t start_tag = html.rfind('<', pos3);
                            size_t end = html.find("</dd>", pos3);
                            if (start_tag != std::string::npos && end != std::string::npos && end > start_tag) {
                                std::string slice = html.substr(start_tag, end - start_tag);
                                std::string out;
                                bool in_tag = false;
                                for (char c : slice) {
                                    if (c == '<') in_tag = true;
                                    else if (c == '>') { in_tag = false; continue; }
                                    if (!in_tag) out.push_back(c);
                                }
                                auto l = out.find_first_not_of(" \t\n\r");
                                auto r = out.find_last_not_of(" \t\n\r");
                                if (l != std::string::npos && r != std::string::npos && r>=l) {
                                    std::string req = out.substr(l, r-l+1);
                                    if (req.size() > 2000) req = req.substr(0,2000);
                                    ji.requirements = req;
                                }
                            }
                        }
                    }
                }
            }
        }

        // 如果详情页解析未能得到 requirements，则回退为结构化摘要：学历 + 工作经验 + 公司行业
        if (ji.requirements.empty()) {
            QString edu = job.value("requireEduLevel").toString();
            QString work = job.value("requireWorkYears").toString();
            QString industry = comp.value("compIndustry").toString();
            QStringList parts;
            if (!edu.isEmpty()) parts << QString("学历: %1").arg(edu);
            if (!work.isEmpty()) parts << QString("工作经验: %1").arg(work);
            if (!industry.isEmpty()) parts << QString("行业: %1").arg(industry);
            ji.requirements = parts.join("\n").toStdString();
        }

        jobs.push_back(ji);
    }

    // 构造 MappingData 分页信息
    MappingData md;
    QJsonObject pagination = data.value("pagination").toObject();
    // Liepin API uses 0-based currentPage. totalPage may be either a count or the last index.
    int apiCurrent = pagination.value("currentPage").toInt(apiPage);
    int apiTotalRaw = pagination.value("totalPage").toInt(-1);
    if (apiTotalRaw >= 0) {
        // If apiTotalRaw looks like a count (greater than apiCurrent), treat it as count and compute lastIndex = count-1
        int lastIndex = (apiTotalRaw > apiCurrent) ? (apiTotalRaw - 1) : apiTotalRaw;
        md.currentPage = apiCurrent + 1;        // convert to 1-based
        md.totalPage = lastIndex + 1;         // total pages as 1-based count
        md.has_more = (apiCurrent < lastIndex);
    } else {
        // fallback: use hasNext if totalPage is not present
        md.currentPage = apiCurrent + 1;
        md.totalPage = md.currentPage;
        md.has_more = !pagination.value("hasNext").toBool(false);
    }
    md.last_api_code = root.value("flag").toInt(1);
    md.last_api_message = "parsed";

    qDebug() << "[LiepinCrawler] Parsed" << jobs.size() << "jobs from captured response";

    return {jobs, md};
}
