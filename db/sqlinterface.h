#ifndef SQLINTERFACE_H
#define SQLINTERFACE_H

#include <QString>
#include <QVector>
#include <QMap>

// SQL数据库模块命名空间
namespace SQLNS {

struct SalaryRange {
    int minSalary;
    int maxSalary;
};

struct JobInfo {
    int jobId;
    QString jobName;
    int companyId;
    int recruitTypeId;
    int cityId;
    QString requirements;
    double salaryMin;
    double salaryMax;
    int salarySlabId;
    QString createTime;
    QString updateTime;
    QString hrLastLoginTime;
    QVector<int> tagIds;
};

} // namespace SQLNS

class SQLInterface {
public:
    SQLInterface();
    ~SQLInterface();

    // SQLite connection: provide path to the .db file
    bool connectSqlite(const QString &dbFilePath);

    bool isConnected() const;
    void disconnect();

    // Schema creation (replaces old ensureDatabaseAndTable)
    bool createAllTables();

    // Company operations (companyId required - general ID)
    int insertCompany(int companyId, const QString &companyName);

    // City operations
    int insertCity(const QString &cityName);

    // Tag operations
    int insertTag(const QString &tagName);

    // Salary Slab operations
    bool insertSalarySlab(int slabId, int maxSalary);

    // Job operations
    int insertJob(const SQLNS::JobInfo &job);
    bool insertJobTagMapping(int jobId, int tagId);
    QVector<SQLNS::JobInfo> queryAllJobs();

    // Utility
    SQLNS::SalaryRange getSalarySlabRange(int slabId);

private:
    bool openSqliteConnection(const QString &dbFilePath);
};

#endif // SQLINTERFACE_H
