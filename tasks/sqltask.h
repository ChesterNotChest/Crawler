#ifndef SQLTASK_H
#define SQLTASK_H

#include <QString>
#include <QVector>
#include "db/sqlinterface.h"

class SQLTask {
public:
    SQLTask(SQLInterface *sqlInterface);

    // === 一般ID部分 (需要传入ID) ===
    
    // Job - jobId required
    int insertJob(int jobId, const QString &jobName, int companyId, int recruitTypeId, 
                  int cityId, const QString &requirements, double salaryMin, double salaryMax,
                  int salarySlabId, const QString &createTime, const QString &updateTime,
                  const QString &hrLastLoginTime);
    
    // Company - companyId required
    int insertCompany(int companyId, const QString &companyName);

    // === 自增ID部分 (不传入ID，数据库自增) ===
    
    // JobCity - cityId auto-increment
    int insertCity(const QString &cityName);
    
    // JobTag - tagId auto-increment
    int insertTag(const QString &tagName);

    // === 自定义ID部分 (需要传入ID) ===
    
    // SalarySlab - salarySlabId required (自设计ID)
    bool insertSalarySlab(int salarySlabId, int maxSalary);

    // === JobTagMapping ===
    bool insertJobTagMapping(int jobId, int tagId);

    // === Query operations ===
    QVector<JobInfo> queryAllJobs();

private:
    SQLInterface *m_sqlInterface;
};

#endif // SQLTASK_H
