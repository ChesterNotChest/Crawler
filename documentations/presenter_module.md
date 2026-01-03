# Presenter 模块说明

概述
- Presenter 层负责把数据库查询到的职位数据（`SQLNS::JobInfoPrint`）整理成分页、排序、筛选和打印友好的形式。
- 它是 UI/任务层与数据库之间的“展示与转换”层，常用入口为 `Presenter::getAllJobs` 与 `PresenterTask::queryJobsWithPaging(...)`。

文件
- `presenter_main.cpp`：获取数据并提供带分页的 `getAllJobs` 实现（内部使用 `SQLInterface::queryAllJobsPrint()`）。
- `presenter_utils.cpp`：工具函数（时间格式化、分页、`toLine`、`printJobsLineByLine`）。
- `presenter_mask.cpp`：筛选/搜索实现（包含单字符串搜索与按字段映射搜索）。
- `presenter_sort.cpp`：通用排序实现（`sortJobs`）及向后兼容的 `sortJobsBySalary`。
- `presenter.h`：公共接口声明。

核心数据结构
- 使用 `SQLNS::JobInfoPrint`（定义在 `constants/db_types.h`）：包含基础字段（`jobId, jobName, companyId, cityId, salaryMin, salaryMax, ...`）以及解析后的可展示字段（`companyName, cityName, sourceName, tagNames`）。

主要 API 与语义
- Presenter::getAllJobs(), getAllJobs(page,pageSize)
  - 从 `SQLInterface::queryAllJobsPrint()` 获取解析后的列表并做分页。

- Presenter::paging(source, page, pageSize)
  - 分页函数，参数有效性保护（page>=1, pageSize>=1）。

- Presenter::toLine(const JobInfoPrint &job)
  - 将岗位格式化为单行，默认包含：`jobId | jobName | company:companyName | city:cityName | salaryMin - salaryMax | createTime`。
  - 用于日志/行式输出。

- Presenter::printJobsLineByLine(jobs)
  - 按行打印每个岗位（使用 `toLine`）。

- Presenter::searchJobs(source, const QString &query)
  - 单字符串搜索：
    - 如果 query 仅由数字组成（正则 `^\d+$`），**只按 `jobId` 精确匹配**（重要：数字不再用于匹配 `tagId`）。
    - 否则按不区分大小写的子串匹配：`jobName`, `requirements`, `companyName`, `cityName`, `sourceName`, 以及 `tagNames` 中的任一条目。

- Presenter::searchJobs(source, const QMap`<QString, QVector<QString>>`& fieldFilters)
  - 字段映射搜索：不同字段之间为 **AND**（必须满足所有字段），同一字段内多值为 **OR**（任一满足即可）。
  - 支持字段（示例）：`jobId`, `jobName`, `requirements`, `companyName`, `cityName`, `sourceName`, `tagNames`/`tags`, `tagIds`。
  - 文本字段使用不区分大小写的子串匹配；`jobId`/`tagIds` 使用数值精确匹配。
  - 对于**未知字段**，不做 fallback 匹配（避免意外命中）；会输出 `qDebug()` 记录提示。

- Presenter::sortJobs(source, field, asc)
  - 通用排序：支持字段示例：`jobId`/`id`、`jobName`（字符串按 QString 的字典序/编码序比较）、`salary`（平均薪资）、`salaryMin`、`salaryMax`。
  - 使用稳定排序（`std::stable_sort`）；字符串比较使用默认 `QString` 比较（code-point 顺序）。
  - `sortJobsBySalary` 作为向后兼容 wrapper（等价于 `sortJobs(..., "salary", asc)`）。

任务层整合
- PresenterTask::queryJobsWithPaging(query, fieldFilters, sortField, asc, page, pageSize)
  - 流程：
    1. 连接数据库，jobList1 = `SQLInterface::queryAllJobsPrint()`；
    2. jobList2 = `Presenter::searchJobs(jobList1, query)`；
    3. jobList3 = `Presenter::searchJobs(jobList2, fieldFilters)`；
    4. jobList4 = `Presenter::sortJobs(jobList3, sortField, asc)`；
    5. 返回 `TaskNS::PagingResult{ pageData, allData, totalPage, currentPage, pageSize }`。

调试与建议
- 排查搜索命中：
  - 请确认传入的字段名是否在支持列表内；未知字段现在不会回退匹配，且会在日志中记录警告信息。
- 行输出包含公司/城市信息，若需要同时输出 `tagNames` 以便调试，可以临时将 `Presenter::toLine` 修改为附加 `job.tagNames.join(",")`（示例）以便快速观察（注意：当前文档不修改代码）。

示例代码片段
```cpp
// 按城市名过滤并按平均薪资降序返回第一页
auto res = PresenterTask::queryJobsWithPaging("", {{"cityName", {"北京"}}}, "salary", false, 1, 20);
Presenter::printJobsLineByLine(res.pageData);

// 按 jobId 精确查询
auto res2 = PresenterTask::queryJobsWithPaging("123456", {}, "", true, 1, 10);
```

测试覆盖
- `test/test_presenter_task.cpp` 覆盖了分页、排序、按字段映射筛选、单字符串搜索及未知字段不回退匹配的场景。请以这些测试为参考扩展更多场景（例如 `tagIds`、组合字段 AND 场景等）。

兼容性说明
- 主查询现在以 `JobInfoPrint` 为主以便直接进行展示/模糊匹配（公司/城市/tag 名称均已解析）。
- 数字查询仅用于 `jobId` 的精确匹配；如需按 `tagId` 筛选，请使用映射搜索字段 `tagIds`。

后续改进建议
- 增加对 `companyName`、`createTime` 等字段的排序支持（createTime 按时间解析排序）。
- 提供 locale-aware 的字符串比较选项（例：基于用户 locale 的字典序）。
- 将字段映射接口暴露到 UI / API 层，以便前端直接构造 `QMap` 进行复杂筛选。

---
文档由代码自动梳理并基于 `presenter/` 源文件与测试生成（未修改任何源代码）。如需把本文档合并入 README 或进一步细化示例，告诉我想要强调的部分即可。