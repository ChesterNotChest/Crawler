// tasks/presenter_task.cpp
#include "presenter_task.h"
#include <QDebug>
#include <algorithm>
#include "presenter/presenter.h"
#include "db/sqlinterface.h" // 引入SQLInterface和queryAllJobs

QVector<SQLNS::JobInfoPrint> PresenterTask::cachedJobs;

// 单一入口：分页 + 搜索 + 字段映射筛选 + 排序
TaskNS::PagingResult PresenterTask::queryJobsWithPaging(const QString& query,
                                                        const QMap<QString, QVector<QString>>& fieldFilters,
                                                        const QString& sortField,
                                                        bool asc,
                                                        int page,
                                                        int pageSize,
                                                        bool refresh) {
    qDebug() << "queryJobsWithPaging called with query:" << query << "fieldFilters:" << fieldFilters << "sortField:" << sortField << "asc:" << asc << "page:" << page << "pageSize:" << pageSize;
    TaskNS::PagingResult result;
    result.currentPage = page;
    result.pageSize = pageSize;

    // 步骤1：连接数据库并获取解析后的全量数据
    QVector<SQLNS::JobInfoPrint> jobList1;
    if (refresh || cachedJobs.isEmpty()) {
        SQLInterface sqlInterface;
        if (sqlInterface.connectSqlite(Presenter::DEFAULT_DB_PATH)) {
            jobList1 = sqlInterface.queryAllJobsPrint();
            sqlInterface.disconnect();
            qDebug() << "Connected to DB, jobList1 size:" << jobList1.size();
        } else {
            jobList1 = Presenter::getAllJobs(0, INT_MAX);
            qDebug() << "Failed to connect DB, fallback jobList1 size:" << jobList1.size();
        }
        cachedJobs = jobList1;
    } else {
        jobList1 = cachedJobs;
        qDebug() << "Using cached data, jobList1 size:" << jobList1.size();
    }

    // 步骤2：单字符串搜索
    QVector<SQLNS::JobInfoPrint> jobList2 = Presenter::searchJobs(jobList1, query);
    qDebug() << "After search, jobList2 size:" << jobList2.size();

    // 步骤3：按字段映射筛选
    QVector<SQLNS::JobInfoPrint> jobList3 = Presenter::searchJobs(jobList2, fieldFilters);
    qDebug() << "After filter, jobList3 size:" << jobList3.size();

    // 步骤4：排序（若提供 sortField）
    QVector<SQLNS::JobInfoPrint> jobList4 = jobList3;
    if (!sortField.isEmpty()) {
        jobList4 = Presenter::sortJobs(jobList3, sortField, asc);
        qDebug() << "After sort, jobList4 size:" << jobList4.size();
    }

    // 步骤5：分页并返回
    result.allData = jobList4;
    result.totalPage = (result.allData.size() + pageSize - 1) / pageSize;
    result.pageData = Presenter::paging(result.allData, page, pageSize);
    qDebug() << "Final: allData size:" << result.allData.size() << "pageData size:" << result.pageData.size() << "totalPage:" << result.totalPage;

    return result;
}

