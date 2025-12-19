# WebView2 捕捉页面请求与 Cookie 获取计划

## 目标
- 在 WebView2 加载页面时，捕捉所有 HTTP(S) 请求（包括 XHR、图片、API 等）。
- 能够获取每个请求的 Cookie（请求头中的 Cookie 字段）。
- 支持信号/回调方式异步返回请求信息。

## 技术要点
- 使用 ICoreWebView2::add_WebResourceRequested 事件，捕捉所有资源请求。
- 在事件回调中，读取 ICoreWebView2WebResourceRequest 对象的 Headers。
- 提取 Cookie 字段，并通过 Qt 信号发射。
- 支持外部注册回调/信号，便于业务层收集所有请求的 Cookie。

## 步骤
1. 在 WebView2BrowserWRL 增加信号：requestIntercepted(const QString& url, const QString& cookie)
2. 在 OnControllerCompleted 中注册 WebResourceRequested 事件。
3. 在事件回调中，提取请求 URL 和 Cookie 字段，发射信号。
4. 提供小方法：enableRequestCapture(bool enable) 控制是否捕捉。
5. 在测试用例中演示如何收集所有请求的 Cookie。

## 参考
- [WebView2 WebResourceRequested 事件官方文档](https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2#add_webresourcerequested)
- [ICoreWebView2WebResourceRequest 接口](https://learn.microsoft.com/en-us/microsoft-edge/webview2/reference/win32/icorewebview2webresourcerequest)
