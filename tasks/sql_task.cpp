#include "sql_task.h"
#include <QDebug>
#include <unordered_set>

SqlTask::SqlTask(SQLInterface *sqlInterface)
    : m_sqlInterface(sqlInterface) {}

// ========== 桥梁方法实现 ==========

int SqlTask::storeJobData(const ::JobInfo& crawledJob) {
    if (!m_sqlInterface) {
        qDebug() << "Error: SQLInterface is null";
        return -1;
    }
    
    // 1. 转换数据类型
    SQLNS::JobInfo sqlJob = convertJobInfo(crawledJob);
    
    // 2. 依赖数据：严格使用JSON中的ID，不生成占位名称
    // 注意：若需要公司/城市/标签名称，应在爬虫阶段解析并传入，当前仅依赖ID。
    
    // 2.x 可选：按名称插入公司/城市/标签（若提供名称）。
    // 公司：如果有company_id和company_name，按ID+名称插入/更新
    if (crawledJob.company_id > 0 && !crawledJob.company_name.empty()) {
        insertCompany(crawledJob.company_id, stdStringToQString(crawledJob.company_name));
    }

    // 标签：如果提供了名称，增量插入并建立映射
    QVector<int> insertedTagIds;
    std::unordered_set<std::string> seenTagNames;
    for (const auto& tagNameStr : crawledJob.tag_names) {
        if (tagNameStr.empty()) continue;
        if (seenTagNames.insert(tagNameStr).second) {
            int tagId = insertTag(stdStringToQString(tagNameStr));
            if (tagId > 0) {
                insertedTagIds.append(tagId);
            }
        }
    }

    // 2.y 城市：如果提供地区名称，按名称插入城市并使用返回的自增ID
    if (!crawledJob.area_name.empty()) {
        int cityId = insertCity(stdStringToQString(crawledJob.area_name));
        if (cityId > 0) {
            sqlJob.cityId = cityId;
        }
    }

    // 3. 存储主Job数据
    qDebug() << "[DEBUG] [SqlTask] Insert Job:"
             << "jobId=" << static_cast<qlonglong>(sqlJob.jobId)
             << ", jobName=" << sqlJob.jobName
             << ", companyId=" << sqlJob.companyId
             << ", recruitTypeId=" << sqlJob.recruitTypeId
             << ", cityId=" << sqlJob.cityId
             << ", salaryMin=" << sqlJob.salaryMin
             << ", salaryMax=" << sqlJob.salaryMax
             << ", slabId=" << sqlJob.salarySlabId
             << ", createTime=" << sqlJob.createTime
             << ", updateTime=" << sqlJob.updateTime;
    int insertRes = m_sqlInterface->insertJob(sqlJob);
    if (insertRes < 0) {
        qDebug() << "Failed to insert job:" << crawledJob.info_id << "; DB connected:" << m_sqlInterface->isConnected();
        // Try to (re)create tables in case schema missing, then retry once
        if (m_sqlInterface->isConnected()) {
            qDebug() << "Attempting to create tables and retry insert...";
            m_sqlInterface->createAllTables();
            int retry = m_sqlInterface->insertJob(sqlJob);
            if (retry >= 0) return retry;
            qDebug() << "Retry insert failed for job:" << crawledJob.info_id;
        }
        return -1;
    }
    long long jobId = sqlJob.jobId; // use full 64-bit id for subsequent mappings
    
    // 4. 建立JobTagMapping关联
    // 优先使用通过名称增量插入得到的tagId；若为空则回退使用原始tag_ids
    if (!insertedTagIds.isEmpty()) {
        for (int tagId : insertedTagIds) {
            insertJobTagMapping(jobId, tagId);
        }
    } else {
        for (int tagId : crawledJob.tag_ids) {
            insertJobTagMapping(jobId, tagId);
        }
    }
    
    return static_cast<int>(jobId);
}

int SqlTask::storeJobDataBatch(const std::vector<::JobInfo>& crawledJobs) {
    int successCount = 0;
    for (const auto& job : crawledJobs) {
        if (storeJobData(job) >= 0) {
            successCount++;
        }
    }
    return successCount;
}

