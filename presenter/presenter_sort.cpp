// presenter/presenter_sort.cpp
#include "presenter.h"

QVector<SQLNS::JobInfo> Presenter::sortJobsBySalary(const QVector<SQLNS::JobInfo>& jobs, bool asc) {
    QVector<SQLNS::JobInfo> sortedJobs = jobs;
    // 按薪资（取中间值）排序
    std::sort(sortedJobs.begin(), sortedJobs.end(), [asc](const SQLNS::JobInfo& a, const SQLNS::JobInfo& b) {
        double salA = (a.salaryMin + a.salaryMax) / 2;
        double salB = (b.salaryMin + b.salaryMax) / 2;
        return asc ? (salA < salB) : (salA > salB);
    });
    return sortedJobs;
}