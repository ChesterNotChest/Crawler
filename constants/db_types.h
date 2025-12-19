#ifndef DB_TYPES_H
#define DB_TYPES_H

#include <QString>
#include <QVector>

/**
 * @file db_types.h
 * @brief 数据库模块的数据结构定义
 * 
 * 包含数据库操作相关的所有数据结构，使用Qt类型
 */

// SQL数据库模块命名空间
namespace SQLNS {

// 数据来源结构体
struct Source {
    int sourceId;              // 数据源ID
    QString sourceName;        // 数据源名称（如"牛客网"、"BOSS直聘"）
    QString sourceCode;        // 数据源代码（如"nowcode"、"zhipin"）
    QString baseUrl;           // 基础URL
    bool enabled;              // 是否启用
    QString createdAt;         // 创建时间
    QString updatedAt;         // 更新时间
};

// 薪资范围结构体（保留用于未来扩展）
struct SalaryRange {
    int minSalary;
    int maxSalary;
};

// 数据库职位信息结构体
struct JobInfo {
    long long jobId;           // 职位ID（64位）
    QString jobName;           // 职位名称
    int companyId;             // 公司ID
    int recruitTypeId;         // 招聘类型ID (1=校招, 2=实习, 3=社招)
    int cityId;                // 城市ID
    int sourceId;              // 数据来源ID（外键关联Source表）
    QString requirements;      // 岗位要求
    double salaryMin;          // 最低薪资
    double salaryMax;          // 最高薪资
    int salarySlabId;          // 薪资档次ID (0-6)
    QString createTime;        // 创建时间
    QString updateTime;        // 更新时间
    QString hrLastLoginTime;   // HR最后登录时间
    QVector<int> tagIds;       // 标签ID列表
};

} // namespace SQLNS

#endif // DB_TYPES_H
