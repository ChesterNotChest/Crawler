// presenter/presenter_sort.cpp
#include "presenter.h"

QVector<SQLNS::JobInfoPrint> Presenter::sortJobs(const QVector<SQLNS::JobInfoPrint>& jobs, const QString& field, bool asc) {
    QVector<SQLNS::JobInfoPrint> sortedJobs = jobs;
    QString key = field.trimmed().toLower();

    auto lessByField = [key](const SQLNS::JobInfoPrint& a, const SQLNS::JobInfoPrint& b) -> bool {
        if (key == "jobid" || key == "id" || key == "job_id") {
            return a.jobId < b.jobId;
        } else if (key == "jobname" || key == "job_name") {
            return a.jobName < b.jobName; // QString operator< uses code-point order
        } else if (key == "salarymin" || key == "salary_min") {
            return a.salaryMin < b.salaryMin;
        } else if (key == "salarymax" || key == "salary_max") {
            return a.salaryMax < b.salaryMax;
        }
        // fallback: by jobId to ensure deterministic order
        return a.jobId < b.jobId;
    };

    if (asc) {
        std::stable_sort(sortedJobs.begin(), sortedJobs.end(), lessByField);
    } else {
        std::stable_sort(sortedJobs.begin(), sortedJobs.end(), [&lessByField](const SQLNS::JobInfoPrint& a, const SQLNS::JobInfoPrint& b){ return lessByField(b, a); });
    }
    return sortedJobs;
}

QVector<SQLNS::JobInfoPrint> Presenter::sortJobsBySalary(const QVector<SQLNS::JobInfoPrint>& jobs, bool asc) {
    // 兼容旧调用，按平均薪资排序
    return Presenter::sortJobs(jobs, "salary", asc);
}