#include "sqltask.h"

SQLTask::SQLTask(SQLInterface *sqlInterface)
    : m_sqlInterface(sqlInterface) {}

// === 一般ID部分 (需要传入ID) ===

int SQLTask::insertJob(int jobId, const QString &jobName, int companyId, int recruitTypeId,
                       int cityId, const QString &requirements, double salaryMin, double salaryMax,
                       int salarySlabId, const QString &createTime, const QString &updateTime,
                       const QString &hrLastLoginTime) {
    if (!m_sqlInterface) return -1;
    
    JobInfo job;
    job.jobId = jobId;  // 传入ID
    job.jobName = jobName;
    job.companyId = companyId;
    job.recruitTypeId = recruitTypeId;
    job.cityId = cityId;
    job.requirements = requirements;
    job.salaryMin = salaryMin;
    job.salaryMax = salaryMax;
    job.salarySlabId = salarySlabId;
    job.createTime = createTime;
    job.updateTime = updateTime;
    job.hrLastLoginTime = hrLastLoginTime;
    
    return m_sqlInterface->insertJob(job);
}

int SQLTask::insertCompany(int companyId, const QString &companyName) {
    if (!m_sqlInterface) return -1;
    
    // Company表需要手动指定companyId (一般ID)
    return m_sqlInterface->insertCompany(companyId, companyName);
}

// === 自增ID部分 (不传入ID，数据库自增) ===

int SQLTask::insertCity(const QString &cityName) {
    if (!m_sqlInterface) return -1;
    
    // cityId 自增，不传入ID
    return m_sqlInterface->insertCity(cityName);
}

int SQLTask::insertTag(const QString &tagName) {
    if (!m_sqlInterface) return -1;
    
    // tagId 自增，不传入ID
    return m_sqlInterface->insertTag(tagName);
}

// === 自定义ID部分 (需要传入ID) ===

bool SQLTask::insertSalarySlab(int salarySlabId, int maxSalary) {
    if (!m_sqlInterface) return false;
    
    // salarySlabId 自设计，传入ID
    return m_sqlInterface->insertSalarySlab(salarySlabId, maxSalary);
}

// === JobTagMapping ===

bool SQLTask::insertJobTagMapping(int jobId, int tagId) {
    if (!m_sqlInterface) return false;
    return m_sqlInterface->insertJobTagMapping(jobId, tagId);
}

// === Query operations ===

QVector<JobInfo> SQLTask::queryAllJobs() {
    if (!m_sqlInterface) return {};
    return m_sqlInterface->queryAllJobs();
}
