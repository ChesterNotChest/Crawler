#include "network/webview2_browser_wrl.h"
#include <QEventLoop>
#include <QDebug>
#include <QTimer>

void test_webview2_cookie_wrl() {

    qDebug() << "\n========== WebView2 WRL BOSS直聘 Cookie 提取测试 ==========";
    WebView2BrowserWRL browser;
    browser.enableRequestCapture(true);
    QString url1 = "https://www.zhipin.com/beijing/?seoRefer=index";
    QString url2 = "https://www.zhipin.com/web/geek/jobs?city=101010100";
    qDebug() << "[测试] 第一步访问首页:" << url1;

    QEventLoop loop;
    bool firstLoaded = false;

    // 合并信号连接，提升安全性和可读性
    QObject::connect(&browser, &WebView2BrowserWRL::requestIntercepted, [&](const QString& reqUrl, const QString& cookie) {
        if (!cookie.isEmpty()) {
            qDebug() << "[请求捕捉] url:" << reqUrl;
            qDebug() << "[请求捕捉] cookie:" << cookie;
        }
    });
    QObject::connect(&browser, &WebView2BrowserWRL::cookieFetched, [&](const QString& cookies) {
        if (!firstLoaded) {
            // 首页加载完成，延时后跳转到职位页
            firstLoaded = true;
            qDebug() << "[测试] 首页加载完成，等待2秒后跳转职位页:" << url2;
            QTimer::singleShot(2000, [&browser, url2]() {
                browser.fetchCookies(url2);
            });
            return;
        }
        // 职位页加载完成，提取cookie
        if (cookies.isEmpty()) {
            qDebug() << "✗ 未能获取到Cookie";
        } else {
            qDebug() << "✓ 成功获取Cookie";
            qDebug() << "Cookie长度:" << cookies.length();
            qDebug() << "Cookie" << cookies;
        }
        // 多停留3秒再退出，模拟用户浏览
        QTimer::singleShot(3000, [&loop]() {
            loop.quit();
        });
    });
    QObject::connect(&browser, &WebView2BrowserWRL::navigationFailed, [&](const QString& reason) {
        qDebug() << "✗ 导航或Cookie获取失败:" << reason;
        loop.quit();
    });
    browser.fetchCookies(url1);
    loop.exec();
    qDebug() << "==============================================\n";
}
