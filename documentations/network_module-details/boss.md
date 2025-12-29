# Add_boss_plan — 补充说明（非正式计划说明）

> 说明：此文档为捕获/解析补充说明与字段映射参考，非正式执行计划。仅用于后续文档与实现参考。

步骤：
1. `crawl_boss`（boss直聘）采用与其他来源相同的捕获策略：优先使用 WebView2（WRL）截取页面发出的 API 请求；必要时抓取详情页 HTML。

1.1 捕获思路
- 使用独立的 WebView2 实例（不复用）在 `crawl_boss` 中加载搜索入口页面（例如 boss 站点的职位搜索页），在 `WebResourceRequested` 中监听目标 API 请求并保存完整请求/响应（包含 headers、Cookie）。

1.2 翻页与触发
- 页面通常通过 XHR/Fetch 请求获得职位列表，翻页由页面触发下一页请求或使用 URL 参数（观察抓到的请求判断）。
- 记录分页控制字段（例如 `page`、`pageSize`、`hasNext` / `totalPage`）以驱动抓取。

1.3 期望响应（示例）
- 由于 boss 的 API 版本可能变化，请以捕获到的实际 JSON 为准。通常包含职位数组（每条记录含职位信息、公司信息、薪资、地区、发布时间等）和分页信息。

Parser 字段映射（请在实现前确认）
- `source`: "boss"
- `jobId` <- `item.jobId` 或 `item.id`
- `title` <- `item.jobName` 或 `item.title`
- `companyName` <- `item.companyName` 或 `item.orgName`
- `location` <- `item.city` / `item.workArea`
- `salary` <- `item.salary` 或 `item.salaryStr`
- `degree` <- `item.education`（若存在）
- `experience` <- `item.workingExp` 或 `item.workYear`
- `detailUrl` <- `item.jobUrl` / `item.detailLink`
- `description` <- 若返回短文本则可能需要再次 fetch 详情页获取完整描述
- `tags` <- `item.tags` 或 `item.welfare`
- `raw` <- 整条 item 的原始 JSON

注意事项
- 支持用户在解析前暂停并确认字段映射。
- 当 API 返回压缩或嵌套字段（如 base64/HTML 编码描述）时，先保存原始 payload，再在解析器中做解码处理。
- 捕获并保存 `Set-Cookie` 与 `Cookie`，但对持久化的敏感数据需做访问限制或加密。
- 对网络或解析异常采用可配置的重试策略和指数退避。

实现 checklist
- 新增 `crawl_boss.cpp/.h`，内部使用独立 WebView2 实例捕获并导出原始 payload。
- 在 `internet_task.cpp/.h` 中增加对应分支以调度 `crawl_boss`。
- 在 `crawler_task` 中加入来源枚举与队列调度支持。
- 提供 `--capture-only` 模式用于仅保存原始 payload。

---

流程图（ASCII）：

    Start
      |
      v
    Create WebView2 Controller (boss)
      |
      v
    Add WebResourceRequested Filter (*.boss*.com / *.zhipin.com)
      |
      v
    On Request Intercepted? -- No --> Ignore
      |
     Yes
      v
    Save raw payload -> Emit `onRawCaptured`
      |
      v
    Confirm mapping? -- No --> Pause/Wait
      |
     Yes
      v
    Parse -> Emit `onJobParsed` -> Persist -> End
