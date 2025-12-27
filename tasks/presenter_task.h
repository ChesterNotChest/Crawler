// tasks/presenter_task.h
#ifndef PRESENTER_TASK_H
#define PRESENTER_TASK_H

#include "presenter/presenter.h"
#include <QVector>

// 声明分页结果结构体（存在.h里）
namespace TaskNS {
struct PagingResult {
    QVector<SQLNS::JobInfo> pageData;   // 分页后数据
    QVector<SQLNS::JobInfo> allData;    // 原始全量数据
    int totalPage;                      // 总页数
    int currentPage;                    // 当前页
    int pageSize;                       // 每页条数
};
}

// Presenter层任务调度类
class PresenterTask {
public:
    // 基础方法：获取原始数据+分页（核心）
    static TaskNS::PagingResult queryJobsWithPaging(int page, int pageSize);
    
    // 原有方法保留
    static QVector<SQLNS::JobInfo> executeGetAllJobs(int page, int pageSize);
    static QVector<SQLNS::JobInfo> executeSortJobs(const QVector<SQLNS::JobInfo>& jobs, bool asc);
    static QVector<SQLNS::JobInfo> executeFilterJobs(const QVector<SQLNS::JobInfo>& jobs, int cityId);

    // 搜索接口（单字符串查询）
    static QVector<SQLNS::JobInfo> executeSearchJobs(const QVector<SQLNS::JobInfo>& jobs,
                                                     const QString& query);
};

#endif // PRESENTER_TASK_H