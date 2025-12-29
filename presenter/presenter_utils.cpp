// presenter/presenter_utils.cpp
#include "presenter.h"
#include <QDateTime>
#include <QVector>
#include <algorithm>

// 格式化时间
QString Presenter::formatTime(const QString& timeStr) {
    QDateTime dt = QDateTime::fromString(timeStr, "yyyy-MM-dd HH:mm:ss");
    return dt.isValid() ? dt.toString("yyyy年MM月dd日") : "无效时间";
}

// 分页函数
QVector<SQLNS::JobInfoPrint> Presenter::paging(const QVector<SQLNS::JobInfoPrint>& source, int page, int pageSize) {
    if (page < 1) page = 1;
    if (pageSize < 1) pageSize = 20;
    int start = (page - 1) * pageSize;
    int end = qMin(start + pageSize, (int)source.size());
    return start >= source.size() ? QVector<SQLNS::JobInfoPrint>() : source.mid(start, end - start);
}

// 本文件只保留工具类实现（时间格式化、分页）。
// 排序和筛选实现分别放在 `presenter_sort.cpp` 和 `presenter_mask.cpp` 中，避免重复定义。

// 将 JobInfoPrint 格式化为单行字符串，便于日志行输出
QString Presenter::toLine(const SQLNS::JobInfoPrint &job) {
    // 格式：jobId | jobName | companyName | cityName | salaryMin-salaryMax | createTime
    return QString("%1 | %2 | company:%3 | city:%4 | %5 - %6 | %7")
        .arg(job.jobId)
        .arg(job.jobName)
        .arg(job.companyName)
        .arg(job.cityName)
        .arg(job.salaryMin)
        .arg(job.salaryMax)
        .arg(formatTime(job.createTime));
}

// 按行打印岗位列表
void Presenter::printJobsLineByLine(const QVector<SQLNS::JobInfoPrint>& jobs) {
    for (int i = 0; i < jobs.size(); ++i) {
        qDebug().noquote() << QString("%1. %2").arg(i+1).arg(Presenter::toLine(jobs.at(i)));
    }
}
