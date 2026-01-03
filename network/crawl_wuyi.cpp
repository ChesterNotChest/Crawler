#include "crawl_wuyi.h"
#include <QDebug>
#include <QTimer>
#include <QEventLoop>
#include <QThread>
#include "webview2_browser_wrl.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QRegularExpression>
#include <QDir>
#include <QFile>
#include <QCoreApplication>
#include <QDateTime>

using namespace WuyiCrawler;

std::pair<std::vector<JobInfo>, MappingData> WuyiCrawler::crawlWuyi(int pageNo, int pageSize, const std::string& city, WebView2BrowserWRL* externalBrowser) {
    qDebug() << "[WuyiCrawler] Starting capture for page" << pageNo << "pageSize" << pageSize << "city" << QString::fromStdString(city)
             << " externalBrowser=" << (externalBrowser?"yes":"no");

    QEventLoop loop;
    bool got = false;
    QString rawJson;

    // If externalBrowser provided, use it (caller controls navigation and clickNext). Otherwise create a temporary one.
    WebView2BrowserWRL* browserPtr = nullptr;
    std::unique_ptr<WebView2BrowserWRL> ownedBrowser;
    if (externalBrowser) {
        browserPtr = externalBrowser;
        // Ensure capture is enabled on external browser and for pages >1, request a click to navigate
        QMetaObject::invokeMethod(browserPtr, "enableRequestCapture", Qt::QueuedConnection, Q_ARG(bool, true));
        if (pageNo > 1) {
            // Ask GUI thread to click next; crawlWuyi will then wait for the responseCaptured
            QMetaObject::invokeMethod(browserPtr, "clickNext", Qt::QueuedConnection);
        }
    } else {
        // 如果未提供外部浏览器且当前在非GUI线程，则返回提示，避免在子线程创建 QWidget
        if (QCoreApplication::instance() && QThread::currentThread() != QCoreApplication::instance()->thread()) {
            qDebug() << "[WuyiCrawler] crawlWuyi 在非GUI线程被调用且未提供 browser，需由主线程提供 WebView2 实例";
            MappingData md;
            md.last_api_code = 38;
            md.last_api_message = "requires_gui_browser";
            return {{}, md};
        }
        ownedBrowser.reset(new WebView2BrowserWRL());
        browserPtr = ownedBrowser.get();
        browserPtr->enableRequestCapture(true);
        // Navigate to initial page
        QString url = QString("https://we.51job.com/pc/search?");
        browserPtr->fetchCookies(url);
    }

    QObject::connect(browserPtr, &WebView2BrowserWRL::responseCaptured, &loop, [&](const QString& url, const QString& body){
        Q_UNUSED(url);
        if (!body.isEmpty()) {
            rawJson = body;
            got = true;
            qDebug() << "[WuyiCrawler] Captured response length:" << rawJson.length();
            if (loop.isRunning()) loop.quit();
        }
    });

    QObject::connect(browserPtr, &WebView2BrowserWRL::navigationFailed, &loop, [&](const QString& reason){
        qDebug() << "[WuyiCrawler] navigationFailed:" << reason;
        if (loop.isRunning()) loop.quit();
    });

    // Wait for capture (caller may have navigated); timeout after 12s
    QTimer::singleShot(12000, &loop, &QEventLoop::quit);
    loop.exec();

    if (!got || rawJson.isEmpty()) {
        qDebug() << "[WuyiCrawler] No API response captured";
        MappingData md;
        md.last_api_code = 37;
        md.last_api_message = "no_response_captured";
        return {{}, md};
    }

    // No longer persisting raw JSON to disk; keep payload in-memory for mapping.
    qDebug() << "[WuyiCrawler] Raw response captured (not saved to disk). Length:" << rawJson.length();

    // 先尝试像 Liepin 一样解析可能的 wrapper：{ type:..., url:..., body: '...json...' }
    QString payload = rawJson;
    QJsonDocument topDoc = QJsonDocument::fromJson(payload.toUtf8());
    if (topDoc.isObject()) {
        QJsonObject topObj = topDoc.object();
        if (topObj.contains("body") && topObj.value("body").isString()) {
            QString inner = topObj.value("body").toString();
            qDebug() << "[WuyiCrawler] Detected wrapper message, extracted inner body length:" << inner.length();
            payload = inner;
        }
    }

    // 解析 payload，如果不是 JSON object 则回退并告知
    QJsonDocument doc = QJsonDocument::fromJson(payload.toUtf8());
    if (!doc.isObject()) {
        qDebug() << "[WuyiCrawler] payload is not a JSON object, saved raw for inspection";
        MappingData md;
        md.currentPage = pageNo;
        md.totalPage = pageNo;
        md.has_more = true; // 继续由外部控制翻页
        md.last_api_code = 37;
        md.last_api_message = "payload_not_json_saved_raw";
        return {{}, md};
    }

    QJsonObject root = doc.object();

    // 尝试抽取分页信息（通用尝试，多个可能命名）
    int currentPageOut = pageNo;
    int totalPageOut = pageNo;
    bool hasMoreOut = true;

    if (root.contains("data") && root.value("data").isObject()) {
        QJsonObject dataObj = root.value("data").toObject();
        QJsonObject pageObj;
        if (dataObj.contains("pagination") && dataObj.value("pagination").isObject()) pageObj = dataObj.value("pagination").toObject();
        else if (dataObj.contains("page") && dataObj.value("page").isObject()) pageObj = dataObj.value("page").toObject();

        if (!pageObj.isEmpty()) {
            int apiCurrent = pageObj.value("currentPage").toInt(pageNo-1);
            if (apiCurrent == (pageNo-1)) {
                // ok
            } else {
                apiCurrent = pageObj.value("page").toInt(pageNo-1);
            }
            int apiTotalRaw = pageObj.value("totalPage").toInt(-1);
            if (apiTotalRaw < 0) apiTotalRaw = pageObj.value("total").toInt(-1);
            if (apiTotalRaw >= 0) {
                int lastIndex = (apiTotalRaw > apiCurrent) ? (apiTotalRaw - 1) : apiTotalRaw;
                currentPageOut = apiCurrent + 1;
                totalPageOut = lastIndex + 1;
                hasMoreOut = (apiCurrent < lastIndex);
            } else {
                currentPageOut = apiCurrent + 1;
                totalPageOut = currentPageOut;
                hasMoreOut = pageObj.value("hasNext").toBool(true);
            }
        } else {
            // 直接在 data 下找常见字段
            if (dataObj.contains("currentPage")) currentPageOut = dataObj.value("currentPage").toInt(pageNo) + 1;
            if (dataObj.contains("totalPage")) totalPageOut = dataObj.value("totalPage").toInt(pageNo);
            if (dataObj.contains("hasMore")) hasMoreOut = dataObj.value("hasMore").toBool(true);
        }
    }

    MappingData md;
    md.currentPage = currentPageOut;
    md.totalPage = totalPageOut;
    md.has_more = hasMoreOut;
    // last_api_code/last_api_message 尝试从根对象中取通用字段，若无则标记为 parsed
    md.last_api_code = root.value("code").toInt(root.value("flag").toInt(0));
    if (md.last_api_code == 0) md.last_api_code = 0; // keep 0 if not present
    md.last_api_message = root.value("message").toString("parsed").toStdString();

    qDebug() << "[WuyiCrawler] Prepared mapping metadata: currentPage=" << md.currentPage << " totalPage=" << md.totalPage << " has_more=" << md.has_more;

    // 尝试从 JSON 中解析职位列表（resultbody.job.items）并映射为 JobInfo
    std::vector<JobInfo> jobs;
    if (root.contains("resultbody") && root.value("resultbody").isObject()) {
        QJsonObject rb = root.value("resultbody").toObject();
        if (rb.contains("job") && rb.value("job").isObject()) {
            QJsonObject jobObj = rb.value("job").toObject();
            QJsonArray items = jobObj.value("items").toArray();
            for (const QJsonValue &it : items) {
                if (!it.isObject()) continue;
                QJsonObject o = it.toObject();
                JobInfo ji{};
                // id/title
                QString jid = o.value("jobId").toString();
                if (jid.isEmpty()) jid = o.value("jobId").toVariant().toString();
                ji.info_id = jid.toLongLong();
                ji.info_name = o.value("jobName").toString().toStdString();

                // company
                ji.company_name = o.value("companyName").toString().toStdString();
                QString coId = o.value("coId").toString();
                if (coId.isEmpty()) coId = o.value("coId").toVariant().toString();
                ji.company_id = coId.toInt();

                // area
                ji.area_name = o.value("jobAreaString").toString().toStdString();
                ji.area_id = o.value("jobAreaCode").toInt();

                // salary: normalize to K units (thousands). incoming fields may be in yuan (e.g. 6000)
                ji.salary_min = 0.0;
                ji.salary_max = 0.0;
                QString smin = o.value("jobSalaryMin").toString();
                QString smax = o.value("jobSalaryMax").toString();
                double minv = 0.0, maxv = 0.0;
                if (!smin.isEmpty()) minv = smin.toDouble();
                if (!smax.isEmpty()) maxv = smax.toDouble();
                // If values look like yuan (>=1000), convert to K
                if (minv >= 1000.0) minv = minv / 1000.0;
                if (maxv >= 1000.0) maxv = maxv / 1000.0;
                // fallback: parse provideSalaryString like "6-9千"
                if (minv == 0.0 && maxv == 0.0) {
                    QString provideSalary = o.value("provideSalaryString").toString();
                    if (!provideSalary.isEmpty()) {
                        QRegularExpression numRe("(\\d+(?:\\.\\d+)?)");
                        QRegularExpressionMatchIterator it = numRe.globalMatch(provideSalary);
                        QVector<double> nums;
                        while (it.hasNext()) nums.append(it.next().captured(1).toDouble());
                        if (!nums.isEmpty()) minv = nums.at(0);
                        if (nums.size() >= 2) maxv = nums.at(1);
                        // if string contains '千' or 'k' then values are already in K
                        if (!(provideSalary.contains(QStringLiteral("千")) || provideSalary.contains('k') || provideSalary.contains('K'))) {
                            if (minv >= 1000.0) minv = minv / 1000.0;
                            if (maxv >= 1000.0) maxv = maxv / 1000.0;
                        }
                    }
                }
                ji.salary_min = minv;
                ji.salary_max = maxv;

                // requirements (use jobDescribe directly)
                ji.requirements = o.value("jobDescribe").toString().toStdString();

                // times
                ji.create_time = o.value("issueDateString").toString().toStdString();
                ji.update_time = o.value("updateDateTime").toString().toStdString();

                // tags (jobTags or jobTagsList)
                ji.tag_names.clear();
                QJsonArray tags1 = o.value("jobTags").toArray();
                for (const QJsonValue &tv : tags1) if (tv.isString()) ji.tag_names.push_back(tv.toString().toStdString());
                QJsonArray tags2 = o.value("jobTagsList").toArray();
                for (const QJsonValue &tv : tags2) if (tv.isObject()) {
                    QJsonObject tobj = tv.toObject();
                    if (tobj.contains("jobTagName")) ji.tag_names.push_back(tobj.value("jobTagName").toString().toStdString());
                }

                // type and area ids
                int tval = o.value("jobType").toInt(-1);
                // default Wuyi to 社招 (3) when jobType absent/unknown
                ji.type_id = (tval <= 0) ? 3 : tval;

                jobs.push_back(ji);
            }

            // pagination: prefer totalcount/totalCount
            int totalCount = jobObj.value("totalCount").toInt(-1);
            if (totalCount < 0) totalCount = jobObj.value("totalcount").toInt(-1);
            if (totalCount >= 0 && pageSize > 0) {
                int lastPage = (totalCount + pageSize - 1) / pageSize;
                md.totalPage = lastPage;
                md.currentPage = pageNo;
                md.has_more = (pageNo < lastPage);
            }
        }
    }

    qDebug() << "[WuyiCrawler] Parsed" << jobs.size() << "jobs from captured response";
    return {jobs, md};
}
