# Add_wuyi_plan — 补充说明（非正式文档）

> 说明：此文档为抓取 / 解析的补充参考，非正式执行计划，仅用于后续实现与文档整理。

## 捕获方式
- 在 `crawl_wuyi` 中创建独立的 WebView2（WRL）实例并加载 51job 的移动站或 PC 搜索页（示例入口：`https://we.51job.com/pc/search`），监听并截取目标 API（示例：`https://we.51job.com/api/job/search-pc`）的请求/响应。
- 使用 `AddWebResourceRequestedFilter` 缩小捕获范围，避免捕获静态资源。

## 主要数据路径
- 主响应路径：`resultbody.job.items[]`（每条包含职位信息、公司、薪资、描述等）。
- 分页：根据返回的 `totalCount` 与请求中的 `pageSize` 计算；页面中也会有 `btn-next` 翻页交互。

## 字段映射（实现前请确认）
- `source`: "wuyi"
- `jobId` <- `items[].jobId`
- `title` <- `items[].jobName`
- `companyName` <- `items[].companyName` 或 `items[].fullCompanyName`
- `location` <- `items[].jobAreaString`
- `salary` <- `items[].provideSalaryString` 或 `jobSalaryMin`/`jobSalaryMax`
- `degree` <- `items[].degreeString`
- `experience` <- `items[].workYearString`
- `description` <- `items[].jobDescribe`（该字段可能包含换行与特殊标记，需清洗）
- `detailUrl` <- `items[].jobHref`
- `tags` <- `items[].jobTags`
- `raw` <- 完整 item JSON

## 注意事项
- 对 `jobDescribe` 字段进行清洗（去除多余 HTML、转义字符并保留必要换行）。
- 支持暂停/确认后再执行批量解析；对失败样例记录原始 payload 供人工检查。
- 若接口受限或带签名，优先通过 WebView2 捕获真实请求链。

## 实现清单
- 新增或更新 `crawl_wuyi.cpp/.h` 实现 WebView2 捕获并导出原始 payload。
- 在 `internet_task.cpp/.h` 中添加 `wuyi` 分支以调度该爬虫。
- 在 `crawler_task` 中注册来源并更新队列/调度逻辑。

---

流程图（ASCII）：

    Start
      |
      v
    启动 WebView2（51job）
      |
      v
    拦截 `search-pc` API 请求 -> 保存 raw payload
      |
      v
    计算分页 -> 循环抓取每页
      |
      v
    Emit `onRawCaptured` -> 解析 -> Emit `onJobParsed`
      |
      v
    Persist -> End
