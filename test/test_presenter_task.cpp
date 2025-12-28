// test/test_presenter_task.cpp
#include "test.h"
#include "tasks/presenter_task.h"
#include "presenter/presenter.h"
#include "db/sqlinterface.h"
#include <QDebug>

void test_presenter_task() {
    qDebug() << "[Test] PresenterTask 测试开始";


    // 测试1：分页查询
    int testPage = 2;
    int testPageSize = 10;
    TaskNS::PagingResult result = PresenterTask::queryJobsWithPaging(testPage, testPageSize);

    qDebug() << "===== 分页测试结果 =====";
    qDebug() << "总数据量：" << result.allData.size();
    qDebug() << "总页数：" << result.totalPage;
    qDebug() << "当前页：" << result.currentPage;
    qDebug() << "当前页数据量：" << result.pageData.size();

    if (!result.pageData.isEmpty()) {
        Presenter::printJobsLineByLine(result.pageData);
    }

    // 测试2：排序（按薪资降序）
    QVector<SQLNS::JobInfo> sortedJobs = PresenterTask::executeSortJobs(result.allData, false);
    if (!sortedJobs.isEmpty()) {
        qDebug() << "\n===== 薪资降序排序后第一条 =====";
        Presenter::printJobsLineByLine({sortedJobs.first()});
    }

    // 测试3：筛选城市ID=3的职位
    QVector<SQLNS::JobInfo> filteredJobs = PresenterTask::executeFilterJobs(result.allData, 3);
    qDebug() << "\n===== 城市ID=3的职位数量 =====" << filteredJobs.size();

    // 测试4：搜索示例
    qDebug() << "\n===== 搜索测试：单字符串查询示例 =====";
    QVector<SQLNS::JobInfo> searchRes1 = PresenterTask::executeSearchJobs(result.allData, "后端");
    qDebug() << "查询 '后端' 搜索结果数量：" << searchRes1.size();
    if (!searchRes1.isEmpty()) Presenter::printJobsLineByLine(searchRes1);

    QVector<SQLNS::JobInfo> searchRes2 = PresenterTask::executeSearchJobs(result.allData, "提供住宿");
    qDebug() << "查询 '提供住宿' 搜索结果数量：" << searchRes2.size();
    if (!searchRes2.isEmpty()) Presenter::printJobsLineByLine(searchRes2);

    QVector<SQLNS::JobInfo> searchRes3 = PresenterTask::executeSearchJobs(result.allData, "工程");
    qDebug() << "查询 '工程' 搜索结果数量：" << searchRes3.size();
    if (!searchRes3.isEmpty()) Presenter::printJobsLineByLine(searchRes3);

    qDebug() << "[Test] PresenterTask 测试结束";
}