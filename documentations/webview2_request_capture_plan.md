# WebView2 Request Capture — 模块说明书

目的
- 描述 `WebView2BrowserWRL` 模块如何捕捉页面发出的 HTTP(S) 请求并提取请求头（特别是 `Cookie` 字段），以便上层通过信号/回调异步获取并处理这些信息。

适用场景
- 需要从嵌入式 WebView2 中收集真实浏览器发出的请求头（例如用于获取或验证 Cookie、分析 XHR 行为、辅助反爬处理时观察请求链）。

设计要点（概述）
- 使用 WebView2 的 `WebResourceRequested` 事件捕捉资源请求。
- 在事件回调里读取 `ICoreWebView2WebResourceRequest` 的 `Headers`，并从中提取 `Cookie`（以及可选的其它头部）。
- 通过 Qt 信号异步向上层发射请求信息，保持轻量且线程安全的边界。
- 提供开/关控制以避免持续捕获导致的性能问题（按需启用、按域过滤）。

公共接口（建议）
- Signal: `requestIntercepted(const QString &url, const QString &method, const QMap<QString,QString> &headers)`
	- 建议包含 `method` 与完整 `headers`（使用 `Cookie` 字段或其它头部时更灵活）。
- Method: `enableRequestCapture(bool enable)` — 注册/注销事件监听或启/停过滤器。
- Method: `setRequestFilter(const std::wstring &uri, COREWEBVIEW2_WEB_RESOURCE_CONTEXT resourceContext)` — 可选：设置 `AddWebResourceRequestedFilter` 的过滤器以减轻负载。

实现细节要点
- 事件注册：在 controller 创建完成后的回调（例如 CreateCoreWebView2Controller 完成 handler）中调用 `add_WebResourceRequested` 并保存返回的 token 以便后续移除。
- 读取 headers：从 `ICoreWebView2WebResourceRequest::get_Headers()` 获取 `ICoreWebView2HttpRequestHeaders`，使用其 `GetHeader` / `GetHeaders` 方法获取具体头部值。
- 区分 Cookie 来源：
	- `Cookie` 请求头：可在 `WebResourceRequested` 的 request.headers 中读取（包含 HttpOnly cookie）。
	- `Set-Cookie` 响应头：位于 response headers；若需要应在 response 回调中读取。
- 事件回调线程：WebView2 回调在 COM/主线程上下文中执行（通常是 UI 线程）。避免在回调中做重运算或阻塞操作，应将处理/持久化异步到工作线程。

性能与过滤建议
- 强烈建议调用 `AddWebResourceRequestedFilter` 以限制捕捉的 URL 模式（例如只监控目标域或 `https://*.zhipin.com/*`），避免捕捉所有图片、字体等静态资源。
- `enableRequestCapture(false)` 应当能释放事件 token 或移除过滤器，避免内存/回调泄漏。

示例信号与签名建议
- `requestIntercepted(const QString &url, const QString &method, const QMap<QString,QString> &headers)`
- `cookieCaptured(const QString &url, const QString &cookie)` — 便捷信号，派生自 `requestIntercepted`（仅在 headers 含 Cookie 时发射）。

WRL / WIL 伪代码示例（提取 Cookie）
```cpp
// 在 WebResourceRequested 事件回调中（WRL/wil 风格，伪代码）
wil::com_ptr<ICoreWebView2WebResourceRequestedEventArgs> args = ...;
wil::com_ptr<ICoreWebView2WebResourceRequest> request;
args->get_Request(&request);

wil::com_ptr<ICoreWebView2HttpRequestHeaders> headers;
request->get_Headers(&headers);

LPWSTR cookieValue = nullptr;
HRESULT hr = headers->GetHeader(L"Cookie", &cookieValue);
if (SUCCEEDED(hr) && cookieValue) {
		// 转换为 UTF-8 / QString 并发射信号
		QString url = qstring_from_wstring(requestUrl); // 获取 URL 可通过 request->get_Uri
		QString cookie = QString::fromWCharArray(cookieValue);
		emit requestIntercepted(url, QString::fromWCharArray(method), headersMap);
		CoTaskMemFree(cookieValue);
}
```

注意事项与边界条件
- HttpOnly cookie：在页面 JS 中不可见，但会随请求出现在 request headers，因此这是获取 HttpOnly cookie 的可靠方式。
- 并非所有请求都会包含 `Cookie`；合理去重、合并与时间戳记录可以提升后续分析价值。
- 某些情况下浏览器会对请求进行预检或分批发送（例如跨域），需要在测试中观察真实请求链以决定过滤与收集策略。

测试建议
- 在单元/集成测试中提供两种模式：
	- 导出全部请求 headers 的样本（用于断言是否存在 Cookie 字段）。
	- 仅捕获含 Cookie 的请求并验证 Cookie 内容与期望值匹配。
- 建议对收集到的 Cookie 做去重并记录时间戳以便调试和回放。

安全与合规提醒
- 捕获并持久化 Cookie 会涉及敏感数据（认证信息、会话 token 等）。请在设计中确保对这些数据的访问控制与安全存储（例如加密或限制写盘）。

文档变更记录
- v1.0 — 初始模块说明，包含公共接口建议、WRL 伪代码、过滤与测试建议。

如需，我可以把上面的伪代码扩展为一个可直接粘贴编译的 WRL/wil 示例，或添加 Qt 层（`WebView2BrowserWRL`）如何把 headers 转换为 `QMap` 并发射 `requestIntercepted` 信号的示例实现。

流程图（ASCII）

开始 -> 创建 Controller -> 注册 WebResourceRequested 事件 -> 接收 RequestEvent

以下为流程图（ASCII）：

			 +-------------------------+
			 |        Start            |
			 +-----------+-------------+
					   |
					   v
   +-----------------------------------------------+
   | CreateCoreWebView2Controller / Controller OK  |
   +----------------------+------------------------+
					 |
					 v
			+-----------------------------+
			| add_WebResourceRequested    |
			+-------------+---------------+
					    |
					    v
			+-----------------------------+
			| On WebResourceRequested     |
			| - get_Request()             |
			| - get_Headers()             |
			+-------------+---------------+
					    |
					    v
			+-----------------------------+
			| Extract headers (Cookie?)   |
			+-------------+---------------+
				|                   |
				| Yes               | No
				v                   v
	+---------------------------+   +----------------------+
	| emit cookieCaptured(...)  |   | optional store/meta  |
	+---------------------------+   +----------------------+
				|
				v
			+----------------+
			|   end / return  |
			+----------------+

