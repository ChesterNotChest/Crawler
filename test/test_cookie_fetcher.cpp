#include "test.h"
#include "network/cookie_fetcher.h"
#include "network/webview2_checker.h"
#include <QCoreApplication>
#include <QDebug>

/**
 * @brief 测试使用QtWebView动态提取Cookie
 * 
 * 功能：加载BOSS直聘页面并通过JavaScript提取document.cookie
 * 
 * ⚠️ 平台限制：
 *   Qt WebView在Windows桌面平台依赖Qt WebEngine
 *   Qt WebEngine仅支持MSVC编译器，MinGW不可用
 *   因此此方案在MinGW环境下无法工作
 * 
 * ✅ 推荐方案：
 *   使用config.json配置文件管理Cookie（已实现）
 *   手动从浏览器获取Cookie并保存到config.json
 *   程序自动读取并使用
 */
void test_cookie_fetcher_webengine() {
    qDebug() << "\n========== QtWebView Cookie提取测试 ==========\n";
    
    // 首先检查WebView2
    qDebug() << WebView2Checker::getDetailInfo();
    qDebug();
    
    if (!WebView2Checker::isInstalled()) {
        qDebug() << "\n✗ 测试终止: Edge WebView2未安装";
        qDebug() << "请先安装WebView2运行时，或使用配置文件方案";
        qDebug() << "\n==========================================\n";
        return;
    }
    
    // 创建Cookie获取器
    CookieFetcher fetcher;
    
    // 目标URL
    QString url = "https://www.zhipin.com/web/geek/jobs?city=101010100";
    
    qDebug() << "[测试] 目标URL:" << url;
    qDebug() << "[说明] 将加载页面并通过JavaScript提取Cookie";
    qDebug() << "[说明] BOSS直聘使用动态token生成，需要较长等待时间\n";
    
    // 尝试提取Cookie（等待15秒，给setGatewayCookie()充足时间）
    QString cookies = fetcher.fetchCookies(url, 15000);
    
    if (!cookies.isEmpty()) {
        qDebug() << "\n✓ Cookie提取成功!";
        qDebug() << "Cookie长度:" << cookies.length();
        qDebug() << "Cookie前100字符:" << cookies.left(100);
        
        // 检查关键cookie
        if (cookies.contains("__zp_stoken__")) {
            qDebug() << "✓ 包含 __zp_stoken__";
        } else {
            qDebug() << "⚠ 未找到 __zp_stoken__";
        }
    } else {
        qDebug() << "\n✗ Cookie提取失败";
        qDebug() << "可能原因：";
        qDebug() << "  1. Edge WebView2未安装";
        qDebug() << "  2. 页面加载失败";
        qDebug() << "  3. JavaScript执行超时";
        qDebug() << "建议：使用配置文件方案（已实现）";
    }
    
    qDebug() << "\n==========================================\n";
}

