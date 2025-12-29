# 网络爬取模块（Network Crawler Module）

## 概述
- 负责从目标招聘网站（如 Liepin、51job、Chinahr 等）抓取原始数据。优先捕获 API JSON 响应，必要时解析详情页 HTML。
- 采用 WebView2（WRL）进行真实浏览器层面的请求捕获与 Cookie 提取，部分请求可使用常规 HTTP 客户端发起。

## 目标与范围
- 捕获并保存请求/响应原始负载以便重放与调试。
- 向上层提供统一的“原始记录 -> 解析器”接口，支持暂停/确认、重试与回放模式。

## 架构要点
- 每个来源（`crawl_liepin`、`crawl_wuyi`、`crawl_chinahr`）使用独立 WebView2 实例（避免跨源状态污染）。
- 提供两类抓取方式：
  - 浏览器捕获（WebView2）：用于捕获 XHR、HttpOnly Cookie、复杂 JS 触发的请求链。
  - 直接请求（HTTP client）：用于清晰的 REST/POST 接口（如 Chinahr 的 search POST）。
- 抓取流程：加载入口页面 -> 拦截目标 API 请求 -> 保存原始 payload -> 上层确认/触发解析。

## 主要接口（建议）
- `startCapture(source, entryUrl, filters)` — 打开 WebView2 / 启动监听并加载页面。
- `stopCapture(source)` — 停止监听并销毁或保留 WebView2（配置决定）。
- `onCaptured(payload)` — 信号/回调，上传 `url`, `method`, `headers`, `body`, `response` 等。
- `fetchDirect(request)` — 用于直接发起 POST/GET（返回 raw response）。

## 捕获与过滤
- 使用 `AddWebResourceRequestedFilter` 限制域名与资源类型（避免捕获图片/字体等静态资源）。
- 对捕获结果按 URL、时间戳、Cookie 进行去重与归档。

## 错误处理与重试
- 对网络故障使用指数退避（exponential backoff）和有限次数重试。
- 对于解析失败，保存原始 payload 和错误日志以便离线修复。

## 安全与合规
- Cookie/会话信息为敏感数据，应当限制访问并在磁盘持久化时加密或使用受控日志目录。

## 测试建议
- 提供 `--capture-only` 模式只保存请求/响应、以及 `--replay` 模式用于在无 GUI 环境下重放请求并验证解析。

## 开发/运维注意
- 为每个来源记录样例 payload（`data/raw/{source}/{yyyymmdd}.json`）。
- 保持 WebView2 实例短生命周期（按任务创建/销毁）或实现明确的回收策略。

## 网络爬取模块 — ASCII 流程图

该文件为 `network_module.md` 的附加流程图，若需要我可以直接将该流程图合并进原始文件末尾。

流程图（ASCII）：

    Start
      |
      v
    Initialize Crawler (source)
      |
      v
    Create WebView2 or HTTP client
      |
      v
    Load entry page / Send request
      |
      v
    On WebResourceRequested / OnResponseReceived
      |
      v
    Filter target API URL?
      |         |
     No         Yes
      |           |
    Discard      Save raw payload
                   |
                   v
               Emit `onRawCaptured`
                   |
                   v
             Submit parse task -> Parser
                   |
                   v
             Parser emits `onJobParsed`
                   |
                   v
              Persist to DB -> Done


## 变更记录
- v1.0 — 初始说明（2025-12-29）。
