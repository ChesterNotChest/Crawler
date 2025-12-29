# Add_liepin_plan — 补充说明（非正式文档）

> 说明：此文档为抓取 / 解析的补充参考，非正式执行计划，仅用于后续实现与文档整理。

## 捕获方式
- 推荐在 `crawl_liepin` 中创建独立的 WebView2（WRL）实例，加载 Liepin 搜索页并拦截 API 请求（示例：`https://api-c.liepin.com/api/com.liepin.searchfront4c.pc-search-job`），保存完整请求与响应（包含 headers 与 Cookie）。
- 若 API 请求不可捕获或有额外依赖，再用 WebView2 做详情页面抓取或使用 HTTP 客户端复现请求并带上捕获到的 headers/cookies。

## 主要数据路径
- 主响应路径：`data.data.jobCardList[]`（每条包含 `job`、`comp`、`recruiter` 等子对象）。
- 分页信息见：`data.pagination.currentPage`, `data.pagination.hasNext`, `data.pagination.totalPage`。
- 详情链接通常在 `job.link`，可能需要单独抓取以获取完整职位描述。

## 字段映射（实现前请确认）
- `source`: "liepin"
- `jobId` <- `job.jobId`
- `title` <- `job.title`
- `companyName` <- `comp.compName`
- `location` <- `job.dq`
- `salary` <- `job.salary`
- `degree` <- `job.requireEduLevel`
- `experience` <- `job.requireWorkYears`
- `detailUrl` <- `job.link`
- `recruiter.name` <- `recruiter.recruiterName`
- `raw` <- 整条 `jobCardList` 项的原始 JSON

## 注意事项
- 支持在解析前人工确认映射并可暂停/继续批量解析。
- 对解析失败或 JSON 格式异常时记录原始 payload 并支持重试或标记为需人工检查。
- WebView2 实例应按来源独立管理，避免跨来源的 Cookie/会话污染。

## 实现清单
- 新增或更新 `crawl_liepin.cpp/.h` 实现 WebView2 捕获逻辑并导出原始 payload。
- 在 `internet_task.cpp/.h` 中添加 `liepin` 分支以调度该爬虫。
- 在 `crawler_task` 中注册来源并更新队列/调度逻辑。

---

流程图（ASCII）：

    Start
      |
      v
    启动 WebView2（liepin）
      |
      v
    拦截 API 请求 -> 保存 raw payload
      |
      v
    检查分页(hasNext?) -> 若有则继续抓取
      |
      v
    Emit `onRawCaptured` -> 解析 -> Emit `onJobParsed`
      |
      v
    Persist -> End
