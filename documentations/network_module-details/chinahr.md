# Add_chinahr_plan — 补充说明（非正式文档）

> 说明：此文档为抓取 / 解析的补充参考，非正式执行计划，仅用于后续实现与文档整理。

## 捕获方式
- 首选通过 WebView2（WRL）创建独立浏览器实例来加载入口页面并在 `WebResourceRequested` 中截取目标请求；对于明确的 REST 接口（如搜索 POST），也可直接用 HTTP 客户端构造请求并保存响应。
- Chinahr 搜索 API 示例：POST https://www.chinahr.com/newchr/open/job/search，body 示例：{"page":1,"pageSize":20,"localId":"1","minSalary":null,"maxSalary":null,"keyWord":""}。
- 单条职位详情可通过 GET https://www.chinahr.com/detail/{jobId} 获取，详情页为 HTML，需解析相应选择器。

## 主要数据路径
- 搜索响应主路径：`data.jobItems[]`。
- 详情页面选择器：`.detail-block.detail-des .detail-des_lists`（职位描述），`.detail-block.detail-work .detail-work_address .workplace_name`（工作地点）。

## 字段映射（实现前请确认）
- `source`: "chinahr"
- `jobId` <- `jobItems[].jobId`
- `title` <- `jobItems[].jobName`
- `companyName` <- `jobItems[].comName`
- `location` <- `jobItems[].workPlace`（或从 detail HTML 解析）
- `salary` <- `jobItems[].salary`
- `degree` <- `jobItems[].degree`
- `experience` <- `jobItems[].workExp`
- `detailUrl` <- `https://www.chinahr.com/detail/{jobId}`
- `description` <- 解析后的 `.detail-des_lists` HTML
- `raw` <- 完整 `jobItems` JSON，解析时可附带抓取到的 detail HTML

## 注意事项
- 在解析 detail HTML 时规范化空白并保留换行，复杂编码（例如 HTML 转义或 base64）先保存原始 payload，再在解析阶段解码。
- 支持用户中途暂停并确认字段映射，避免大规模错误解析。
- 对网络或解析异常使用指数退避与有限重试；记录并保留原始 payload 以便离线修复。

## 实现清单
- 新增 `crawl_chinahr.cpp/.h`（或在现有模块中实现）以支持 POST 搜索与 detail 抓取/解析。
- 在 `internet_task.cpp/.h` 中添加 `chinahr` 分支以调度新爬虫。
- 在 `crawler_task` 中注册来源并更新调度逻辑。
- 提供 `--capture-only` 模式以仅保存原始请求/响应用于调试。

---

流程图（ASCII）：

    Start
      |
      v
    构建 POST 请求 / 启动 WebView2
      |
      v
    接收响应 -> 保存 raw payload
      |
      v
    是否需要 detail fetch? -- No --> 解析 raw -> Emit parsed
      |
     Yes
      v
    请求 detail 页面 -> 解析 HTML -> Emit parsed
      |
      v
    Persist -> End