int SqlTask::storeJobDataWithSource(const ::JobInfo& crawledJob, int sourceId) {
    if (!m_sqlInterface) {
        qDebug() << "Error: SQLInterface is null";
        return -1;
    }
    
    // 1. 转换数据类型
    SQLNS::JobInfo sqlJob = convertJobInfo(crawledJob);
    sqlJob.sourceId = sourceId; // 设置sourceId
    
    // 2. 依赖数据处理（与storeJobData相同）
    if (crawledJob.company_id > 0 && !crawledJob.company_name.empty()) {
        insertCompany(crawledJob.company_id, stdStringToQString(crawledJob.company_name));
    }

    QVector<int> insertedTagIds;
    std::unordered_set<std::string> seenTagNames;
    for (const auto& tagNameStr : crawledJob.tag_names) {
        if (tagNameStr.empty()) continue;
        if (seenTagNames.insert(tagNameStr).second) {
            int tagId = insertTag(stdStringToQString(tagNameStr));
            if (tagId > 0) {
                insertedTagIds.append(tagId);
            }
        }
    }

    if (!crawledJob.area_name.empty()) {
        int cityId = insertCity(stdStringToQString(crawledJob.area_name));
        if (cityId > 0) {
            sqlJob.cityId = cityId;
        }
    }

    // 3. 存储主Job数据
    qDebug() << "[DEBUG] [SqlTask] Insert Job with sourceId=" << sourceId
             << ", jobId=" << static_cast<qlonglong>(sqlJob.jobId)
             << ", jobName=" << sqlJob.jobName;
    int insertRes = m_sqlInterface->insertJob(sqlJob);
    if (insertRes < 0) {
        qDebug() << "Failed to insert job:" << crawledJob.info_id << "; DB connected:" << m_sqlInterface->isConnected();
        if (m_sqlInterface->isConnected()) {
            qDebug() << "Attempting to create tables and retry insert...";
            m_sqlInterface->createAllTables();
            int retry = m_sqlInterface->insertJob(sqlJob);
            if (retry >= 0) return retry;
            qDebug() << "Retry insert failed for job:" << crawledJob.info_id;
        }
        return -1;
    }
    long long jobId = sqlJob.jobId;
    
    // 4. 建立JobTagMapping关联
    if (!insertedTagIds.isEmpty()) {
        for (int tagId : insertedTagIds) {
            insertJobTagMapping(jobId, tagId);
        }
    } else {
        for (int tagId : crawledJob.tag_ids) {
            insertJobTagMapping(jobId, tagId);
        }
    }
    
    return static_cast<int>(jobId);
}

int SqlTask::storeJobDataBatchWithSource(const std::vector<::JobInfo>& crawledJobs, int sourceId) {
    int successCount = 0;
    for (const auto& job : crawledJobs) {
        if (storeJobDataWithSource(job, sourceId) >= 0) {
            successCount++;
        }
    }
    return successCount;
}

// ========== 内部转换方法实现 ==========

SQLNS::JobInfo SqlTask::convertJobInfo(const ::JobInfo& crawledJob) {
    SQLNS::JobInfo sqlJob;
    
    // 基本字段转换
    sqlJob.jobId = static_cast<long long>(crawledJob.info_id);
    sqlJob.jobName = stdStringToQString(crawledJob.info_name);
    sqlJob.companyId = crawledJob.company_id;
    sqlJob.recruitTypeId = crawledJob.type_id;
    sqlJob.cityId = crawledJob.area_id;
        sqlJob.sourceId = 0; // 默认值，需要在存储前设置
    sqlJob.requirements = stdStringToQString(crawledJob.requirements);
    
    // 薪资转换
    sqlJob.salaryMin = crawledJob.salary_min;
    sqlJob.salaryMax = crawledJob.salary_max;
    
    // 薪资档次ID计算（传入recruitType以区分实习的元/天单位）
    sqlJob.salarySlabId = calculateSalarySlabId(crawledJob.salary_min, crawledJob.salary_max, crawledJob.type_id);
    
    // 时间字段转换
    sqlJob.createTime = stdStringToQString(crawledJob.create_time);
    sqlJob.updateTime = stdStringToQString(crawledJob.update_time);
    sqlJob.hrLastLoginTime = stdStringToQString(crawledJob.hr_last_login);
    
    // 标签ID列表转换
    for (int tagId : crawledJob.tag_ids) {
        sqlJob.tagIds.append(tagId);
    }
    
    return sqlJob;
}

