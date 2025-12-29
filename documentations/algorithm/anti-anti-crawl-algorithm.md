# WebView2 / WebView 反爬算法说明（基于 WRL 捕获逻辑与 network_module-details）

## 目的
- 将 `WebView2`（WRL/WIL）层捕获与观察能力，用作反爬（anti-bot / anti-scraping）检测与缓解的核心手段。本文档把捕获接口实现细节（如 `WebResourceRequested`）与从 `network_module-details` 中各来源策略总结结合，形成可执行的反爬检测与响应策略规范。

## 适用范围与威胁模型
- 适用于嵌入式 WebView2 发起真实浏览器请求的场景（例如 `wuyi` 需要长期会话、`zhipin` 需要 cookie 刷新）。
- 威胁模型：远端服务对请求行为做反爬判定（基于 cookie、UA、请求频率、交互痕迹、资源加载序列等），需检测异常会话并采取优化或回退策略（更新 cookie、放慢速率、切换代理或人工辅助）。

## 核心检测信号（可从 WRL 回调直接获得）
- Request-level headers：`Cookie`, `User-Agent`, `Referer`, 自定义 headers（来自 `ICoreWebView2WebResourceRequest::get_Headers()`）。
- Response-level headers：`Set-Cookie`, 状态码（401/403/429/其它业务反爬码，例如项目中出现的 code 37）。
- 请求序列特征：短时间内相同资源/接口的高频重复、并发 XHR 数量、资源类型比例（过多静态资源请求 vs API 请求）。
- 会话交互痕迹：是否存在页面可见交互（点击/滚动/JS 执行）与导航序列（长会话、多页翻页的真实点击更可信）。

## 反爬判定策略（从检测到决策）
1. 规则检测（轻量、实时）：
	 - 识别返回的特定反爬码（例如 code 37），或 `Set-Cookie` 指示的登录/验证码流程。
	 - 检测 Cookie 缺失或 Cookie 与期望值不一致。
	 - 统计短时间内请求频率/失败率达到阈值。

2. 指纹/行为异常检测（中等代价）：
	 - UA 与 header 模板异常（头部缺失或排序异常）。
	 - 请求时间序列不自然（无渲染等待、瞬时并发大量 API 请求）。
	 - 页面内 JS 未触发预期的资源（例如未加载关键脚本但直接调用 API）。

3. 综合评分与阈值：
	 - 把各检测项组合为风险分（weight-based），超过阈值触发缓解措施。

## 缓解与响应策略（按优先级）
- 优先级 1：轻量自愈
	- 对短时错误/反爬码触发 cookie 刷新：调用 `InternetTask::updateCookieBySource(source)` 并重试。（代码中 `CrawlerTask` 已有该逻辑用于 37）
	- 对高失败率降低并发与增加请求间隔（指数退避）。

- 优先级 2：会话/浏览器级改动
	- 为 `wuyi` 等需要完整会话的来源使用长会话 `WebView2BrowserWRL`，并在页面内模拟下一页点击或滑动（保持真实交互痕迹）。
	- 在必要时重新创建 Controller（新会话）并重现用户行为以刷新服务器端状态。

- 优先级 3：告警
	- 将任务上报给人工。

- 证据保存与回放
	- 在触发严重反爬時，将关键请求/响应（仅 metadata 或经脱敏的 cookie）保存用于离线分析。

## WRL 实现建议与细节要点
- 事件注册与生命周期
	- 在 `CreateCoreWebView2Controller` 完成后调用 `add_WebResourceRequested` 并记录 token，保证 `enableRequestCapture(false)` 时能移除监听，避免内存泄漏。

- 只捕获感兴趣资源
	- 使用 `AddWebResourceRequestedFilter` 限定要监控的域名与资源上下文（例如 `COREWEBVIEW2_WEB_RESOURCE_CONTEXT_XMLHTTPREQUEST`、`DOCUMENT`），避免捕获大量图像/字体。

