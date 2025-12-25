#include <QTimer>
#include "webview2_browser_wrl.h"
#include <QDebug>
#include <QTimer>
#include <QCoreApplication>
#include <QThread>
#include <QStandardPaths>
#include <QDir>
#include <WebView2.h>
#include <wrl.h>
#include "wil/com.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

using namespace Microsoft::WRL;

void WebView2BrowserWRL::enableRequestCapture(bool enable) {
    m_captureRequests = enable;
}
#include "webview2_browser_wrl.h"
#include <QDebug>
#include <QTimer>
#include <QCoreApplication>
#include <QThread>
#include <QStandardPaths>
#include <QDir>
#include <WebView2.h>
#include <wrl.h>
#include "wil/com.h"

using namespace Microsoft::WRL;

WebView2BrowserWRL::WebView2BrowserWRL(QWidget* parent)
    : QWidget(parent) {}

WebView2BrowserWRL::~WebView2BrowserWRL() {}

void WebView2BrowserWRL::fetchCookies(const QString& url) {
    m_pendingUrl = url;
    // 创建环境
    CreateCoreWebView2EnvironmentWithOptions(
        nullptr, nullptr, nullptr,
        Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                return OnEnvironmentCompleted(result, env);
            }).Get());
}

HRESULT WebView2BrowserWRL::OnEnvironmentCompleted(HRESULT result, ICoreWebView2Environment* env) {
    if (!SUCCEEDED(result) || !env) {
        emit navigationFailed("WebView2环境创建失败");
        return S_OK;
    }
    env->CreateCoreWebView2Controller((HWND)this->winId(),
        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
            [this](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                return OnControllerCompleted(result, controller);
            }).Get());
    return S_OK;
}

HRESULT WebView2BrowserWRL::OnControllerCompleted(HRESULT result, ICoreWebView2Controller* controller) {
    if (!SUCCEEDED(result) || !controller) {
        emit navigationFailed("WebView2控制器创建失败");
        return S_OK;
    }
    m_controller = controller;
    m_controller->get_CoreWebView2(&m_webview);
    RECT rect = {0, 0, this->width(), this->height()};
    m_controller->put_Bounds(rect);
    m_controller->put_IsVisible(TRUE);

    // 预注入拦截脚本，确保在页面脚本运行前替换fetch/XHR
    if (m_captureRequests && m_webview) {
        const wchar_t* preInject = LR"(
            (function(){
                try{
                    function tryPost(msg){
                        try{ if(window.chrome && window.chrome.webview && window.chrome.webview.postMessage){ window.chrome.webview.postMessage(JSON.stringify(msg)); } }catch(e){}
                    }
                    const target = 'api-c.liepin.com/api/com.liepin.searchfront4c.pc-search-job';
                    const _fetch = window.fetch.bind(window);
                    window.fetch = function() {
                        return _fetch.apply(null, arguments).then(function(response){
                            try{
                                var url = (response && response.url) ? response.url : '';
                                if(url.indexOf(target) !== -1){
                                    response.clone().text().then(function(text){
                                        try{
                                            if(typeof text === 'string' && text.indexOf('jobCardList') !== -1){
                                                tryPost({type:'api_response', url:url, body:text});
                                            }
                                        }catch(e){}
                                    }).catch(function(){});
                                }
                            }catch(e){}
                            return response;
                        });
                    };
                    (function(open){
                        XMLHttpRequest.prototype.open = function(method, url){
                            this._reqUrl = url; return open.apply(this, arguments);
                        };
                    })(XMLHttpRequest.prototype.open);
                    (function(send){
                        XMLHttpRequest.prototype.send = function(){
                            var self = this;
                            this.addEventListener('readystatechange', function(){
                                try{
                                    if(self.readyState === 4 && self._reqUrl && self._reqUrl.indexOf('api-c.liepin.com/api/com.liepin.searchfront4c.pc-search-job') !== -1){
                                        try{
                                            var txt = self.responseText || '';
                                            if(typeof txt === 'string' && txt.indexOf('jobCardList') !== -1){
                                                tryPost({type:'api_response', url:self._reqUrl, body:txt});
                                            }
                                        }catch(e){}
                                    }
                                }catch(e){}
                            });
                            return send.apply(this, arguments);
                        };
                    })(XMLHttpRequest.prototype.send);
                }catch(e){}
            })();
        )";
        m_webview->AddScriptToExecuteOnDocumentCreated(preInject, nullptr);
    }

    // 官方推荐：精准注册zhipin.com下所有请求的拦截器
    if (m_captureRequests && m_webview) {
        // 只拦截zhipin.com下所有资源类型
        m_webview->AddWebResourceRequestedFilter(L"https://www.zhipin.com/*", COREWEBVIEW2_WEB_RESOURCE_CONTEXT_ALL);
        m_webview->add_WebResourceRequested(
            Callback<ICoreWebView2WebResourceRequestedEventHandler>(
                [this](ICoreWebView2* sender, ICoreWebView2WebResourceRequestedEventArgs* args) -> HRESULT {
                    return this->OnWebResourceRequested(sender, args);
                }).Get(), &m_resourceRequestedToken);
    }
    // 注册导航完成事件
    m_webview->add_NavigationCompleted(
        Callback<ICoreWebView2NavigationCompletedEventHandler>(
            [this](ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) -> HRESULT {
                return OnNavigationCompleted(sender, args);
            }).Get(), &m_navCompletedToken);
    // 注册WebMessageReceived用于接收注入脚本通过 window.chrome.webview.postMessage 发送的数据
    m_webview->add_WebMessageReceived(
        Callback<ICoreWebView2WebMessageReceivedEventHandler>(
            [this](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                return this->OnWebMessageReceived(sender, args);
            }).Get(), &m_webMessageToken);
    // 导航
    m_webview->Navigate(m_pendingUrl.toStdWString().c_str());
    return S_OK;
}

