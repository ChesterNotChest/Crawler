#include "mainwindow.h"
#include "test/test.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // ========== 单元测试 ==========
    qDebug() << "\n========== UNIT TESTS ==========\n";
    
    qDebug() << "[1/2] InternetTask 单元测试 - 网络爬取功能";
    test_internet_task_unit();
    
    qDebug() << "\n[2/2] SqlTask 单元测试 - 数据转换和存储功能";
    test_sql_task_unit();
    
    // ========== 集成测试 ==========
    qDebug() << "\n========== INTEGRATION TEST ==========\n";
    qDebug() << "CrawlerTask 集成测试 - Crawler侧完整工作流";
    test_crawler_task_integration();

    // 启动GUI应用
    qDebug() << "\n========== Launching GUI ==========";
    MainWindow w;
    w.show();
    return a.exec();
}
