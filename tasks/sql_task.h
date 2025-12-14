#ifndef SQL_TASK_H
#define SQL_TASK_H

#include <QString>
#include <QVector>
#include "db/sqlinterface.h"
#include "network/job_crawler.h"

/**
 * @brief SqlTask桥梁类
 * 连接Network模块(::JobInfo)和SQL模块(SQLNS::JobInfo)
 * 负责数据转换和业务逻辑处理
 */
class SqlTask {
public:
    SqlTask(SQLInterface *sqlInterface);

    // ========== 桥梁方法: Network → SQL ==========
    
    /**
     * @brief 存储爬虫Job数据（一站式方法）
     * 自动处理:
     * 1. 类型转换 (::JobInfo → SQLNS::JobInfo)
     * 2. 依赖数据存储 (Company, City, Tags)
     * 3. 主数据存储 (Job)
     * 4. 关联数据存储 (JobTagMapping)
     * 
     * @param crawledJob 爬虫获取的Job数据
     * @return 成功返回jobId，失败返回-1
     */
    int storeJobData(const ::JobInfo& crawledJob);
    
    /**
     * @brief 批量存储爬虫Job数据
     * @param crawledJobs 爬虫获取的Job列表
     * @return 成功存储的数量
     */
    int storeJobDataBatch(const std::vector<::JobInfo>& crawledJobs);

    // ========== 基础SQL操作方法 ==========
    
    // === 一般ID部分 (需要传入ID) ===
    
    // Job - jobId required
    int insertJob(int jobId, const QString &jobName, int companyId, int recruitTypeId, 
                  int cityId, const QString &requirements, double salaryMin, double salaryMax,
                  int salarySlabId, const QString &createTime, const QString &updateTime,
                  const QString &hrLastLoginTime);
    
    // Company - companyId required
    int insertCompany(int companyId, const QString &companyName);

    // === 自增ID部分 (不传入ID，数据库自增) ===
    
    // JobCity - cityId auto-increment
    int insertCity(const QString &cityName);
    
    // JobTag - tagId auto-increment
    int insertTag(const QString &tagName);

    // === 自定义ID部分 (需要传入ID) ===
    
    // SalarySlab - salarySlabId required (自设计ID)
    bool insertSalarySlab(int salarySlabId, int maxSalary);

    // === JobTagMapping ===
    bool insertJobTagMapping(int jobId, int tagId);

    // === Query operations ===
    QVector<SQLNS::JobInfo> queryAllJobs();

private:
    SQLInterface *m_sqlInterface;
    
    // ========== 内部转换方法 ==========
    
    /**
     * @brief 将爬虫JobInfo转换为SQL JobInfo
     */
    SQLNS::JobInfo convertJobInfo(const ::JobInfo& crawledJob);
    
    /**
     * @brief 计算薪资档次ID
     */
    int calculateSalarySlabId(double salaryMin, double salaryMax);
    
    /**
     * @brief std::string → QString 转换
     */
    static inline QString stdStringToQString(const std::string& str) {
        return QString::fromStdString(str);
    }
};

#endif // SQL_TASK_H
