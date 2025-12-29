// presenter/presenter.h
#ifndef PRESENTER_H
#define PRESENTER_H

#include <QVector>
#include <QString>
#include <QMap>
#include "constants/db_types.h"

// Forward declare SQLInterface to avoid depending on db/sqlinterface.h being present
class SQLInterface;


// 声明外部的queryAllJobsPrint（返回解析后的展示结构）
namespace SQLNS {
// 返回用于展示/打印的职位信息（包含 companyName / cityName / tagNames）
QVector<JobInfoPrint> queryAllJobsPrint();
}

// 统一声明Presenter层的公共接口
class Presenter {
public:
    // 默认参数与配置
    static constexpr int DEFAULT_PAGE = 1;
    static constexpr int DEFAULT_PAGE_SIZE = 20;
    static constexpr const char* DEFAULT_DB_PATH = "crawler.db";

    // 无参数版本（默认分页）
    static QVector<SQLNS::JobInfoPrint> getAllJobs();
    // 获取所有职位（调用 queryAllJobsPrint）
    static QVector<SQLNS::JobInfoPrint> getAllJobs(int page, int pageSize);
    
    // 辅助方法：格式化时间
    static QString formatTime(const QString& timeStr);

    // 单行输出辅助（将 JobInfoPrint 格式化为单行字符串）
    static QString toLine(const SQLNS::JobInfoPrint &job);
    // 打印整页岗位（每个岗位一行）
    static void printJobsLineByLine(const QVector<SQLNS::JobInfoPrint>& jobs);
    
    // 通用分页函数
    static QVector<SQLNS::JobInfoPrint> paging(const QVector<SQLNS::JobInfoPrint>& source, int page, int pageSize);
    
    // 排序业务（通用）：按指定字段排序（字段名，asc=true为升序）
    // 支持字段示例："jobId"/"id", "jobName", "salary"(平均薪资), "salaryMin", "salaryMax"
    static QVector<SQLNS::JobInfoPrint> sortJobs(const QVector<SQLNS::JobInfoPrint>& jobs, const QString& field, bool asc = true);

    // 向后兼容：旧方法作为 wrapper（弃用），等价于 sortJobs(..., "salary", asc)
    static QVector<SQLNS::JobInfoPrint> sortJobsBySalary(const QVector<SQLNS::JobInfoPrint>& jobs, bool asc = true);
    
    // 筛选业务：按城市名称进行模糊匹配（保留基于名称的业务，ID只用于 jobId）
    static QVector<SQLNS::JobInfoPrint> filterJobsByCity(const QVector<SQLNS::JobInfoPrint>& jobs, const QString& cityQuery);

    // 搜索业务（单字符串）：使用单一查询字符串匹配（若为纯数字则仅用于匹配 jobId）
    // 查询逻辑：如果 `query` 为空则返回原始 `source`；否则当任一条件成立时命中：
    // - `jobName` 包含 query（不区分大小写）
    // - `requirements` 包含 query（不区分大小写）
    // - `companyName` 或 `cityName` 包含 query（不区分大小写）
    // - 任一 `tagNames` 条目包含 query（不区分大小写）
    // - 如果 `query` 是纯数字，则按 `jobId` 精确匹配
    static QVector<SQLNS::JobInfoPrint> searchJobs(const QVector<SQLNS::JobInfoPrint>& source,
                                                   const QString& query);

    // 搜索业务（按字段映射）：接收 QMap<字段名, QVector<匹配字符串>>，用于按列筛选
    // 语义：不同字段之间使用 AND（必须同时满足所有字段条件），同一字段内的多个值为 OR（任一匹配即可）
    // 支持字段: "jobName", "requirements", "companyName", "cityName", "sourceName", "tagNames", "jobId"
    // 对于文本字段，使用不区分大小写的子串匹配；对于 "jobId" 使用精确数值匹配。
    static QVector<SQLNS::JobInfoPrint> searchJobs(const QVector<SQLNS::JobInfoPrint>& source,
                                                   const QMap<QString, QVector<QString>>& fieldFilters);

};

#endif // PRESENTER_H
