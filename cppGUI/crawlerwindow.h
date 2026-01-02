#ifndef CRAWLERWINDOW_H
#define CRAWLERWINDOW_H

#include <QMainWindow>
#include <QLineEdit>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QStandardItemModel>
#include <QMap>
#include <QVector>
#include <QMenu>
#include "constants/db_types.h"

class CrawlerWindow : public QMainWindow {
    Q_OBJECT

public:
    CrawlerWindow(QWidget *parent = nullptr);

private slots:
    void onSearchClicked(bool refresh = true);
    void onCrawlButtonClicked();
    void onPrevPage();
    void onNextPage();
    void onPageChanged(int page);
    void onHeaderClicked(int logicalIndex);
    void showJobDetail(const SQLNS::JobInfoPrint &job);
    void showFilterDialog(const QString &field, QPushButton *button);
    void onClearFilters();

signals:
    void returnToLauncher();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void fillTable(const QVector<SQLNS::JobInfoPrint> &jobs);
    void populateFilters(const QVector<SQLNS::JobInfoPrint> &jobs);
    QMap<QString, QVector<QString>> getFieldFilters();

    QLineEdit *searchLineEdit;
    QPushButton *searchButton;
    QPushButton *refreshButton;
    QPushButton *crawlButton;
    QTableWidget *table;
    QPushButton *salaryFilter;
    QPushButton *tagFilter;
    QPushButton *cityFilter;
    QPushButton *recruitTypeFilter;
    QPushButton *sourceFilter;
    QPushButton *clearFilterButton;
    QPushButton *prevButton;
    QSpinBox *pageSpin;
    QLabel *totalLabel;
    QPushButton *nextButton;
    QSpinBox *pageSizeSpin;

    int currentPage;
    int pageSize;
    QString sortField;
    bool asc;
    QVector<SQLNS::JobInfoPrint> allJobs;
    QMap<QString, QString> fieldMap;
    QMap<QString, QVector<QString>> currentFilters;
    QMap<QString, QSet<QString>> filterOptions;
};

#endif // CRAWLERWINDOW_H
