#include "mainwindow.h"
#include "test/test.h"

#include <QApplication>
#include <QDebug>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // 运行测试
    qDebug() << "\n========== Running SQL database tests ==========";
    test_sql_database();

    qDebug() << "\n========== Running job crawler tests ==========";
    test_job_crawler();

    // 启动GUI应用
    qDebug() << "\n========== Launching GUI ==========";
    MainWindow w;
    w.show();
    return a.exec();
}
