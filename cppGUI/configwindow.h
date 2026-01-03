#ifndef CONFIGWINDOW_H
#define CONFIGWINDOW_H

#include <QMainWindow>

class QCheckBox;
class QLineEdit;
class QPushButton;

class ConfigWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit ConfigWindow(QWidget *parent = nullptr);

signals:
    void returnToLauncher();

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onSaveClicked();
    void onCancelClicked();

private:
    void buildUI();
    void loadSettings();
    void applyStyle();

    QCheckBox *saveAndVectorizeCheck;
    QCheckBox *sendAlertCheck;
    QLineEdit *receiverEdit;
    QPushButton *saveButton;
    QPushButton *cancelButton;
};

#endif // CONFIGWINDOW_H