int SqlTask::calculateSalarySlabId(double salaryMin, double salaryMax, int recruitType) {
    int salary = static_cast<int>(max(salaryMin, salaryMax));
    
    // recruitType=2 为实习，薪资单位是 元/天（3位数）
    if (recruitType == 2) {
        // 实习薪资档次（元/天）
        if (salary == 0) return 0;         // 无薪资信息
        else if (salary <= 100) return 1;  // ≤100元/天
        else if (salary <= 200) return 2;  // 100-200元/天
        else if (salary <= 300) return 3;  // 200-300元/天
        else if (salary <= 400) return 4;  // 300-400元/天
        else if (salary <= 600) return 5;  // 400-600元/天
        else return 6;                     // >600元/天
    }
    
    // 校招/社招：薪资单位是 K/月
    if (salary <= 15) return 1;      // ≤15k
    else if (salary <= 25) return 2; // 15k-25k
    else if (salary <= 40) return 3; // 25k-40k
    else if (salary <= 60) return 4; // 40k-60k
    else if (salary <= 100) return 5;// 60k-100k
    else return 6;                    // >100k
}

// ========== 基础SQL操作方法 ==========

// === 一般ID部分 (需要传入ID) ===

int SqlTask::insertJob(int jobId, const QString &jobName, int companyId, int recruitTypeId,
                       int cityId, const QString &requirements, double salaryMin, double salaryMax,
                       int salarySlabId, const QString &createTime, const QString &updateTime,
                       const QString &hrLastLoginTime) {
    if (!m_sqlInterface) return -1;
    
    SQLNS::JobInfo job;
    job.jobId = jobId;  // 传入ID
    job.jobName = jobName;
    job.companyId = companyId;
    job.recruitTypeId = recruitTypeId;
    job.cityId = cityId;
    job.requirements = requirements;
    job.salaryMin = salaryMin;
    job.salaryMax = salaryMax;
    job.salarySlabId = salarySlabId;
    job.createTime = createTime;
    job.updateTime = updateTime;
    job.hrLastLoginTime = hrLastLoginTime;
    
    return m_sqlInterface->insertJob(job);
}

int SqlTask::insertCompany(int companyId, const QString &companyName) {
    if (!m_sqlInterface) return -1;
    
    // Company表需要手动指定companyId (一般ID)
    return m_sqlInterface->insertCompany(companyId, companyName);
}

// === 自增ID部分 (不传入ID，数据库自增) ===

int SqlTask::insertCity(const QString &cityName) {
    if (!m_sqlInterface) return -1;
    
    // cityId 自增，不传入ID
    return m_sqlInterface->insertCity(cityName);
}

int SqlTask::insertTag(const QString &tagName) {
    if (!m_sqlInterface) return -1;
    
    // tagId 自增，不传入ID
    return m_sqlInterface->insertTag(tagName);
}

// === JobTagMapping ===

bool SqlTask::insertJobTagMapping(long long jobId, int tagId) {
    if (!m_sqlInterface) return false;
    return m_sqlInterface->insertJobTagMapping(jobId, tagId);
}

// === Query operations ===

QVector<SQLNS::JobInfo> SqlTask::queryAllJobs() {
    if (!m_sqlInterface) return {};
    return m_sqlInterface->queryAllJobs();
}

QVector<SQLNS::JobInfoPrint> SqlTask::queryAllJobsPrint() {
    if (!m_sqlInterface) return {};
    return m_sqlInterface->queryAllJobsPrint();
}
