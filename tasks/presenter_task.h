// tasks/presenter_task.h
#ifndef PRESENTER_TASK_H
#define PRESENTER_TASK_H

#include "presenter/presenter.h"
#include <QVector>

// 声明分页结果结构体（存在.h里）
namespace TaskNS {
struct PagingResult {
    QVector<SQLNS::JobInfoPrint> pageData;   // 分页后数据
    QVector<SQLNS::JobInfoPrint> allData;    // 原始全量数据
    int totalPage;                      // 总页数
    int currentPage;                    // 当前页
    int pageSize;                       // 每页条数
};
}

// Presenter层任务调度类
class PresenterTask {
public:
    // 单一入口：分页 + 搜索 + 字段映射筛选 + 排序
    // 参数：
    // - query: 单字符串查询（可为空）
    // - fieldFilters: QMap<字段名, QVector<匹配字符串>>（可为空）
    // - sortField: 排序字段名（可为空表示不排序）
    // - asc: 是否升序
    // - page: 当前页（从1开始）
    // - pageSize: 每页大小
    // - refresh: 是否重新从数据库读取数据
    static TaskNS::PagingResult queryJobsWithPaging(const QString& query,
                                                    const QMap<QString, QVector<QString>>& fieldFilters,
                                                    const QString& sortField,
                                                    bool asc,
                                                    int page,
                                                    int pageSize,
                                                    bool refresh = true);

private:
    static QVector<SQLNS::JobInfoPrint> cachedJobs;
};

#endif // PRESENTER_TASK_H