- Header 读取
	- 使用 `ICoreWebView2WebResourceRequest::get_Headers()` 和 `ICoreWebView2HttpRequestHeaders::GetHeader` / `GetHeaders` 获取 `Cookie` 等。
	- 注意 `CoTaskMemFree` 与字符串编码（LPWSTR -> QString）。

- Response 观察
	- 通过 `WebResourceRequested` 的 event args 可获取请求相关信息；当前实现额外通过注入的 `fetch`/`XHR` 拦截脚本在页面内捕获目标 API 的响应并用 `window.chrome.webview.postMessage` 回传，宿主在 `WebView2BrowserWRL::OnWebMessageReceived` 中处理并发出 `responseCaptured` 信号。

- 线程与性能
	- WRL 回调在 COM/UI 线程执行，回调应尽量轻量：把解析/持久化放到工作线程并使用 Qt 信号回传。

- 安全考虑
	- 不要在默认情况下持久化完整 Cookie；若需持久化必须进行访问控制与加密（按合规策略）。

## 与 `network_module-details` 中来源的按源建议
- `wuyi`（需要长会话）：
	- 使用单独的长会话 `WebView2` 实例来维护登录状态；模拟页面内点击翻页以保持行为一致性；定期从 `WebResourceRequested` 捕获 `Cookie` 并对照有效性。

- `zhipin`（城市轮转/cookie 依赖）：
	- 在检测到反爬码或异常时，触发 `InternetTask::updateCookieBySource("zhipin")` 并重试；按城市轮转请求以分散流量特征。

- `nowcode`（按 recruitType 抓取）：
	- 以请求参数轮换（recruitType）并配合节流，检测是否有类型对应的反爬触发点。

- `liepin`/`chinahr`：
	- 优先以 request/response headers 与分页逻辑检测异常；这些来源通常可用较少会话模拟处理，但仍需观察请求频率与 header 完整性。

## 测试与验证策略
- 回归测试场景：
	- 模拟正常用户浏览（加载 JS、点击、翻页）并记录 baseline 请求序列。
	- 注入异常行为（高速并发、缺失 header、无渲染等待）并验证检测规则触发。

- 监控与报警：
	- 在生产环境收集风险分布（低/中/高），对高风险会话触发告警并自动保存脱敏证据用于离线分析。

## 配置与运行时参数
- 推荐放置于 `config.json` 的选项：
	- `requestCapture.enabled`：是否启用捕获。
	- `requestCapture.filters`：按来源域/资源类型的过滤表。
	- `antiBot.retryPolicy`：错误码/失败时的退避与重试策略。
	- `rawPayload.storage`：是否允许保存原始 payload（默认 false）。

## 示例公共方法（`WebView2BrowserWRL`）
- `void enableRequestCapture(bool enable)` — 启用/禁用请求捕获。
- `void fetchCookies(const QString &url)` — 异步抓取指定 URL 的 Cookie 并触发 `cookieFetched`。
- `void clickNext()` — 在页面内触发定义好的下一页点击逻辑。
- `signal void requestIntercepted(const QString &url, const QString &cookie)` — 捕获到请求时发出。
- `signal void responseCaptured(const QString &url, const QString &body)` — 注入脚本回传 公共方法 响应。
- `signal void cookieFetched(const QString &cookies)` — `GetCookies` 完成回调。
- `signal void navigationFailed(const QString &reason)` — 导航或环境创建失败时发出。

## 内联流程图（简化）

		Start
			|
			v
		CreateCoreWebView2Controller -> add_WebResourceRequested (with filters)
			|
			v
		On WebResourceRequested: read Request headers
			|
			v
		If Cookie/Anomaly detected -> compute risk score
			|                     |
			v                     v
		Low risk -> continue     High risk -> mitigation (cookie refresh / backoff / new session / alert)
			|
			v
		Persist evidence? -> End

## 变更记录
- v2.0 — 从模块说明演化为反爬算法规范，结合 WRL 捕获细节与 per-source 策略（2025-12-29）


