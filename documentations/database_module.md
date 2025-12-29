# 数据库模块（Database Module）

## 概述
- 该文档严格反映当前仓库中 `db/sqlinterface.*` 与 `constants/db_types.h` 的实现。当前数据库表与接口以 SQLite 为目标实现，主要表在 `createAllTables()` 中创建。注意：仓库当前没有存储原始请求/响应（raw payloads）的表。

## 已实现的表（来自 `db/sqlinterface.cpp`）
- `Source`：数据来源表（`sourceId, sourceName, sourceCode, baseUrl, enabled, createdAt, updatedAt`）。
- `RecruitType`：招聘类型枚举表（初始化为：1=校招，2=实习，3=社招）。
- `Company`：公司信息（`companyId, companyName`）。
- `JobCity`：城市表（`cityId, cityName`）。
- `JobTag`：职位标签（`tagId, tagName`）。
- `Job`：职位主表（字段示例：`jobId, jobName, companyId, recruitTypeId, cityId, sourceId, requirements, salaryMin, salaryMax, salarySlabId, createTime, updateTime, hrLastLoginTime`）。
- `JobTagMapping`：职位与标签映射（复合主键 `(jobId, tagId)`）。

## 相关数据结构（`constants/db_types.h`）
- `SQLNS::Source`：与 `Source` 表对应的结构体。
- `SQLNS::JobInfo`：包含 `jobId, jobName, companyId, recruitTypeId, cityId, sourceId, requirements, salaryMin, salaryMax, salarySlabId, createTime, updateTime, hrLastLoginTime, tagIds`。
- `SQLNS::JobInfoPrint`：用于展示的结构，包含解析后的公司/城市/来源名称与 tag 名称列表。

## 对外接口（在 `db/sqlinterface.h` 可见）
- 连接与生命周期：`connectSqlite(dbFilePath)`, `isConnected()`, `disconnect()`。
- 表/模式管理：`createAllTables()`（负责建表与简单迁移，例如为 `Job` 添加 `sourceId`）。
- Source 操作：`insertSource`, `querySourceById`, `querySourceByCode`, `queryAllSources`, `queryEnabledSources`。
- Company/City/Tag 操作：`insertCompany`, `insertCity`, `insertTag`（均为 `INSERT OR IGNORE` 风格并返回 id）。
- Job 操作：`insertJob(const SQLNS::JobInfo &job)`（直接插入；返回 `jobId` 或 -1 表示失败），`insertJobTagMapping(jobId, tagId)`。
- 查询：`queryAllJobs()` 返回 `QVector<SQLNS::JobInfo>`，`queryAllJobsPrint()` 返回 `QVector<SQLNS::JobInfoPrint>`（带已解析名称与 tag 名称）。

## 设计要点与约定
- 幂等插入：Company/City/Tag 使用 `INSERT OR IGNORE`，并随后查询已存在 id，保证重复插入安全。
- 事务/迁移：`createAllTables()` 包含对表结构的检测与简单迁移（示例：检测 `Job` 表列并 `ALTER TABLE` 添加 `sourceId`）。
- 外键：`Job.sourceId` 与 `Source.sourceId` 逻辑上相关联，但实现主要依赖程序端保证引用一致性（SQLite 未强制外键约束的情形下，应在运行时保持正确性）。

## 关于原始 payload（raw）
- 当前仓库并未在 DB 层实现 `raw_payloads` 表或对应的接口；因此原始请求/响应若需持久化，推荐两种做法：
  1. 将 raw payload 写入文件系统（例如 `data/raw/{source}/{yyyymmdd}.json`），并在必要时在 DB 的 `Job` 或 `Job` 表中保留外部引用路径。
  2. 若希望集中管理并支持查询，可在后续迭代新增 `raw_payloads` 表并实现 `insertRaw/queryRaw`（当前不修改代码，需单独开发）；

## 运维与使用建议
- 启动时应先调用 `connectSqlite()` 并 `createAllTables()` 以确保 schema 就绪。
- 插入 Job 的顺序建议：确保 `Company` / `City` / `Source` 已存在或先通过相应接口创建，然后调用 `insertJob()`，最后写入 `JobTagMapping`。
- 若使用 SQLite 做并发写入，建议启用 WAL 模式并将写入操作排队以避免锁竞争。

## 内联流程图（与实现一致）

    Start
      |
      v
    connectSqlite() -> createAllTables()
      |
      v
    insertCompany / insertCity / insertTag (INSERT OR IGNORE)
      |
      v
    insertJob(job) -> insertJobTagMapping*
      |
      v
    queryAllJobs() / queryAllJobsPrint() -> UI / Export

## 参考文件
- `db/sqlinterface.h` / `db/sqlinterface.cpp`
- `constants/db_types.h`

## 变更记录
- v2.0 — 与 `db/sqlinterface` 实现对齐（2025-12-29）
