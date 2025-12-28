// tasks/presenter_task.cpp
#include "presenter_task.h"
#include <algorithm>
#include "presenter/presenter.h"
#include "db/sqlinterface.h" // 引入SQLInterface和queryAllJobs

// 核心方法：查询原始数据+分页（调用真实queryAllJobs）
TaskNS::PagingResult PresenterTask::queryJobsWithPaging(int page, int pageSize) {
    TaskNS::PagingResult result;
    result.currentPage = page;
    result.pageSize = pageSize;

    // 步骤1：调用真实的数据库查询接口（SQLInterface实例化）
    SQLInterface sqlInterface;
    if (sqlInterface.connectSqlite(Presenter::DEFAULT_DB_PATH)) { // 使用默认数据库路径
        result.allData = sqlInterface.queryAllJobs();    // 调用queryAllJobs获取全量数据
        sqlInterface.disconnect();
    } else {
        // 若直接打开失败，尝试通过 Presenter 层的 getAllJobs（已包含自身分页保护）获取数据
        result.allData = Presenter::getAllJobs(0, INT_MAX);
    }

    // 步骤2：计算总页数
    result.totalPage = (result.allData.size() + pageSize - 1) / pageSize;

    // 步骤3：调用Presenter的分页函数处理
    result.pageData = Presenter::paging(result.allData, page, pageSize);

    return result;
}

// 原有方法实现
QVector<SQLNS::JobInfo> PresenterTask::executeGetAllJobs(int page, int pageSize) {
    return Presenter::paging(Presenter::getAllJobs(), page, pageSize);
}

QVector<SQLNS::JobInfo> PresenterTask::executeSortJobs(const QVector<SQLNS::JobInfo>& jobs, bool asc) {
    return Presenter::sortJobsBySalary(jobs, asc);
}

QVector<SQLNS::JobInfo> PresenterTask::executeFilterJobs(const QVector<SQLNS::JobInfo>& jobs, int cityId) {
    return Presenter::filterJobsByCity(jobs, cityId);
}

QVector<SQLNS::JobInfo> PresenterTask::executeSearchJobs(const QVector<SQLNS::JobInfo>& jobs,
                                                         const QString& query) {
    return Presenter::searchJobs(jobs, query);
}


