// test/test_presenter_task.cpp
#include "test.h"
#include "tasks/presenter_task.h"
#include "presenter/presenter.h"
#include "db/sqlinterface.h"
#include <QDebug>

void test_presenter_task() {
    qDebug() << "[Test] PresenterTask 测试开始";


    // 测试1：分页查询（使用新统一入口，空查询+空过滤+不排序）
    int testPage = 2;
    int testPageSize = 10;
    QString emptyQuery;
    QMap<QString, QVector<QString>> emptyFilters;
    QString emptySort;
    TaskNS::PagingResult result = PresenterTask::queryJobsWithPaging(emptyQuery, emptyFilters, emptySort, true, testPage, testPageSize);

    qDebug() << "===== 分页测试结果 =====";
    qDebug() << "总数据量：" << result.allData.size();
    qDebug() << "总页数：" << result.totalPage;
    qDebug() << "当前页：" << result.currentPage;
    qDebug() << "当前页数据量：" << result.pageData.size();

    if (!result.pageData.isEmpty()) {
        Presenter::printJobsLineByLine(result.pageData);
    }

    // 测试2：排序（按平均薪资降序）通过 Task 单入口
    TaskNS::PagingResult sortedRes = PresenterTask::queryJobsWithPaging(emptyQuery, emptyFilters, "salaryMax", false, 1, INT_MAX);
    if (!sortedRes.pageData.isEmpty()) {
        qDebug() << "\n===== 最高薪资降序排序后第一条 =====";
        Presenter::printJobsLineByLine({sortedRes.pageData.first()});
    }

    // 测试3：按城市名称筛选（示例使用第一条记录的 cityName，如果存在）
    QString sampleCity = result.allData.isEmpty() ? QString() : result.allData.first().cityName;
    TaskNS::PagingResult cityFiltered = PresenterTask::queryJobsWithPaging(emptyQuery, { {"cityName", {sampleCity}} }, "", true, 1, INT_MAX);
    qDebug() << "\n===== 城市('" << sampleCity << "') 的职位数量 =====" << cityFiltered.allData.size();

    // 验证：Presenter::searchJobs 的映射过滤与 Task 行为一致
    QMap<QString, QVector<QString>> mapFilters;
    mapFilters.insert("cityName", {sampleCity});
    QVector<SQLNS::JobInfoPrint> filteredByMap = Presenter::searchJobs(result.allData, mapFilters);
    if (filteredByMap.size() == cityFiltered.allData.size()) qDebug() << "✓ 映射过滤与 Task 行为一致";

    // 测试：未知字段不应产生回退匹配（保证不会产生意外命中）
    QMap<QString, QVector<QString>> badFilter;
    badFilter.insert("unknown_field", {"no-such-value"});
    QVector<SQLNS::JobInfoPrint> badFiltered = Presenter::searchJobs(result.allData, badFilter);
    qDebug() << "\n===== 未知字段滤器应返回0条（或与预期一致） =====" << badFiltered.size();
    if (badFiltered.size() == 0) qDebug() << "✓ 未知字段不会回退匹配";
    // 测试4：搜索示例（单字符串查询）
    qDebug() << "\n===== 搜索测试：单字符串查询示例 =====";
    TaskNS::PagingResult qres1 = PresenterTask::queryJobsWithPaging("后端", emptyFilters, "", true, 1, INT_MAX);
    qDebug() << "查询 '后端' 搜索结果数量：" << qres1.allData.size();
    if (!qres1.allData.isEmpty()) Presenter::printJobsLineByLine(qres1.pageData);

    TaskNS::PagingResult qres2 = PresenterTask::queryJobsWithPaging("提供住宿", emptyFilters, "", true, 1, INT_MAX);
    qDebug() << "查询 '提供住宿' 搜索结果数量：" << qres2.allData.size();
    if (!qres2.allData.isEmpty()) Presenter::printJobsLineByLine(qres2.pageData);

    TaskNS::PagingResult qres3 = PresenterTask::queryJobsWithPaging("工程", emptyFilters, "", true, 1, INT_MAX);
    qDebug() << "查询 '工程' 搜索结果数量：" << qres3.allData.size();
    if (!qres3.allData.isEmpty()) Presenter::printJobsLineByLine(qres3.pageData);

    qDebug() << "[Test] PresenterTask 测试结束";
}