// 捕捉到页面请求时，提取url和cookie字段
HRESULT WebView2BrowserWRL::OnWebResourceRequested(ICoreWebView2* sender, ICoreWebView2WebResourceRequestedEventArgs* args) {
    if (!args) return S_OK;
    wil::com_ptr<ICoreWebView2WebResourceRequest> req;
    args->get_Request(&req);
    if (!req) return S_OK;

    // 获取请求URL
    wil::unique_cotaskmem_string uri;
    req->get_Uri(&uri);
    QString urlStr = uri ? QString::fromWCharArray(uri.get()) : QString();

    // 获取请求方法
    wil::unique_cotaskmem_string method;
    req->get_Method(&method);
    QString methodStr = method ? QString::fromWCharArray(method.get()) : QString();

    // 获取请求头
    wil::com_ptr<ICoreWebView2HttpRequestHeaders> headers;
    req->get_Headers(&headers);
    QString cookieStr, refererStr, uaStr;
    wil::unique_cotaskmem_string cookie, referer, ua;
    if (headers) {
        headers->GetHeader(L"Cookie", &cookie);
        headers->GetHeader(L"Referer", &referer);
        headers->GetHeader(L"User-Agent", &ua);
    }
    if (cookie) cookieStr = QString::fromWCharArray(cookie.get());
    if (referer) refererStr = QString::fromWCharArray(referer.get());
    if (ua) uaStr = QString::fromWCharArray(ua.get());

    // // ----------- 关键：补全请求头仿真 -------------
    // // 只对主域请求(zhipin.com, bosszhipin.com, static.zhipin.com)补全
    // if (urlStr.contains("zhipin.com")) {
    //     if (headers) {
    //         // Accept
    //         headers->SetHeader(L"Accept", L"text/html,application/xhtml+xml,application/xml;q=0.9,image/avif,image/webp,image/apng,*/*;q=0.8,application/signed-exchange;v=b3;q=0.7");
    //         // Accept-Language
    //         headers->SetHeader(L"Accept-Language", L"zh-CN,zh;q=0.9");
    //         // Priority
    //         headers->SetHeader(L"Priority", L"u=0, i");
    //         // sec-ch-ua
    //         headers->SetHeader(L"sec-ch-ua", L"\"Google Chrome\";v=\"143\", \"Chromium\";v=\"143\", \"Not A(Brand\";v=\"24\"");
    //         // sec-ch-ua-mobile
    //         headers->SetHeader(L"sec-ch-ua-mobile", L"?0");
    //         // sec-ch-ua-platform
    //         headers->SetHeader(L"sec-ch-ua-platform", L"\"Windows\"");
    //         // sec-fetch-dest
    //         headers->SetHeader(L"sec-fetch-dest", L"document");
    //         // sec-fetch-mode
    //         headers->SetHeader(L"sec-fetch-mode", L"navigate");
    //         // sec-fetch-site
    //         headers->SetHeader(L"sec-fetch-site", L"same-origin");
    //         // sec-fetch-user
    //         headers->SetHeader(L"sec-fetch-user", L"?1");
    //         // upgrade-insecure-requests
    //         headers->SetHeader(L"upgrade-insecure-requests", L"1");
    //         // referrer
    //         if (!refererStr.isEmpty()) {
    //             headers->SetHeader(L"Referer", reinterpret_cast<LPCWSTR>(refererStr.utf16()));
    //         } else {
    //             headers->SetHeader(L"Referer", L"https://www.zhipin.com/web/geek/jobs?city=101010100");
    //         }
    //         // credentials: include（WebView2自动处理cookie，headers中无需设置credentials字段）
    //     }
    // }
    // // ------------------------------------------

    // 输出更丰富的请求信息，便于调试和扩展
    QString info = QString("[请求捕捉] %1 %2\n  Referer: %3\n  UA: %4\n  Cookie: %5")
        .arg(methodStr, urlStr, refererStr, uaStr, cookieStr);
    qDebug() << info;

    // 仍然通过信号传递url和cookie，兼容旧逻辑
    emit requestIntercepted(urlStr, cookieStr);
    return S_OK;
}

