#include "crawlerwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QSizePolicy>
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
#include <QMessageBox>
#include "tasks/presenter_task.h"
#include "tasks/crawler_task.h"
#include "db/sqlinterface.h"
#include "presenter/presenter.h"
#include "constants/db_types.h"
#include "crawlprogresswindow.h"

CrawlerWindow::CrawlerWindow(QWidget *parent) : QMainWindow(parent), currentPage(1), pageSize(20), asc(true) {
    setWindowTitle("职位爬虫");
    setMinimumSize(960, 640);

    fieldMap["jobId"] = "jobId";
    fieldMap["工作名称"] = "jobName";
    fieldMap["招聘类型"] = "recruitTypeName";
    fieldMap["城市"] = "cityName";
    fieldMap["薪资"] = "salaryMin";
    fieldMap["来源"] = "sourceName";
    fieldMap["Tags"] = "tagNames";

    QWidget *central = new QWidget;
    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(28, 24, 28, 24);
    mainLayout->setSpacing(14);

    // top
    QHBoxLayout *topLayout = new QHBoxLayout;
    topLayout->setSpacing(10);
    searchLineEdit = new QLineEdit;
    searchLineEdit->setPlaceholderText("输入关键词...");
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
    filterLayout->setSpacing(10);
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
    // make columns stretch to fill available width
    header->setSectionResizeMode(QHeaderView::Stretch);
    table->setAlternatingRowColors(true);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(header, &QHeaderView::sectionClicked, this, &CrawlerWindow::onHeaderClicked);
    mainLayout->addWidget(table);

    // page
    QHBoxLayout *pageLayout = new QHBoxLayout;
    pageLayout->setSpacing(10);
    prevButton = new QPushButton("上一页");
    pageSpin = new QSpinBox;
    pageSpin->setMinimum(1);
    totalLabel = new QLabel("/ 1");
    nextButton = new QPushButton("下一页");
    pageLayout->addWidget(prevButton);
    pageLayout->addWidget(pageSpin);
    pageLayout->addWidget(totalLabel);
    pageLayout->addWidget(nextButton);
    // 每页显示数量设置
    QLabel *pageSizeLabel = new QLabel("每页:");
    pageSizeSpin = new QSpinBox;
    pageSizeSpin->setRange(1, 200);
    pageSizeSpin->setValue(pageSize);
    pageLayout->addWidget(pageSizeLabel);
    pageLayout->addWidget(pageSizeSpin);
    mainLayout->addLayout(pageLayout);

    setCentralWidget(central);

    setStyleSheet(R"(
        CrawlerWindow, QMainWindow {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f8f9fa, stop:1 #e9ecef);
        }
        QLineEdit {
            border: 1px solid #dfe6ed;
            border-radius: 10px;
            padding: 8px 12px;
            background: #ffffff;
            color: #000000;
        }
        QLineEdit:focus {
            border-color: #3498db;
            box-shadow: 0 0 0 2px rgba(52,152,219,0.15);
        }
        QSpinBox {
            background: #ffffff;
            color: #000000;
            border: 1px solid #dfe6ed;
            border-radius: 8px;
            padding: 4px 8px;
            min-width: 72px;
        }
        QPushButton {
            background-color: #3498db;
            color: #ffffff;
            border: none;
            border-radius: 10px;
            padding: 8px 14px;
            font-weight: bold;
            min-height: 32px;
        }
        QPushButton:hover {
            background-color: #2980b9;
        }
        QPushButton:pressed {
            background-color: #2471a3;
        }
        QPushButton#danger {
            background-color: #e74c3c;
        }
        QPushButton#danger:hover {
            background-color: #c0392b;
        }
        QPushButton#viewCellButton {
            background: transparent;
            color: #3498db;
            border: none;
            padding: 0 6px;
            min-height: 0;
            min-width: 0;
        }
        QPushButton#viewCellButton:hover {
            text-decoration: underline;
        }
        QTableWidget {
            background: #ffffff;
            border: 1px solid #ecf0f1;
            border-radius: 12px;
            gridline-color: #ecf0f1;
            selection-background-color: #e8f3fd;
        }
        QTableView::item:alternate {
            background: #f9fbff;
        }
        QHeaderView::section {
            background: #f4f6f8;
            padding: 10px 6px;
            border: none;
            border-right: 1px solid #e1e6eb;
            font-weight: bold;
            color: #000000;
        }
        QTableCornerButton::section {
            background: #f4f6f8;
            border: none;
            border-right: 1px solid #e1e6eb;
            color: #000000;
        }
        QTableWidget::item {
            padding: 6px;
            color: #000000;
        }
        QLabel {
            color: #000000;
        }
    )");

    // connect
    connect(searchButton, &QPushButton::clicked, this, [this]() { onSearchClicked(true); });
    connect(refreshButton, &QPushButton::clicked, this, [this]() { onSearchClicked(true); });
    connect(crawlButton, &QPushButton::clicked, this, &CrawlerWindow::onCrawlButtonClicked);
    connect(prevButton, &QPushButton::clicked, this, [this]() { onPrevPage(); });
    connect(nextButton, &QPushButton::clicked, this, [this]() { onNextPage(); });
    connect(pageSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int page) { onPageChanged(page); });
    // apply page size only when user confirms
    QPushButton *applyPageSizeBtn = new QPushButton("确定");
    pageLayout->addWidget(applyPageSizeBtn);
    connect(applyPageSizeBtn, &QPushButton::clicked, this, [this]() {
        pageSize = pageSizeSpin->value();
        currentPage = 1;
        onSearchClicked(true);
    });

    // Initial load
    onSearchClicked(true);
}

