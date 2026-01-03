#include "configwindow.h"

#include <QCheckBox>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include "../config/config_manager.h"

ConfigWindow::ConfigWindow(QWidget *parent)
    : QMainWindow(parent)
    , saveAndVectorizeCheck(nullptr)
    , sendAlertCheck(nullptr)
    , receiverEdit(nullptr)
    , saveButton(nullptr)
    , cancelButton(nullptr) {
    buildUI();
    applyStyle();
    loadSettings();
}

void ConfigWindow::buildUI() {
    setWindowTitle("系统设置");
    setMinimumSize(520, 420);

    QWidget *central = new QWidget(this);
    setCentralWidget(central);

    QVBoxLayout *mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(30, 30, 30, 30);
    mainLayout->setSpacing(20);

    QLabel *title = new QLabel("系统设置", this);
    title->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    title->setStyleSheet("font-size: 28px; font-weight: bold; color: #2c3e50;");
    mainLayout->addWidget(title);

    QLabel *subtitle = new QLabel("配置应用运行参数和告警通知", this);
    subtitle->setStyleSheet("font-size: 14px; color: #7f8c8d;");
    mainLayout->addWidget(subtitle);

    auto addSwitchRow = [this, mainLayout](const QString &labelText, QCheckBox *&checkbox) {
        QWidget *row = new QWidget(this);
        QHBoxLayout *rowLayout = new QHBoxLayout(row);
        rowLayout->setContentsMargins(12, 10, 12, 10);
        rowLayout->setSpacing(12);

        QLabel *label = new QLabel(labelText, row);
        label->setStyleSheet("font-size: 16px; color: #34495e;");
        checkbox = new QCheckBox(row);
        checkbox->setStyleSheet("QCheckBox::indicator { width: 26px; height: 26px; }");

        rowLayout->addWidget(label);
        rowLayout->addStretch();
        rowLayout->addWidget(checkbox);
        mainLayout->addWidget(row);
    };

    addSwitchRow("爬取并向量化", saveAndVectorizeCheck);
    addSwitchRow("异常邮件发送", sendAlertCheck);

    QWidget *emailRow = new QWidget(this);
    QHBoxLayout *emailLayout = new QHBoxLayout(emailRow);
    emailLayout->setContentsMargins(12, 10, 12, 10);
    emailLayout->setSpacing(12);

    QLabel *emailLabel = new QLabel("运维邮箱名称", emailRow);
    emailLabel->setStyleSheet("font-size: 16px; color: #34495e;");
    receiverEdit = new QLineEdit(emailRow);
    receiverEdit->setPlaceholderText("ops@example.com");

    emailLayout->addWidget(emailLabel);
    emailLayout->addStretch();
    emailLayout->addWidget(receiverEdit, 1);
    mainLayout->addWidget(emailRow);

    mainLayout->addStretch();

    QWidget *buttonRow = new QWidget(this);
    QHBoxLayout *buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->addStretch();

    cancelButton = new QPushButton("取消", buttonRow);
    saveButton = new QPushButton("保存", buttonRow);
    cancelButton->setFixedWidth(120);
    saveButton->setFixedWidth(160);

    buttonLayout->addWidget(cancelButton);
    buttonLayout->addWidget(saveButton);
    mainLayout->addWidget(buttonRow);

    connect(saveButton, &QPushButton::clicked, this, &ConfigWindow::onSaveClicked);
    connect(cancelButton, &QPushButton::clicked, this, &ConfigWindow::onCancelClicked);
}

void ConfigWindow::applyStyle() {
    saveButton->setObjectName("Primary");
    setStyleSheet(R"(
        ConfigWindow {
            background: qlineargradient(x1:0, y1:0, x2:1, y2:1,
                stop:0 #f9fbff, stop:1 #eef2f7);
        }
        QPushButton {
            padding: 10px 14px;
            border-radius: 8px;
            font-size: 14px;
        }
        QPushButton#Primary {
            background-color: #3498db;
            color: white;
            border: none;
        }
        QPushButton#Primary:hover {
            background-color: #2980b9;
        }
        QPushButton#Primary:pressed {
            background-color: #2471a3;
        }
    )");
}

void ConfigWindow::loadSettings() {
    ConfigManager::loadConfig();
    saveAndVectorizeCheck->setChecked(ConfigManager::getSaveAndVectorize(true));
    sendAlertCheck->setChecked(ConfigManager::getSendAlert(true));
    receiverEdit->setText(ConfigManager::getEmailReceiver());
}

void ConfigWindow::onSaveClicked() {
    const QString receiver = receiverEdit->text().trimmed();

    ConfigManager::setSaveAndVectorize(saveAndVectorizeCheck->isChecked());
    ConfigManager::setSendAlert(sendAlertCheck->isChecked());
    ConfigManager::setEmailReceiver(receiver);

    if (ConfigManager::saveConfig()) {
        QMessageBox::information(this, "已保存", "配置已保存到配置文件。");
    } else {
        QMessageBox::warning(this, "保存失败", "写入配置文件失败，请检查路径和权限。");
    }
}

void ConfigWindow::onCancelClicked() {
    close();
}

void ConfigWindow::closeEvent(QCloseEvent *event) {
    emit returnToLauncher();
    event->accept();
}
