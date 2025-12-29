# 方法集成模块（Tasks / Integration Module）

## 概述
- 本文档直接反映 `tasks/` 目录中实际实现，不引入未实现的持久化结构。该模块由若干 Task 类组成，负责爬取调度、来源适配、数据库写入、后端/AI 交互与展示层数据提供。

主要 Task 列表（实现文件）：
- `AITransferTask` — `tasks/ai_transfer_task.*`
- `BackendManagerTask` — `tasks/backend_manager_task.*`
- `CrawlerTask` — `tasks/crawler_task.*`
- `InternetTask` — `tasks/internet_task.*`
- `SqlTask` — `tasks/sql_task.*`
- `PresenterTask` — `tasks/presenter_task.*`

## 每个 Task 的职责与对外接口（简要）
- `AITransferTask`：
  - 作用：与 Python/AI 后端交换消息、触发知识库更新。
  - 主要接口：`sendChatMessage(message, callback)`, `updateKnowledgeBaseFromDatabase(parentWidget, callback)`。
  - 实现要点：通过 `SQLInterface::queryAllJobsPrint()` 读取数据，使用 `QNetworkAccessManager` 与后端通信；提供进度信号 `progressUpdated` 与完成信号 `transferCompleted(bool,msg)`。

- `BackendManagerTask`：
  - 作用：管理后端连接、心跳/重连与版本检查。
  - 主要接口：`connectToBackend(callback)`, `disconnectFromBackend()`, `checkBackendHealth()`, `checkUpdates(callback)`。

- `InternetTask`：
  - 作用：统一适配各来源爬虫实现并暴露抓取 API。
  - 主要接口：
    - `fetchJobData(pageNo, pageSize, recruitType)` — 主要用于 Nowcode（牛客）等按类型抓取。
    - `fetchBySource(sourceCode, pageNo, pageSize, ...)` — 按来源映射到对应爬虫，返回 `(vector<::JobInfo>, MappingData)`。
    - `fetchBySource(..., WebView2BrowserWRL* browser, ...)` — 为需要会话的来源（如 Wuyi）传入浏览器实例的重载。
    - `updateCookieBySource(sourceCode)` — 使用 WebView2/浏览器会话更新 cookie 并写入 `config.json`（代码中部分来源支持）。

- `CrawlerTask`：
  - 作用：爬取流程的顶层协调器，按来源与分页循环调度抓取并将结果交给 `SqlTask`。
  - 主要方法：`crawlAll(maxPagesPerSource, pageSize)`。
  - 实现要点：来源列表（默认顺序含 `nowcode`, `zhipin`, `chinahr`, `wuyi` 等），对 `wuyi` 使用长会话浏览器；遇到反爬码（如 37）会触发 cookie 更新并重试。

- `SqlTask`：
  - 作用：将爬虫层的 `::JobInfo` 转换为 SQL 层的 `SQLNS::JobInfo` 并持久化到数据库。
  - 主要方法：`storeJobData`, `storeJobDataBatch`, `storeJobDataWithSource`, `storeJobDataBatchWithSource`。
  - 实现要点：调用 `SQLInterface::insertCompany/insertCity/insertTag/insertJob/insertJobTagMapping`；负责类型转换、薪资档次计算与 idempotency（使用插入去重策略）。

- `PresenterTask`：
  - 作用：为 UI/展示层提供分页与检索接口。
  - 主要接口：`queryJobsWithPaging(page, pageSize, filters...)`，内部使用 `SQLInterface::queryAllJobs()` 并在需要时进行排序/筛选。



## 错误处理与重试策略（实现概要）
- `CrawlerTask` 与 `InternetTask`：面对网络失败或反爬码会执行有限重试，遇到特定反爬码（例如 37）会调用 `InternetTask::updateCookieBySource` 并再次尝试。
- 后端交互（AITransferTask / BackendManagerTask）：包含超时、重连与状态回调；对长时间任务提供进度更新与用户可取消的 UI 支持。

## 日志与监控
- 各 Task 在关键步骤使用 `qDebug()/qInfo()/qWarning()` 记录状态与错误，便于在 `logs/` 中定位问题。长耗时操作（AI 传输、批量写库）会有进度回调以便 UI 显示。

## 与代码文件的快速映射
- `tasks/ai_transfer_task.*` — AI 数据传输 / 知识库更新
- `tasks/backend_manager_task.*` — 后端连接管理与健康/更新检查
- `tasks/internet_task.*` — 多来源爬虫适配器（包装 `network/` 下各抓取实现）
- `tasks/crawler_task.*` — 抓取调度器（来源/分页/会话/反爬重试）
- `tasks/sql_task.*` — 转换并持久化爬虫数据到 `db/` 接口
- `tasks/presenter_task.*` — 提供分页/检索/排序接口给 UI

## 内联流程图（实现级）

    Start
      |
      v
    CrawlerTask::crawlAll()
      |
      v
    for each source -> InternetTask::fetchBySource(source, page)
      |
      v
    if jobs returned -> SqlTask::storeJobDataBatchWithSource(jobs, sourceId)
      |
      v
    SQLInterface: insertCompany/insertCity/insertTag/insertJob/insertJobTagMapping
      |
      v
    PresenterTask / AITransferTask consume persisted results
      |
      v
     End

## 变更记录
- v3.0 — 反映 `tasks/` （2025-12-29）