void CrawlerWindow::onCrawlButtonClicked() {
    QDialog dialog(this);
    dialog.setWindowTitle("爬取设置");
    dialog.setStyleSheet("QDialog { background: #ffffff; } QLabel { color: #050404ff; }");
    QVBoxLayout *layout = new QVBoxLayout(&dialog);
    QLabel *labelPages = new QLabel("为每个来源设置爬取页数（未勾选的来源将被忽略）:");
    labelPages->setStyleSheet("color: #000000;");
    layout->addWidget(labelPages);

    // list of available sources with per-source spinboxes
    QListWidget *listSources = new QListWidget();
    listSources->setStyleSheet(
        "QListWidget { background: #ffffff; color: #000000; border: 1px solid #dfe6ed; } "
        "QCheckBox { color: #000000; background: transparent; } "
        "QCheckBox::indicator { width: 18px; height: 18px; border: 1px solid #c4c9d0; border-radius: 4px; background: #ffffff; } "
        "QCheckBox::indicator:checked { background-color: #3498db; border-color: #2980b9; }"
    );
    QStringList sources = {"liepin", "nowcode", "zhipin", "chinahr", "wuyi"};
    struct ItemWidgets { QCheckBox *check; QSpinBox *spin; };
    std::vector<ItemWidgets> widgets;
    for (const QString &source : sources) {
        QListWidgetItem *item = new QListWidgetItem();
        QWidget *w = new QWidget();
        QHBoxLayout *h = new QHBoxLayout(w);
        QCheckBox *cb = new QCheckBox(source);
        cb->setChecked(true);
        QSpinBox *sp = new QSpinBox();
        sp->setRange(1, 100);
        sp->setValue(10);
        h->addWidget(cb);
        h->addStretch();
        h->addWidget(new QLabel("页数:"));
        h->addWidget(sp);
        h->setContentsMargins(2,2,2,2);
        listSources->addItem(item);
        item->setSizeHint(w->sizeHint());
        listSources->setItemWidget(item, w);
        widgets.push_back({cb, sp});
    }
    layout->addWidget(listSources);
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttonBox);
    connect(buttonBox, &QDialogButtonBox::accepted, &dialog, [this, listSources, sources, widgets]() {
        std::vector<std::string> selectedSources;
        std::vector<int> pagesList;
        for (int i = 0; i < listSources->count() && i < (int)widgets.size(); ++i) {
            if (widgets[i].check->isChecked()) {
                selectedSources.push_back(sources[i].toStdString());
                pagesList.push_back(widgets[i].spin->value());
            }
        }
        if (!selectedSources.empty()) {
            CrawlProgressWindow *progressWindow = new CrawlProgressWindow(selectedSources, pagesList, this);
            progressWindow->setAttribute(Qt::WA_DeleteOnClose);
            connect(progressWindow, &CrawlProgressWindow::crawlFinished, this, [this](const QString &summary) {
                onSearchClicked(true);
                QMessageBox msg(QMessageBox::Information, "爬取完成", summary, QMessageBox::Ok, this);
                msg.setStyleSheet(
                    "QMessageBox { background: #ffffff; } "
                    "QLabel { color: #000000; } "
                    "QPushButton { background-color: #3498db; color: #ffffff; border: none; border-radius: 10px; padding: 6px 14px; min-width: 72px; } "
                    "QPushButton:hover { background-color: #2980b9; } "
                    "QPushButton:pressed { background-color: #2471a3; }"
                );
                msg.exec();
            });
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
        viewButton->setObjectName("viewCellButton");
        viewButton->setMinimumWidth(56);
        viewButton->setMaximumWidth(80);
        viewButton->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);
        connect(viewButton, &QPushButton::clicked, this, [this, job]() { showJobDetail(job); });
        table->setCellWidget(i, 7, viewButton);
    }
}

