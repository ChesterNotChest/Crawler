#ifndef SQLINTERFACE_H
#define SQLINTERFACE_H

#include <QString>
#include <QVector>
#include <QMap>

// 数据结构定义
#include "constants/db_types.h"

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

    // Source operations
    int insertSource(const SQLNS::Source &source);
    SQLNS::Source querySourceById(int sourceId);
    SQLNS::Source querySourceByCode(const QString &sourceCode);
    QVector<SQLNS::Source> queryAllSources();
    QVector<SQLNS::Source> queryEnabledSources();

    // Company operations (companyId required - general ID)
    int insertCompany(int companyId, const QString &companyName);

    // City operations
    int insertCity(const QString &cityName);

    // Tag operations
    int insertTag(const QString &tagName);

    // Job operations
    int insertJob(const SQLNS::JobInfo &job);
    bool insertJobTagMapping(long long jobId, int tagId);
    QVector<SQLNS::JobInfo> queryAllJobs();

private:
    bool openSqliteConnection(const QString &dbFilePath);
};

#endif // SQLINTERFACE_H
