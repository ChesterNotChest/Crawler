#include "crawlerwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QPushButton>
#include <QDialog>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QCloseEvent>
#include <QStandardItem>
#include <QHeaderView>
#include <QListWidget>
#include <QCheckBox>
#include "tasks/presenter_task.h"
#include "tasks/crawler_task.h"
#include "db/sqlinterface.h"
#include "presenter/presenter.h"
#include "constants/db_types.h"
#include "crawlprogresswindow.h"

CrawlerWindow::CrawlerWindow(QWidget *parent) : QMainWindow(parent), currentPage(1), pageSize(20), asc(true) {
    fieldMap["jobId"] = "jobId";
    fieldMap["工作名称"] = "jobName";
    fieldMap["招聘类型"] = "recruitTypeName";
    fieldMap["城市"] = "cityName";
    fieldMap["薪资"] = "salaryMin";
    fieldMap["来源"] = "sourceName";
    fieldMap["Tags"] = "tagNames";

    QWidget *central = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(central);

    // top
    QHBoxLayout *topLayout = new QHBoxLayout;
    searchLineEdit = new QLineEdit;
    searchButton = new QPushButton("搜索");
    refreshButton = new QPushButton("刷新");
    crawlButton = new QPushButton("爬取");
    topLayout->addWidget(searchLineEdit);
    topLayout->addWidget(searchButton);
    topLayout->addWidget(refreshButton);
    topLayout->addWidget(crawlButton);
    mainLayout->addLayout(topLayout);

    // filter
    QHBoxLayout *filterLayout = new QHBoxLayout;
    salaryFilter = new QPushButton("薪金筛选");
    connect(salaryFilter, &QPushButton::clicked, this, [this]() { showFilterDialog("salary", salaryFilter); });
    filterLayout->addWidget(salaryFilter);

    tagFilter = new QPushButton("Tag筛选");
    connect(tagFilter, &QPushButton::clicked, this, [this]() { showFilterDialog("tagNames", tagFilter); });
    filterLayout->addWidget(tagFilter);

    cityFilter = new QPushButton("城市筛选");
    connect(cityFilter, &QPushButton::clicked, this, [this]() { showFilterDialog("cityName", cityFilter); });
    filterLayout->addWidget(cityFilter);

    recruitTypeFilter = new QPushButton("招聘类型筛选");
    connect(recruitTypeFilter, &QPushButton::clicked, this, [this]() { showFilterDialog("recruitTypeName", recruitTypeFilter); });
    filterLayout->addWidget(recruitTypeFilter);

    sourceFilter = new QPushButton("来源筛选");
    connect(sourceFilter, &QPushButton::clicked, this, [this]() { showFilterDialog("sourceName", sourceFilter); });
    filterLayout->addWidget(sourceFilter);

    clearFilterButton = new QPushButton("清除筛选");
    connect(clearFilterButton, &QPushButton::clicked, this, &CrawlerWindow::onClearFilters);
    filterLayout->addWidget(clearFilterButton);

    mainLayout->addLayout(filterLayout);

    // table
    table = new QTableWidget;
    table->setColumnCount(8);
    table->setHorizontalHeaderLabels({"jobId", "工作名称", "招聘类型", "城市", "薪资", "来源", "Tags", "查看"});
    table->setSortingEnabled(false); // disable auto sort
    QHeaderView *header = table->horizontalHeader();
    connect(header, &QHeaderView::sectionClicked, this, &CrawlerWindow::onHeaderClicked);
    mainLayout->addWidget(table);

    // page
    QHBoxLayout *pageLayout = new QHBoxLayout;
    prevButton = new QPushButton("上一页");
    pageSpin = new QSpinBox;
    pageSpin->setMinimum(1);
    totalLabel = new QLabel("/ 1");
    nextButton = new QPushButton("下一页");
    pageLayout->addWidget(prevButton);
    pageLayout->addWidget(pageSpin);
    pageLayout->addWidget(totalLabel);
    pageLayout->addWidget(nextButton);
    mainLayout->addLayout(pageLayout);

    setCentralWidget(central);

    // connect
    connect(searchButton, &QPushButton::clicked, this, [this]() { onSearchClicked(true); });
    connect(refreshButton, &QPushButton::clicked, this, [this]() { onSearchClicked(true); });
    connect(crawlButton, &QPushButton::clicked, this, &CrawlerWindow::onCrawlButtonClicked);
    connect(prevButton, &QPushButton::clicked, this, [this]() { onPrevPage(); });
    connect(nextButton, &QPushButton::clicked, this, [this]() { onNextPage(); });
    connect(pageSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int page) { onPageChanged(page); });

    // Initial load
    onSearchClicked(true);
}

