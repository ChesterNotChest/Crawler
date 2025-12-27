// presenter/presenter_mask.cpp
#include "presenter.h"

QVector<SQLNS::JobInfo> Presenter::filterJobsByCity(const QVector<SQLNS::JobInfo>& jobs, int cityId) {
    QVector<SQLNS::JobInfo> filteredJobs;
    for (const auto& job : jobs) {
        if (job.cityId == cityId) {
            filteredJobs.append(job);
        }
    }
    return filteredJobs;
}


// 搜索实现：按名称/要求/标签（全部条件为 AND）
QVector<SQLNS::JobInfo> Presenter::searchJobs(const QVector<SQLNS::JobInfo>& source,
                                             const QString& query) {
    QVector<SQLNS::JobInfo> ret;
    if (query.isEmpty()) return source;

    // 若 query 能解析为整数，则用于匹配 tagId
    bool ok = false;
    int qid = query.toInt(&ok);

    for (const auto &job : source) {
        // 名称或要求子串匹配（不区分大小写）
        if (job.jobName.contains(query, Qt::CaseInsensitive)) { ret.append(job); continue; }
        if (job.requirements.contains(query, Qt::CaseInsensitive)) { ret.append(job); continue; }

        // 若 query 是整数，检查是否为标签ID之一
        if (ok && job.tagIds.contains(qid)) { ret.append(job); continue; } // TODO 改为按照tag内容匹配
    }
    return ret;
}