void CrawlerWindow::populateFilters(const QVector<SQLNS::JobInfoPrint> &jobs) {
    QSet<QString> salaries, tags, cities, recruitTypes, sources;
    for (const auto &job : jobs) {
        // use salarySlabId for filtering
        salaries.insert(QString::number(job.salarySlabId));
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
    dialog.setStyleSheet(R"(
        QDialog { background: #ffffff; }
        QLineEdit { border: 1px solid #dfe6ed; border-radius: 8px; padding: 6px 8px; background: #ffffff; }
        QCheckBox { color: #000000; }
        QCheckBox::indicator { width: 16px; height: 16px; border: 1px solid #c4c9d0; border-radius: 3px; background: #ffffff; }
        QCheckBox::indicator:checked { background-color: #3498db; border-color: #2980b9; }
        QListWidget { background: #ffffff; border: 1px solid #dfe6ed; border-radius: 8px; }
        QPushButton { background-color: #3498db; color: #ffffff; border: none; border-radius: 8px; padding: 6px 12px; }
        QPushButton:hover { background-color: #2980b9; }
    )");

    QVBoxLayout *layout = new QVBoxLayout(&dialog);

    QLineEdit *searchEdit = new QLineEdit;
    searchEdit->setPlaceholderText("搜索...");
    layout->addWidget(searchEdit);

    QCheckBox *selectAll = new QCheckBox("全选");
    layout->addWidget(selectAll);

    QListWidget *listWidget = new QListWidget;
    listWidget->setSelectionMode(QAbstractItemView::NoSelection);
    layout->addWidget(listWidget);

    // Populate listWidget with custom widgets (keeps checkboxes visible/stylable)
    const QSet<QString> &optionsSet = filterOptions.value(field);
    QStringList optionsList;
    for (const QString &opt : optionsSet) optionsList << opt;
    optionsList.sort(Qt::CaseInsensitive);
    QVector<QString> currentChecked = currentFilters.value(field);
    // helper: convert salary slab id -> human readable label
    auto salaryLabel = [](int slabId) -> QString {
        if (slabId == 0) return QStringLiteral("薪资面议");
        switch (slabId) {
            case 1: return QStringLiteral("≤100/≤15k");
            case 2: return QStringLiteral("101-200/15k-25k");
            case 3: return QStringLiteral("201-300/25k-40k");
            case 4: return QStringLiteral("301-400/40k-60k");
            case 5: return QStringLiteral("401-600/60k-100k");
            case 6: return QStringLiteral("薪资面议");
            default: return QString::number(slabId);
        }
    };

    for (const QString &option : optionsList) {
        QListWidgetItem *item = new QListWidgetItem(listWidget);
        QWidget *w = new QWidget;
        QHBoxLayout *h = new QHBoxLayout(w);
        h->setContentsMargins(6, 2, 6, 2);
        QCheckBox *cb = nullptr;
        // If this is the salary field, show readable label but keep slab id as data
        if (field == "salary") {
            bool ok = false;
            int slab = option.toInt(&ok);
            QString label = ok ? salaryLabel(slab) : option;
            cb = new QCheckBox(label);
            cb->setProperty("value", option); // keep original id string
            cb->setChecked(currentChecked.contains(option));
        } else {
            cb = new QCheckBox(option);
            cb->setChecked(currentChecked.contains(option));
        }
        h->addWidget(cb);
        h->addStretch();
        item->setSizeHint(w->sizeHint());
        listWidget->setItemWidget(item, w);
    }

    connect(searchEdit, &QLineEdit::textChanged, [listWidget](const QString &text) {
        for (int i = 0; i < listWidget->count(); ++i) {
            QListWidgetItem *item = listWidget->item(i);
            QWidget *w = listWidget->itemWidget(item);
            if (!w) continue;
            QCheckBox *cb = w->findChild<QCheckBox *>();
            if (!cb) continue;
            item->setHidden(!cb->text().contains(text, Qt::CaseInsensitive));
        }
    });

    connect(selectAll, &QCheckBox::toggled, [listWidget](bool checked) {
        for (int i = 0; i < listWidget->count(); ++i) {
            QListWidgetItem *item = listWidget->item(i);
            QWidget *w = listWidget->itemWidget(item);
            if (!w) continue;
            QCheckBox *cb = w->findChild<QCheckBox *>();
            if (cb) cb->setChecked(checked);
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
            QListWidgetItem *item = listWidget->item(i);
            QWidget *w = listWidget->itemWidget(item);
            if (!w) continue;
            QCheckBox *cb = w->findChild<QCheckBox *>();
            if (cb && cb->isChecked()) {
                if (field == "salary" && cb->property("value").isValid()) {
                    checked.append(cb->property("value").toString());
                } else {
                    checked.append(cb->text());
                }
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
    dialog.setStyleSheet("QDialog { background: #ffffff; } QLabel { color: #000000; }");
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
