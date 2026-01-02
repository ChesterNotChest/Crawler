// presenter/presenter_mask.cpp
#include "presenter.h"
#include <QRegularExpression>
#include <QDebug>

QVector<SQLNS::JobInfoPrint> Presenter::filterJobsByCity(const QVector<SQLNS::JobInfoPrint>& jobs, const QString& cityQuery) {
    if (cityQuery.isEmpty()) return jobs;
    QVector<SQLNS::JobInfoPrint> filteredJobs;
    for (const auto& job : jobs) {
        if (job.cityName.contains(cityQuery, Qt::CaseInsensitive)) {
            filteredJobs.append(job);
        }
    }
    return filteredJobs;
}


// 搜索实现：模糊匹配名称/公司/城市/标签；若为纯数字，则仅按 jobId 精确匹配
QVector<SQLNS::JobInfoPrint> Presenter::searchJobs(const QVector<SQLNS::JobInfoPrint>& source,
                                             const QString& query) {
    QVector<SQLNS::JobInfoPrint> ret;
    if (query.isEmpty()) return source;

    // 检查是否为纯数字（用于匹配 jobId）
    QRegularExpression digitsRegex("^\\d+$");
    bool isDigits = digitsRegex.match(query.trimmed()).hasMatch();

    // 若为数字，则尝试按 jobId 精确匹配并返回
    if (isDigits) {
        bool ok = false;
        long long qid = query.toLongLong(&ok);
        if (ok) {
            for (const auto &job : source) {
                if (job.jobId == qid) ret.append(job);
            }
        }
        return ret;
    }

    // 非数字：按模糊文本匹配 jobName / requirements / companyName / cityName / tagNames
    for (const auto &job : source) {
        if (job.jobName.contains(query, Qt::CaseInsensitive)) { ret.append(job); continue; }
        if (job.requirements.contains(query, Qt::CaseInsensitive)) { ret.append(job); continue; }
        if (job.companyName.contains(query, Qt::CaseInsensitive)) { ret.append(job); continue; }
        if (job.cityName.contains(query, Qt::CaseInsensitive)) { ret.append(job); continue; }
        bool tagMatched = false;
        for (const auto &t : job.tagNames) {
            if (t.contains(query, Qt::CaseInsensitive)) { tagMatched = true; break; }
        }
        if (tagMatched) { ret.append(job); continue; }
    }
    return ret;
}

// 搜索实现（按字段映射）：QMap<字段名, QVector<值>>
// 语义：不同字段之间使用 AND，字段内部值使用 OR
QVector<SQLNS::JobInfoPrint> Presenter::searchJobs(const QVector<SQLNS::JobInfoPrint>& source,
                                                   const QMap<QString, QVector<QString>>& fieldFilters) {
    QVector<SQLNS::JobInfoPrint> ret;
    if (fieldFilters.isEmpty()) return source;

    for (const auto &job : source) {
        bool include = true;
        // 遍历每个字段过滤条件（AND）
        for (auto it = fieldFilters.constBegin(); it != fieldFilters.constEnd(); ++it) {
            const QString field = it.key();
            const QVector<QString> vals = it.value();
            // 如果该字段的筛选值为空，则跳过该字段的筛选
            if (vals.isEmpty()) continue;
            bool fieldMatched = false;

            if (field == "jobId") {
                // jobId: exact numeric match; accept if any value matches
                for (const auto &v : vals) {
                    bool ok = false;
                    long long q = v.toLongLong(&ok);
                    if (ok && job.jobId == q) { fieldMatched = true; break; }
                }
            } else if (field == "jobName") {
                for (const auto &v : vals) {
                    if (job.jobName.contains(v, Qt::CaseInsensitive)) { fieldMatched = true; break; }
                }
            } else if (field == "requirements") {
                for (const auto &v : vals) {
                    if (job.requirements.contains(v, Qt::CaseInsensitive)) { fieldMatched = true; break; }
                }
            } else if (field == "companyName") {
                for (const auto &v : vals) {
                    if (job.companyName.contains(v, Qt::CaseInsensitive)) { fieldMatched = true; break; }
                }
            } else if (field == "cityName") {
                for (const auto &v : vals) {
                    if (job.cityName.contains(v, Qt::CaseInsensitive)) { fieldMatched = true; break; }
                }
            } else if (field == "sourceName") {
                for (const auto &v : vals) {
                    if (job.sourceName.contains(v, Qt::CaseInsensitive)) { fieldMatched = true; break; }
                }
            } else if (field == "recruitTypeName") {
                for (const auto &v : vals) {
                    if (job.recruitTypeName.contains(v, Qt::CaseInsensitive)) { fieldMatched = true; break; }
                }
            } else if (field == "salary") {
                    // salary filter now uses salarySlabId (integer) matching
                    for (const auto &v : vals) {
                        bool ok = false;
                        int q = v.toInt(&ok);
                        if (!ok) continue;
                        if (job.salarySlabId == q) { fieldMatched = true; break; }
                }
            } else if (field == "tagNames" || field == "tags") {
                for (const auto &v : vals) {
                    for (const auto &t : job.tagNames) {
                        if (t.contains(v, Qt::CaseInsensitive)) { fieldMatched = true; break; }
                    }
                    if (fieldMatched) break;
                }
            } else if (field == "tagIds") {
                // numeric tag id matching
                for (const auto &v : vals) {
                    bool ok = false;
                    int q = v.toInt(&ok);
                    if (!ok) continue;
                    if (job.tagIds.contains(q)) { fieldMatched = true; break; }
                }
            } else {
                // 未知字段：不做任何回退匹配（避免意外命中）。记录调试信息以便排查。
                // qDebug() << "Presenter::searchJobs: unknown filter field='" << field << "' - will not match any job";
                fieldMatched = false; // 明确标记为不匹配
            }

            if (!fieldMatched) { include = false; break; }
        }

        if (include) ret.append(job);
    }

    return ret;
}