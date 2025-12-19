#include "cookie_fetcher.h"
#include "webview2_checker.h"
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QtWebView/QtWebView>
#include <QDebug>

CookieFetcher::CookieFetcher(QObject *parent)
    : QObject(parent)
    , m_view(nullptr)
    , m_eventLoop(nullptr)
{
    // 初始化Qt WebView
    QtWebView::initialize();
    qDebug() << "[CookieFetcher] Qt WebView已初始化";
}

CookieFetcher::~CookieFetcher() {
    if (m_view) {
        delete m_view;
    }
}

void CookieFetcher::onCookieReceived(const QString& cookies) {
    qDebug() << "[CookieFetcher] ========== onCookieReceived被调用 ==========";
    qDebug() << "[CookieFetcher] Cookie内容:" << cookies;
    qDebug() << "[CookieFetcher] Cookie长度:" << cookies.length();
    
    m_cookies = cookies;
    
    if (m_eventLoop) {
        qDebug() << "[CookieFetcher] EventLoop存在, 运行中:" << m_eventLoop->isRunning();
        if (m_eventLoop->isRunning()) {
            qDebug() << "[CookieFetcher] 退出EventLoop";
            m_eventLoop->quit();
        }
    } else {
        qDebug() << "[CookieFetcher] ⚠️ EventLoop为空指针";
    }
    qDebug() << "[CookieFetcher] ===========================================\n";
}