HRESULT WebView2BrowserWRL::OnNavigationCompleted(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args) {
    BOOL isSuccess = FALSE;
    if (args) args->get_IsSuccess(&isSuccess);
    if (!isSuccess) {
        emit navigationFailed("页面加载失败");
        return S_OK;
    }
    // 注入JS补全window、navigator、screen、chrome等属性，模拟真实Chrome环境
    if (m_webview) {
        const wchar_t* patchJs = LR"(
            (function() {
                try {
                    // navigator.webdriver
                    Object.defineProperty(window.navigator, 'webdriver', {get: () => false, configurable: true});
                    // navigator.languages
                    Object.defineProperty(window.navigator, 'languages', {get: () => ['zh-CN', 'zh', 'en'], configurable: true});
                    // navigator.plugins
                    Object.defineProperty(window.navigator, 'plugins', {get: () => [1,2,3,4,5], configurable: true});
                    // navigator.platform
                    Object.defineProperty(window.navigator, 'platform', {get: () => 'Win32', configurable: true});
                    // navigator.userAgent
                    Object.defineProperty(window.navigator, 'userAgent', {get: () => 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/143.0.0.0 Safari/537.36', configurable: true});
                    // navigator.vendor
                    Object.defineProperty(window.navigator, 'vendor', {get: () => 'Google Inc.', configurable: true});
                    // navigator.hardwareConcurrency
                    Object.defineProperty(window.navigator, 'hardwareConcurrency', {get: () => 8, configurable: true});
                    // navigator.deviceMemory
                    Object.defineProperty(window.navigator, 'deviceMemory', {get: () => 8, configurable: true});
                    // navigator.connection
                    Object.defineProperty(window.navigator, 'connection', {get: () => ({rtt: 50, downlink: 10, effectiveType: '4g'}), configurable: true});
                    // window.chrome
                    if (!window.chrome) window.chrome = { runtime: {}, loadTimes: () => {}, csi: () => {} };
                    // window.devicePixelRatio
                    Object.defineProperty(window, 'devicePixelRatio', {get: () => 1.25, configurable: true});
                    // window.outerWidth/outerHeight
                    Object.defineProperty(window, 'outerWidth', {get: () => 1920, configurable: true});
                    Object.defineProperty(window, 'outerHeight', {get: () => 1080, configurable: true});
                    // window.innerWidth/innerHeight
                    Object.defineProperty(window, 'innerWidth', {get: () => 1920, configurable: true});
                    Object.defineProperty(window, 'innerHeight', {get: () => 1040, configurable: true});
                    // window.screen
                    if (!window.screen) window.screen = {};
                    Object.defineProperty(window.screen, 'width', {get: () => 1920, configurable: true});
                    Object.defineProperty(window.screen, 'height', {get: () => 1080, configurable: true});
                    Object.defineProperty(window.screen, 'availWidth', {get: () => 1920, configurable: true});
                    Object.defineProperty(window.screen, 'availHeight', {get: () => 1040, configurable: true});
                    Object.defineProperty(window.screen, 'colorDepth', {get: () => 24, configurable: true});
                    Object.defineProperty(window.screen, 'pixelDepth', {get: () => 24, configurable: true});
                    // window.navigator.doNotTrack
                    Object.defineProperty(window.navigator, 'doNotTrack', {get: () => null, configurable: true});
                } catch(e) {}
            })();
        )";
        m_webview->ExecuteScript(patchJs, nullptr);
    }
    // 注入fetch/XHR拦截脚本，在页面内捕获指定API响应并通过postMessage发回宿主
    if (m_webview) {
        const wchar_t* captureJs = LR"(
            (function(){
                try{
                    function tryPost(msg){
                        try{ if(window.chrome && window.chrome.webview && window.chrome.webview.postMessage){ window.chrome.webview.postMessage(JSON.stringify(msg)); } }catch(e){}
                    }
                    const target = 'api-c.liepin.com/api/com.liepin.searchfront4c.pc-search-job';
                    const _fetch = window.fetch.bind(window);
                    window.fetch = function() {
                        return _fetch.apply(null, arguments).then(function(response){
                            try{
                                var url = (response && response.url) ? response.url : '';
                                if(url.indexOf(target) !== -1){
                                    response.clone().text().then(function(text){ tryPost({type:'api_response', url:url, body:text}); }).catch(function(){});
                                }
                            }catch(e){}
                            return response;
                        });
                    };
                    // XMLHttpRequest 兼容
                    (function(open){
                        XMLHttpRequest.prototype.open = function(method, url){
                            this._reqUrl = url; return open.apply(this, arguments);
                        };
                    })(XMLHttpRequest.prototype.open);
                    (function(send){
                        XMLHttpRequest.prototype.send = function(){
                            var self = this;
                            this.addEventListener('readystatechange', function(){
                                try{
                                    if(self.readyState === 4 && self._reqUrl && self._reqUrl.indexOf('api-c.liepin.com/api/com.liepin.searchfront4c.pc-search-job') !== -1){
                                        tryPost({type:'api_response', url:self._reqUrl, body:self.responseText});
                                    }
                                }catch(e){}
                            });
                            return send.apply(this, arguments);
                        };
                    })(XMLHttpRequest.prototype.send);
                }catch(e){}
            })();
        )";
        m_webview->ExecuteScript(captureJs, nullptr);
    }
    // 模拟向下滚动页面，触发更多内容加载
    if (m_webview) {
        // 延迟1秒后滚动到底部
        QTimer::singleShot(1000, [webview = m_webview](){
            webview->ExecuteScript(L"window.scrollTo(0, document.body.scrollHeight);", nullptr);
        });
    }
    // 获取CookieManager
    wil::com_ptr<ICoreWebView2_2> webview2;
    sender->QueryInterface(IID_ICoreWebView2_2, reinterpret_cast<void**>(webview2.put()));
    if (!webview2) {
        emit navigationFailed("WebView2_2接口不可用");
        return S_OK;
    }
    wil::com_ptr<ICoreWebView2CookieManager> mgr;
    webview2->get_CookieManager(&mgr);
    if (!mgr) {
        emit navigationFailed("CookieManager获取失败");
        return S_OK;
    }
    // 获取所有cookie
    mgr->GetCookies(m_pendingUrl.toStdWString().c_str(),
        Callback<ICoreWebView2GetCookiesCompletedHandler>(
            [this](HRESULT errorCode, ICoreWebView2CookieList* cookieList) -> HRESULT {
                return OnGetCookiesCompleted(errorCode, cookieList);
            }).Get());
    return S_OK;
}