void CrawlerWindow::onCrawlButtonClicked() {
    QDialog dialog(this);
    dialog.setWindowTitle("爬取设置");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *labelPages = new QLabel("每个来源爬取的页数:");
    QSpinBox *spinPages = new QSpinBox();
    spinPages->setRange(1, 100);
    spinPages->setValue(10);
    layout->addWidget(labelPages);
    layout->addWidget(spinPages);
    QLabel *labelSources = new QLabel("需要爬取的来源:");
    QListWidget *listSources = new QListWidget();
    QStringList sources = {"liepin", "nowcode", "zhipin", "chinahr", "wuyi"};
    for (const QString &source : sources) {
        QListWidgetItem *item = new QListWidgetItem(source);
        item->setCheckState(Qt::Checked);
        listSources->addItem(item);
    }
    layout->addWidget(labelSources);
    layout->addWidget(listSources);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [this, spinPages, listSources]() {
        int pages = spinPages->value();
        std::vector<std::string> selectedSources;
        for (int i = 0; i < listSources->count(); ++i) {
            QListWidgetItem *item = listSources->item(i);
            if (item->checkState() == Qt::Checked) {
                selectedSources.push_back(item->text().toStdString());
            }
        }
        if (!selectedSources.empty()) {
            CrawlProgressWindow *progressWindow = new CrawlProgressWindow(selectedSources, pages, this);
            progressWindow->show();
        }
    });
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    dialog.exec();
}

void CrawlerWindow::onSearchClicked(bool refresh) {
    PresenterTask presenterTask;
    auto result = presenterTask.queryJobsWithPaging(searchLineEdit->text(), getFieldFilters(), sortField, asc, currentPage, pageSize, refresh);
    allJobs = result.allData;
    fillTable(result.pageData);
    populateFilters(allJobs);
    pageSpin->setMaximum(result.totalPage);
    pageSpin->setValue(result.currentPage);
    totalLabel->setText(QString("/ %1").arg(result.totalPage));
}

void CrawlerWindow::onPrevPage() {
    if (currentPage > 1) {
        currentPage--;
        onSearchClicked(false);
    }
}

void CrawlerWindow::onNextPage() {
    currentPage++;
    onSearchClicked(false);
}

void CrawlerWindow::onPageChanged(int page) {
    currentPage = page;
    onSearchClicked(false);
}

void CrawlerWindow::onHeaderClicked(int logicalIndex) {
    QString headerText = table->horizontalHeaderItem(logicalIndex)->text();
    QString newSortField = fieldMap.value(headerText, "");
    if (newSortField.isEmpty()) return;
    if (sortField == newSortField) {
        asc = !asc;
    } else {
        sortField = newSortField;
        asc = true;
    }
    onSearchClicked(false);
}

void CrawlerWindow::fillTable(const QVector<SQLNS::JobInfoPrint> &jobs) {
    table->setRowCount(jobs.size());
    for (int i = 0; i < jobs.size(); ++i) {
        const auto &job = jobs[i];
        table->setItem(i, 0, new QTableWidgetItem(QString::number(job.jobId)));
        table->setItem(i, 1, new QTableWidgetItem(job.jobName));
        table->setItem(i, 2, new QTableWidgetItem(job.recruitTypeName));
        table->setItem(i, 3, new QTableWidgetItem(job.cityName));
        QString salary = (job.salaryMin == 0) ? "面议" : QString("%1-%2").arg(job.salaryMin).arg(job.salaryMax);
        table->setItem(i, 4, new QTableWidgetItem(salary));
        table->setItem(i, 5, new QTableWidgetItem(job.sourceName));
        QString tags = job.tagNames.mid(0, 3).join(", ");
        table->setItem(i, 6, new QTableWidgetItem(tags));
        QPushButton *viewButton = new QPushButton("查看");
        connect(viewButton, &QPushButton::clicked, this, [this, job]() { showJobDetail(job); });
        table->setCellWidget(i, 7, viewButton);
    }
}