QString CookieFetcher::fetchCookies(const QString& url, int waitTime) {
    qDebug() << "[CookieFetcher] ========================================";
    qDebug() << "[CookieFetcher] 开始Cookie提取流程";
    
    // 检查WebView2是否安装
    if (!WebView2Checker::isInstalled()) {
        qDebug() << "[CookieFetcher] ✗ Edge WebView2未安装！";
        qDebug() << WebView2Checker::getDetailInfo();
        return QString();
    }
    
    QString version = WebView2Checker::getVersion();
    qDebug() << "[CookieFetcher] ✓ Edge WebView2已安装, 版本:" << version;
    
    qDebug() << "[CookieFetcher] 目标URL:" << url;
    qDebug() << "[CookieFetcher] 等待时间:" << waitTime << "ms";
    qDebug() << "[说明] BOSS直聘使用动态生成的__zp_stoken__";
    qDebug() << "[说明] 需要等待setGatewayCookie()完成执行";
    qDebug() << "[CookieFetcher] ========================================\n";
    
    m_cookies.clear();
    
    // 创建QML引擎和视图
    m_view = new QQuickView();
    m_view->engine()->rootContext()->setContextProperty("targetUrl", url);
    m_view->engine()->rootContext()->setContextProperty("cookieFetcher", this);
    
    // 使用内联QML内容，包含JavaScript注入获取cookie
    QByteArray qmlContent = R"(
import QtQuick 2.15
import QtWebView 1.15

Rectangle {
    width: 1024
    height: 768
    
    Component.onCompleted: {
        console.log("[WebView] QML组件已创建");
    }
    
    WebView {
        id: webView
        anchors.fill: parent
        url: targetUrl
        
        property bool cookieExtracted: false
        
        Component.onCompleted: {
            console.log("[WebView] WebView组件已创建，目标URL:", targetUrl);
        }
        
        onLoadingChanged: function(loadRequest) {
            console.log("[WebView] ========== 加载状态变化 ==========");
            console.log("[WebView] URL:", loadRequest.url);
            console.log("[WebView] 状态码:", loadRequest.status);
            
            if (loadRequest.status === WebView.LoadStartedStatus) {
                console.log("[WebView] ▶ 开始加载页面...");
            } else if (loadRequest.status === WebView.LoadSucceededStatus) {
                console.log("[WebView] ✓ 页面加载成功！");
                console.log("[WebView] 准备在2秒后提取Cookie...");
                extractCookieTimer.start();
            } else if (loadRequest.status === WebView.LoadFailedStatus) {
                console.log("[WebView] ✗ 页面加载失败！");
                console.log("[WebView] 错误域:", loadRequest.errorDomain);
                console.log("[WebView] 错误码:", loadRequest.errorCode);
                console.log("[WebView] 错误描述:", loadRequest.errorString);
                // 即使失败也尝试提取cookie
                cookieFetcher.onCookieReceived("");
            } else if (loadRequest.status === WebView.LoadStoppedStatus) {
                console.log("[WebView] ■ 页面加载停止");
            }
            console.log("[WebView] ======================================\n");
        }
        
        Timer {
            id: extractCookieTimer
            interval: 5000  // 等待5秒让页面JS充分执行（包括setGatewayCookie）
            repeat: false
            onTriggered: {
                console.log("[WebView] ========== Timer触发，开始提取Cookie ==========");
                console.log("[WebView] 执行JavaScript检查cookie状态");
                
                // 执行JavaScript获取document.cookie并检查关键值
                var checkScript = `
                    (function() {
                        var cookies = document.cookie;
                        var result = {
                            fullCookie: cookies,
                            hasStoken: cookies.indexOf('__zp_stoken__') !== -1,
                            hasSseed: cookies.indexOf('__zp_sseed__') !== -1,
                            hasSname: cookies.indexOf('__zp_sname__') !== -1,
                            hasSts: cookies.indexOf('__zp_sts__') !== -1,
                            length: cookies.length
                        };
                        console.log('[JS检查] Cookie长度:', result.length);
                        console.log('[JS检查] 包含__zp_stoken__:', result.hasStoken);
                        console.log('[JS检查] 包含__zp_sseed__:', result.hasSseed);
                        console.log('[JS检查] 包含__zp_sname__:', result.hasSname);
                        console.log('[JS检查] 包含__zp_sts__:', result.hasSts);
                        return cookies;
                    })()
                `;
                
                webView.runJavaScript(
                    checkScript,
                    function(result) {
                        console.log("[WebView] ========== JavaScript回调 ==========");
                        console.log("[WebView] 返回类型:", typeof result);
                        console.log("[WebView] 返回值长度:", result ? result.length : 0);
                        
                        if (result && result.length > 0) {
                            console.log("[WebView] ✓ Cookie提取成功");
                            console.log("[WebView] Cookie前200字符:", result.substring(0, 200));
                            cookieFetcher.onCookieReceived(result);
                        } else {
                            console.log("[WebView] ⚠️ Cookie为空或未定义");
                            cookieFetcher.onCookieReceived("");
                        }
                        console.log("[WebView] ======================================\n");
                    }
                );
                
                console.log("[WebView] runJavaScript调用已发起（异步）");
            }
        }
    }
}
)";
    
    // 使用QQmlComponent加载内联QML
    QQmlComponent component(m_view->engine());
    component.setData(qmlContent, QUrl());
    
    if (component.isError()) {
        qDebug() << "[CookieFetcher] QML加载错误:";
        for (const auto& error : component.errors()) {
            qDebug() << "  -" << error.toString();
        }
        qDebug() << "[提示] Qt WebView可能未正确安装或系统缺少WebView2运行时";
        return QString();
    }
    
    QObject* rootObject = component.create();
    if (!rootObject) {
        qDebug() << "[CookieFetcher] 无法创建QML根对象";
        return QString();
    }
    
    m_view->setContent(QUrl(), &component, rootObject);
    
    // 显示窗口
    m_view->show();
    
    qDebug() << "[CookieFetcher] 窗口已显示，等待Cookie提取...";
    
    // 等待Cookie提取完成或超时
    QEventLoop loop;
    m_eventLoop = &loop;
    
    QTimer::singleShot(waitTime, &loop, &QEventLoop::quit);
    loop.exec();
    
    m_eventLoop = nullptr;
    
    qDebug() << "[CookieFetcher] ========== 结果诊断 ==========";
    qDebug() << "[CookieFetcher] m_cookies内容:" << m_cookies;
    qDebug() << "[CookieFetcher] m_cookies长度:" << m_cookies.length();
    qDebug() << "[CookieFetcher] m_cookies是否为空:" << m_cookies.isEmpty();
    
    if (m_cookies.isEmpty()) {
        qDebug() << "[CookieFetcher] ✗ 未能获取到Cookie";
        qDebug() << "[诊断] 可能原因:";
        qDebug() << "  1. Edge WebView2未安装或版本不兼容";
        qDebug() << "  2. 网页加载失败（检查上方WebView日志）";
        qDebug() << "  3. JavaScript执行失败或超时";
        qDebug() << "  4. QML信号连接失败（检查onCookieReceived是否被调用）";
        qDebug() << "  5. 网站需要登录才能设置Cookie";
        qDebug() << "[建议] 使用配置文件方案（已实现且稳定）";
    } else {
        qDebug() << "[CookieFetcher] ✓ 成功获取Cookie!";
        qDebug() << "[CookieFetcher] Cookie前200字符:" << m_cookies.left(200);
    }
    qDebug() << "[CookieFetcher] ==========================================\n";
    
    // 隐藏窗口
    if (m_view) {
        m_view->hide();
    }
    
    return m_cookies;
}
