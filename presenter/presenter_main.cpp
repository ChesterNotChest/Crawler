// presenter/presenter_main.cpp
#include "presenter.h"
#include "tasks/sql_task.h"
#include "db/sqlinterface.h"

QVector<SQLNS::JobInfo> Presenter::getAllJobs() {
    return Presenter::getAllJobs(Presenter::DEFAULT_PAGE, Presenter::DEFAULT_PAGE_SIZE);  // 使用默认常量
}

// 带参数版本（自定义分页）
QVector<SQLNS::JobInfo> Presenter::getAllJobs(int page, int pageSize) {
    QVector<SQLNS::JobInfo> allJobs;

    // 连接数据库并查询
    SQLInterface sqlInterface;
    if (sqlInterface.connectSqlite(Presenter::DEFAULT_DB_PATH)) {
        allJobs = sqlInterface.queryAllJobs();
        sqlInterface.disconnect();
    }

    // 调用分页函数，使用传入的分页参数
    return Presenter::paging(allJobs, page, pageSize);
}