void CrawlerWindow::populateFilters(const QVector<SQLNS::JobInfoPrint> &jobs) {
    QSet<QString> salaries, tags, cities, recruitTypes, sources;
    for (const auto &job : jobs) {
        salaries.insert((job.salaryMin == 0) ? "面议" : QString("%1-%2").arg(job.salaryMin).arg(job.salaryMax));
        for (const auto &tag : job.tagNames) tags.insert(tag);
        cities.insert(job.cityName);
        recruitTypes.insert(job.recruitTypeName);
        sources.insert(job.sourceName);
    }

    filterOptions["salary"] = salaries;
    filterOptions["tagNames"] = tags;
    filterOptions["cityName"] = cities;
    filterOptions["recruitTypeName"] = recruitTypes;
    filterOptions["sourceName"] = sources;
}

QMap<QString, QVector<QString>> CrawlerWindow::getFieldFilters() {
    return currentFilters;
}

void CrawlerWindow::showFilterDialog(const QString &field, QPushButton *button) {
    QDialog dialog(this);
    dialog.setWindowTitle(button->text());
    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLineEdit *searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText("搜索...");
    layout->addWidget(searchEdit);

    QCheckBox *selectAll = new QCheckBox("全选");
    layout->addWidget(selectAll);

    QListWidget *listWidget = new QListWidget;
    layout->addWidget(listWidget);

    // Populate listWidget with items from filterOptions
    const QSet<QString> &options = filterOptions.value(field);
    QVector<QString> currentChecked = currentFilters.value(field);
    for (const QString &option : options) {
        QListWidgetItem *item = new QListWidgetItem(option);
        item->setCheckState(currentChecked.contains(option) ? Qt::Checked : Qt::Unchecked);
        listWidget->addItem(item);
    }

    connect(searchEdit, &QLineEdit::textChanged, [listWidget](const QString &text) {
        for (int i = 0; i < listWidget->count(); ++i) {
            QListWidgetItem *item = listWidget->item(i);
            item->setHidden(!item->text().contains(text, Qt::CaseInsensitive));
        }
    });

    connect(selectAll, &QCheckBox::toggled, [listWidget](bool checked) {
        for (int i = 0; i < listWidget->count(); ++i) {
            listWidget->item(i)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        }
    });

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    QPushButton *cancelBtn = new QPushButton("取消");
    QPushButton *okBtn = new QPushButton("确认");
    buttonLayout->addWidget(cancelBtn);
    buttonLayout->addWidget(okBtn);
    layout->addLayout(buttonLayout);

    connect(cancelBtn, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(okBtn, &QPushButton::clicked, &dialog, [this, &dialog, listWidget, field]() {
        QVector<QString> checked;
        for (int i = 0; i < listWidget->count(); ++i) {
            if (listWidget->item(i)->checkState() == Qt::Checked) {
                checked.append(listWidget->item(i)->text());
            }
        }
        currentFilters[field] = checked;
        onSearchClicked(false);
        dialog.accept();
    });

    dialog.exec();
}

void CrawlerWindow::showJobDetail(const SQLNS::JobInfoPrint &job) {
    QDialog dialog(this);
    dialog.setWindowTitle("职位详情");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    layout->addWidget(new QLabel(QString("职位ID: %1").arg(job.jobId)));
    layout->addWidget(new QLabel(QString("职位名称: %1").arg(job.jobName)));
    layout->addWidget(new QLabel(QString("公司: %1").arg(job.companyName)));
    layout->addWidget(new QLabel(QString("招聘类型: %1").arg(job.recruitTypeName)));
    layout->addWidget(new QLabel(QString("城市: %1").arg(job.cityName)));
    layout->addWidget(new QLabel(QString("来源: %1").arg(job.sourceName)));
    layout->addWidget(new QLabel(QString("薪资: %1-%2").arg(job.salaryMin).arg(job.salaryMax)));
    layout->addWidget(new QLabel(QString("要求: %1").arg(job.requirements)));
    layout->addWidget(new QLabel(QString("标签: %1").arg(job.tagNames.join(", "))));
    layout->addWidget(new QLabel(QString("创建时间: %1").arg(job.createTime)));
    layout->addWidget(new QLabel(QString("更新时间: %1").arg(job.updateTime)));
    layout->addWidget(new QLabel(QString("HR最后登录: %1").arg(job.hrLastLoginTime)));
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    dialog.exec();
}

void CrawlerWindow::onClearFilters() {
    currentFilters.clear();
    onSearchClicked(false);
}

void CrawlerWindow::closeEvent(QCloseEvent *event) {
    emit returnToLauncher();
    event->accept();
}
