// presenter/presenter.h
#ifndef PRESENTER_H
#define PRESENTER_H

#include <QVector>
#include <QString>
#include "db/sqlinterface.h"

// 声明外部的queryAllJobs（图片中提到的原始查询接口）
namespace SQLNS {
// 原始查询接口：返回所有职位（分页由上层处理）
QVector<JobInfo> queryAllJobs();
}

// 统一声明Presenter层的公共接口
class Presenter {
public:
    // 默认参数与配置
    static constexpr int DEFAULT_PAGE = 1;
    static constexpr int DEFAULT_PAGE_SIZE = 20;
    static constexpr const char* DEFAULT_DB_PATH = "crawler.db";

    // 无参数版本（默认分页）
    static QVector<SQLNS::JobInfo> getAllJobs();
    // 获取所有职位（调用原始queryAllJobs）
    static QVector<SQLNS::JobInfo> getAllJobs(int page, int pageSize);
    
    // 辅助方法：格式化时间
    static QString formatTime(const QString& timeStr);

    // 单行输出辅助（将 JobInfo 格式化为单行字符串）
    static QString toLine(const SQLNS::JobInfo &job);
    // 打印整页岗位（每个岗位一行）
    static void printJobsLineByLine(const QVector<SQLNS::JobInfo>& jobs);
    
    // 通用分页函数
    static QVector<SQLNS::JobInfo> paging(const QVector<SQLNS::JobInfo>& source, int page, int pageSize);
    
    // 排序业务：按薪资排序（asc：升序/降序）
    static QVector<SQLNS::JobInfo> sortJobsBySalary(const QVector<SQLNS::JobInfo>& jobs, bool asc = true);
    
    // 筛选业务：按城市ID筛选
    static QVector<SQLNS::JobInfo> filterJobsByCity(const QVector<SQLNS::JobInfo>& jobs, int cityId);

    // 搜索业务：使用单一查询字符串匹配（若为数字则尝试匹配标签ID）
    // 查询逻辑：如果 `query` 为空则返回原始 `source`；否则当任一条件成立时命中：
    // - `jobName` 包含 query（不区分大小写）
    // - `requirements` 包含 query（不区分大小写）
    // - 如果 query 为整数，则该整数存在于 job.tagIds 中
    static QVector<SQLNS::JobInfo> searchJobs(const QVector<SQLNS::JobInfo>& source,
                                              const QString& query);

};

#endif // PRESENTER_H