HRESULT WebView2BrowserWRL::OnGetCookiesCompleted(HRESULT errorCode, ICoreWebView2CookieList* cookieList) {
    if (!SUCCEEDED(errorCode) || !cookieList) {
        emit navigationFailed("Cookie获取失败");
        return S_OK;
    }
    UINT count = 0;
    cookieList->get_Count(&count);
    QStringList pairs;
    for (UINT i = 0; i < count; ++i) {
        wil::com_ptr<ICoreWebView2Cookie> c;
        if (SUCCEEDED(cookieList->GetValueAtIndex(i, c.put())) && c) {
            wil::unique_cotaskmem_string name, value;
            c->get_Name(&name);
            c->get_Value(&value);
            if (name && value) {
                pairs << QString::fromWCharArray(name.get()) + "=" + QString::fromWCharArray(value.get());
            }
        }
    }
    emit cookieFetched(pairs.join("; "));
    return S_OK;
}

HRESULT WebView2BrowserWRL::OnWebMessageReceived(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) {
    if (!args) return S_OK;
    wil::unique_cotaskmem_string msg;
    if (SUCCEEDED(args->get_WebMessageAsJson(&msg)) && msg) {
        QString jsonStr = QString::fromWCharArray(msg.get());
        // If message is a quoted JSON string, unquote it
        if (jsonStr.startsWith('"') && jsonStr.endsWith('"')) {
            jsonStr = jsonStr.mid(1, jsonStr.length()-2);
            jsonStr = jsonStr.replace("\\\"", "\"");
        }
        // Try parse into JSON object
        QJsonDocument jd = QJsonDocument::fromJson(jsonStr.toUtf8());
        if (jd.isObject()) {
            QJsonObject jo = jd.object();
            QString url;
            QString body;
            if (jo.contains("url")) url = jo.value("url").toString();
            if (jo.contains("body")) body = jo.value("body").toString();
            // If body appears escaped (contains JSON escape sequences), unescape common patterns
            if (!body.isEmpty()) {
                if (body.contains("\\\"") || body.contains("\\\\")) {
                    QString un = body;
                    un.replace("\\\\", "\\");
                    un.replace("\\\"", "\"");
                    un.replace("\\n", "\n");
                    un.replace("\\r", "\r");
                    un.replace("\\t", "\t");
                    body = un;
                }
            }
            // If after unescaping the body is itself JSON, normalize it
            if (!body.isEmpty()) {
                QJsonDocument inner = QJsonDocument::fromJson(body.toUtf8());
                if (!inner.isNull()) {
                    body = inner.toJson(QJsonDocument::Compact);
                }
            }
            emit responseCaptured(url, body);
        } else {
            // fallback: emit raw
            emit responseCaptured(QString(), jsonStr);
        }
    }
    return S_OK;
}
