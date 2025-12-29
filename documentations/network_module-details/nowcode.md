# Add_nowcode_plan — 补充说明（非正式计划说明）

> 说明：此文档为捕获/解析补充说明与字段映射参考，非正式执行计划。仅用于后续文档与实现参考。

步骤：
1. `crawl_nowcode`（NowCoder / Nowcode）采用与其他来源一致的捕获策略：优先使用 WebView2（WRL）捕获 API 请求；必要时解析 HTML 详情页。

1.1 捕获思路
- 在 `crawl_nowcode` 中创建独立的 WebView2 实例并加载搜索/推荐入口页面，监听 `WebResourceRequested` 事件以截获目标 JSON API 响应并持久化。
- 使用 `AddWebResourceRequestedFilter` 限制域名（例如 `*.nowcoder.com` / `*.nowcode.cn`，以实际请求为准）和资源类型。

1.2 翻页与分页
- 观察被截获请求的参数，记录 `page` / `pageSize` 或其他游标（cursor、offset）参数，并用其驱动后续抓取。

1.3 期望响应（示例）
- 搜索 API 通常返回包含 `items` 或 `data.list` 的数组，并包含职位 `id`、`title`、`company`、`salary`、`area` 等字段以及分页信息（`total`、`hasMore` 等）。实现时以实际捕获到的 JSON 为准。

Parser 字段映射（请在实现前确认）
- `source`: "nowcode"
- `jobId` <- `item.id` 或 `item.jobId`
- `title` <- `item.title` 或 `item.jobName`
- `companyName` <- `item.company` / `item.companyName`
- `location` <- `item.location` / `item.area`
- `salary` <- `item.salary` 或 `item.salaryStr`
- `degree` <- `item.degree`（若有）
- `experience` <- `item.experience` 或 `item.workYear`
- `detailUrl` <- `item.url` / `item.jobUrl`
- `description` <- `item.description` 或需单独 fetch 详情页
- `raw` <- 完整 item JSON

注意事项
- 某些站点对接口做了防爬（签名、频率限制等），优先在 WebView2 中以真实浏览器环境捕获请求链。
- 若返回字段名混淆或嵌套深，先保持原始 JSON 记录，再逐步补充解析逻辑。
- 支持暂停/确认与逐页人工检查，以避免大规模错误解析。

实现 checklist
- 在 `network/` 下添加或更新 `crawl_nowcode.cpp/.h`，实现 WebView2 捕获逻辑并导出原始 payload。
- 在 `internet_task.cpp/.h` 中增加分支处理 `nowcode` 源。
- 在 `crawler_task` 中注册新来源并更新调度规则。

---

流程图（ASCII）：

    Start
      |
      v
    Create WebView2 Controller (nowcode)
      |
      v
    Add WebResourceRequested Filter (*.nowcoder.com / *.nowcode.*)
      |
      v
    On Request Intercepted? -- No --> Ignore
      |
     Yes
      v
    Save raw payload -> Emit `onRawCaptured`
      |
      v
    Confirm mapping? -- No --> Pause
      |
     Yes
      v
    Parse -> Emit `onJobParsed` -> Persist -> End
