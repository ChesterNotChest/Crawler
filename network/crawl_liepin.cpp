#include "crawl_liepin.h"
#include <QDebug>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QCoreApplication>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include "webview2_browser_wrl.h"

using namespace LiepinCrawler;

std::pair<std::vector<JobInfo>, MappingData> LiepinCrawler::crawlLiepin(int pageNo, int pageSize, const std::string& city) {
    qDebug() << "[LiepinCrawler] Starting capture for page" << pageNo << "pageSize" << pageSize << "city" << QString::fromStdString(city);

    WebView2BrowserWRL browser;
    browser.enableRequestCapture(true);

    QEventLoop loop;
    bool got = false;
    QString rawJson;

    QObject::connect(&browser, &WebView2BrowserWRL::responseCaptured, [&](const QString& url, const QString& body){
        Q_UNUSED(url);
        if (!body.isEmpty()) {
            rawJson = body;
            got = true;
            qDebug() << "[LiepinCrawler] Captured raw API response length:" << rawJson.length();
            qDebug() << "[LiepinCrawler] Captured raw API response (truncated to 64k for log):";
            QString toPrint = rawJson;
            if (toPrint.length() > 65536) toPrint = toPrint.left(65536) + "...[truncated]";
            qDebug().noquote() << toPrint;
            if (loop.isRunning()) loop.quit();
        }
    });
    QObject::connect(&browser, &WebView2BrowserWRL::navigationFailed, [&](const QString& reason){
        qDebug() << "[LiepinCrawler] navigationFailed:" << reason;
        if (loop.isRunning()) loop.quit();
    });

    // 构造列表页URL（以pageNo做示例）
    QString url = QString("https://www.liepin.com/zhaopin/?city=%1&currentPage=%2&pageSize=%3").arg(QString::fromStdString(city)).arg(pageNo).arg(pageSize);
    browser.fetchCookies(url);

    // 等待若干秒以捕获后台API响应
    QTimer::singleShot(12000, &loop, &QEventLoop::quit);
    loop.exec();

    if (!got || rawJson.isEmpty()) {
        qDebug() << "[LiepinCrawler] 未捕获到API响应，请尝试增加等待时间或检查页面加载";
        return {{}, {}};
    }

    // 保存原始响应到文件，便于离线检查
    QDir d(QCoreApplication::applicationDirPath());
    QString outDir = d.absoluteFilePath("liepin_captured");
    QDir().mkpath(outDir);
    QString outPath = outDir + QString("/liepin_page_%1.json").arg(pageNo);
    QFile f(outPath);
    if (f.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
        f.write(rawJson.toUtf8());
        f.close();
        qDebug() << "[LiepinCrawler] Raw API response saved to:" << outPath;
    } else {
        qDebug() << "[LiepinCrawler] Failed to write captured response to" << outPath;
    }

    // 尝试解析一次，给出字段映射建议（仅用于确认，不做最终解析）
    QJsonDocument doc = QJsonDocument::fromJson(rawJson.toUtf8());
    if (!doc.isObject()) {
        qDebug() << "[LiepinCrawler] 捕获内容不是JSON对象，无法生成映射建议";
        return {{}, {}};
    }
    QJsonObject root = doc.object();
    QJsonObject data = root.value("data").toObject();
    QJsonObject inner = data.value("data").toObject();
    QJsonArray list = inner.value("jobCardList").toArray();
    if (list.isEmpty()) {
        qDebug() << "[LiepinCrawler] jobCardList 为空，无法生成映射建议";
        return {{}, {}};
    }

    QJsonObject first = list.at(0).toObject();
    QJsonObject comp = first.value("comp").toObject();
    QJsonObject job = first.value("job").toObject();

    QJsonObject mappingSuggestion;
    mappingSuggestion["company_name"] = QString("data.data.jobCardList[0].comp.compName");
    mappingSuggestion["company_id"] = QString("data.data.jobCardList[0].comp.compId");
    mappingSuggestion["job_title"] = QString("data.data.jobCardList[0].job.title");
    mappingSuggestion["job_link"] = QString("data.data.jobCardList[0].job.link");
    mappingSuggestion["job_id"] = QString("data.data.jobCardList[0].job.jobId");
    mappingSuggestion["salary"] = QString("data.data.jobCardList[0].job.salary");
    mappingSuggestion["location"] = QString("data.data.jobCardList[0].job.dq");
    mappingSuggestion["require_work_years"] = QString("data.data.jobCardList[0].job.requireWorkYears");
    mappingSuggestion["require_edu"] = QString("data.data.jobCardList[0].job.requireEduLevel");

    qDebug() << "[LiepinCrawler] 建议映射（请确认后我将继续实现parser）：" << QJsonDocument(mappingSuggestion).toJson(QJsonDocument::Compact);

    // 将建议映射写入文件，供人工确认
    QString mapPath = outDir + "/liepin_mapping_suggestion.json";
    QFile mf(mapPath);
    if (mf.open(QIODevice::WriteOnly|QIODevice::Truncate)) {
        mf.write(QJsonDocument(mappingSuggestion).toJson(QJsonDocument::Indented));
        mf.close();
        qDebug() << "[LiepinCrawler] 映射建议已写入：" << mapPath;
    }

    // 暂不做解析，返回空集合并预填MappingData的分页信息
    MappingData md;
    md.currentPage = pageNo;
    md.last_api_code = 1;
    md.last_api_message = "captured_raw_json_saved";

    return {{}, md};
}
