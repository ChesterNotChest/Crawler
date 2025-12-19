#ifndef COOKIE_FETCHER_H
#define COOKIE_FETCHER_H

#include <QObject>
#include <QString>
#include <QQuickView>
#include <QQmlEngine>
#include <QQmlContext>
#include <QEventLoop>
#include <QTimer>

/**
 * @brief 使用QtWebView加载页面并获取Cookie
 * 
 * 通过JavaScript注入获取document.cookie
 */
class CookieFetcher : public QObject {
    Q_OBJECT

public:
    explicit CookieFetcher(QObject *parent = nullptr);
    ~CookieFetcher();

    /**
     * @brief 加载URL并获取Cookie
     * @param url 目标URL
     * @param waitTime 等待时间（毫秒），让JS充分执行
     * @return Cookie字符串
     */
    QString fetchCookies(const QString& url, int waitTime = 5000);

private slots:
    void onCookieReceived(const QString& cookies);

private:
    QQuickView *m_view;
    QString m_cookies;
    QEventLoop *m_eventLoop;
};

#endif // COOKIE_FETCHER_H
