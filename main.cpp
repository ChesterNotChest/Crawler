#include "cppGUI/launcherwindow.h"
#include "test/test.h"
#include "config/config_manager.h"
#include "maintenance/logger.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Initialize logger early so qDebug() goes to log file
    Maintenance::initializeLogger();

    // ========== 配置加载 ==========
    qDebug() << "\n========== CONFIG LOADING ==========" << "\n";
    if (ConfigManager::loadConfig("config.json")) {
        qDebug() << "✓ 配置文件加载成功\n";
    } else {
        qDebug() << "⚠️  配置文件加载失败，将使用默认值\n";
    }

    // ========== 单元测试 ==========
    // qDebug() << "\n========== UNIT TESTS ==========" << "\n";

    // qDebug() << "[1/2] InternetTask 单元测试 - 网络爬取功能";
    // test_internet_task_unit();

    // qDebug() << "\n[2/2] SqlTask 单元测试 - 数据转换和存储功能";
    // test_sql_task_unit();


    // ========== WebView2 WRL Cookie测试 ========== 
    // qDebug() << "\n========== WEBVIEW2 WRL COOKIE TEST =========\n";
    // test_webview2_cookie_wrl();

    // ========== 集成测试 ==========
    // qDebug() << "\n========== INTEGRATION TEST ==========\n";
    // qDebug() << "CrawlerTask 集成测试 - Crawler侧完整工作流";
    // test_batch_crawl();

    // PresenterTask 功能测试 - 数据查询与处理
    // qDebug() << "\n========== PRESENTER TASK TEST ==========\n";
    // qDebug() << "PresenterTask 功能测试 - 数据查询与处理";
    // test_presenter_task();

    // 启动GUI应用
    qDebug() << "\n========== Launching GUI ==========";
    LauncherWindow w;
    w.show();
    int rc = a.exec();

    // Shutdown logger cleanly
    Maintenance::shutdownLogger();
    return rc;
}
