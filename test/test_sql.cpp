#include "db/sqlinterface.h"
#include "tasks/sqltask.h"
#include <QDebug>
#include <QDateTime>

// Multi-table SQLite schema test using SQLTask
void test_sql_database()
{
    SQLInterface sql;
    const QString dbFile = "crawler.db";

    bool ok = sql.connectSqlite(dbFile);
    qDebug() << "SQLite connect status:" << (ok ? "connected" : "failed");
    if (ok) {
        // Create all tables (RecruitType enum auto-populated)
        if (!sql.createAllTables()) {
            qDebug() << "Failed to create tables.";
            sql.disconnect();
            return;
        }

        // Create SqlTask instance
        SqlTask task(&sql);

        // Insert companies (一般ID - 需要传入companyId)
        qDebug() << "\n=== Inserting Companies (General ID) ===";
        int companyId1 = task.insertCompany(101, "TechCorp");
        int companyId2 = task.insertCompany(102, "StartupXYZ");
        qDebug() << "Inserted companies with IDs:" << companyId1 << companyId2;

        // Insert cities (自增ID - 不传入cityId)
        qDebug() << "\n=== Inserting Cities (Auto-increment ID) ===";
        int cityIdBJ = task.insertCity("北京");
        int cityIdSH = task.insertCity("上海");
        int cityIdSZ = task.insertCity("深圳");
        qDebug() << "Inserted cities with IDs: BJ=" << cityIdBJ << "SH=" << cityIdSH << "SZ=" << cityIdSZ;

        // Insert tags (自增ID - 不传入tagId)
        qDebug() << "\n=== Inserting Tags (Auto-increment ID) ===";
        int tagId1 = task.insertTag("C++");
        int tagId2 = task.insertTag("Qt");
        int tagId3 = task.insertTag("Python");
        int tagId4 = task.insertTag("远程");
        qDebug() << "Inserted tags with IDs:" << tagId1 << tagId2 << tagId3 << tagId4;

        // Insert salary slabs (自定义ID - 需要传入salarySlabId)
        qDebug() << "\n=== Inserting Salary Slabs (Custom ID) ===";
        task.insertSalarySlab(1, 15000);
        task.insertSalarySlab(2, 25000);
        task.insertSalarySlab(3, 40000);
        qDebug() << "Inserted salary slabs (1: 15k, 2: 25k, 3: 40k)";

        // Insert jobs (一般ID - 需要传入jobId)
        qDebug() << "\n=== Inserting Jobs (General ID) ===";
        QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
        
        int jobId1 = task.insertJob(
            1001, "C++ 高级开发", companyId1, 1, cityIdBJ,
            "掌握 C++ 11/17，Qt框架，SQLite 数据库设计，有大型项目经验优先。",
            12, 20, 2, currentTime, currentTime, currentTime  // 单位: K
        );
        task.insertJobTagMapping(jobId1, tagId1); // C++
        task.insertJobTagMapping(jobId1, tagId2); // Qt
        qDebug() << "Inserted Job 1 (C++ 高级开发) with ID:" << jobId1;

        int jobId2 = task.insertJob(
            1002, "Python 实习生", companyId2, 2, cityIdSH,
            "掌握 Python 3.8+，有爬虫或数据分析经验；学习能力强。",
            8, 12, 1, currentTime, currentTime, currentTime  // 单位: K
        );
        task.insertJobTagMapping(jobId2, tagId3); // Python
        task.insertJobTagMapping(jobId2, tagId4); // 远程
        qDebug() << "Inserted Job 2 (Python 实习生) with ID:" << jobId2;

        // Query and display all jobs
        qDebug() << "\n=== Querying All Jobs ===";
        const auto jobs = task.queryAllJobs();
        for (const auto &job : jobs) {
            qDebug() << "\nJob ID:" << job.jobId
                     << "| Name:" << job.jobName
                     << "| CompanyID:" << job.companyId
                     << "| CityID:" << job.cityId
                     << "| Salary:" << job.salaryMin << "-" << job.salaryMax;
            qDebug() << "  Requirements:" << job.requirements.mid(0, 50) + "...";
            qDebug() << "  Tag IDs:" << job.tagIds;
            qDebug() << "  Created:" << job.createTime << "| Updated:" << job.updateTime;
        }

        sql.disconnect();
        qDebug() << "\n=== SQLite disconnected successfully ===";
    }
}
