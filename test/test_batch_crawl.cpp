#include "test.h"
#include "tasks/crawler_task.h"
#include "db/sqlinterface.h"
#include <QDebug>

void test_batch_crawl() {
    qDebug() << "[Test] 开始 batch crawl 集成测试（轻量）";

    SQLInterface sql;
    // 使用磁盘数据库 crawler.db（按要求写入文件）
    if (!sql.connectSqlite("crawler.db")) {
        qDebug() << "[Test] 无法连接内存 SQLite，测试将中止";
        return;
    }
    if (!sql.createAllTables()) {
        qDebug() << "[Test] 无法创建数据库表，测试中止";
        return;
    }

    CrawlerTask task(&sql);
    // pageSize 使用默认(15)
    int stored = task.crawlAll(40, 0);

    qDebug() << "[Test] crawlAll 返回存储数量:" << stored;
    qDebug() << "[Test] batch crawl 集成测试 完成";
}
