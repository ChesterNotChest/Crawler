#ifndef WEBVIEW2_BROWSER_WRL_H
#define WEBVIEW2_BROWSER_WRL_H

#include <QWidget>
#include <QString>
#include <QObject>
#include <wrl.h>
#include "wil/com.h"
#include <WebView2.h>

class WebView2BrowserWRL : public QWidget {
    Q_OBJECT

public:
    explicit WebView2BrowserWRL(QWidget* parent = nullptr);
    ~WebView2BrowserWRL();

    // 启动导航并异步获取cookie
    void fetchCookies(const QString& url);

    // 启用/禁用请求捕捉
    void enableRequestCapture(bool enable);

signals:
    void cookieFetched(const QString& cookies);
    void navigationFailed(const QString& reason);
    // 捕捉到页面请求时发射（url, cookie字段）
    void requestIntercepted(const QString& url, const QString& cookie);

    // 当注入脚本捕获到API响应时发射（url, body）
    void responseCaptured(const QString& url, const QString& body);


private:
    // WebView2回调
    HRESULT OnEnvironmentCompleted(HRESULT result, ICoreWebView2Environment* env);
    HRESULT OnControllerCompleted(HRESULT result, ICoreWebView2Controller* controller);
    HRESULT OnNavigationCompleted(ICoreWebView2* sender, ICoreWebView2NavigationCompletedEventArgs* args);
    HRESULT OnGetCookiesCompleted(HRESULT errorCode, ICoreWebView2CookieList* cookieList);
    HRESULT OnWebResourceRequested(ICoreWebView2* sender, ICoreWebView2WebResourceRequestedEventArgs* args);
    HRESULT OnWebMessageReceived(ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args);

    wil::com_ptr<ICoreWebView2Controller> m_controller;
    wil::com_ptr<ICoreWebView2> m_webview;
    EventRegistrationToken m_navCompletedToken{};
    EventRegistrationToken m_resourceRequestedToken{};
    EventRegistrationToken m_webMessageToken{};
    QString m_pendingUrl;
    bool m_captureRequests = false;
};

#endif // WEBVIEW2_BROWSER_WRL_H
