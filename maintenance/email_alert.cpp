#include "email_alert.h"
#include "../config/config_manager.h"

#include <QJsonObject>
#include <QJsonValue>
#include <QDebug>
#include <QSslSocket>
#include <QByteArray>
#include <QThread>

#include <thread>
#include <functional>
#include <memory>

using namespace std::literals::chrono_literals;

namespace Maintenance {

static QByteArray toBase64(const QString& s) {
    return s.toUtf8().toBase64();
}

static void doSendEmail(const QJsonObject& senderCfg, const QString& receiver, const QString& subject, const QString& body) {
    QString smtpServer = senderCfg.value("smtp_server").toString();
    int smtpPort = senderCfg.value("smtp_port").toInt(465);
    QString username = senderCfg.value("username").toString();
    QString password = senderCfg.value("password").toString();
    QString fromAddr = senderCfg.value("from").toString();
    bool useSsl = senderCfg.value("use_ssl").toBool(true);

    if (smtpServer.isEmpty() || username.isEmpty() || password.isEmpty() || receiver.isEmpty()) {
        qDebug() << "[EmailAlert] Missing SMTP configuration or receiver";
        return;
    }

    if (fromAddr.isEmpty()) fromAddr = username;

    QSslSocket socket;
    socket.setPeerVerifyMode(QSslSocket::VerifyNone);

    if (useSsl) {
        socket.connectToHostEncrypted(smtpServer, smtpPort);
        if (!socket.waitForEncrypted(10000)) {
            qDebug() << "[EmailAlert] Failed to establish encrypted connection:" << socket.errorString();
            return;
        }
    } else {
        socket.connectToHost(smtpServer, smtpPort);
        if (!socket.waitForConnected(10000)) {
            qDebug() << "[EmailAlert] Failed to connect to SMTP server:" << socket.errorString();
            return;
        }
    }

    auto readResponse = [&](int timeout = 10000) -> QString {
        if (!socket.waitForReadyRead(timeout)) return QString();
        QByteArray resp = socket.readAll();
        return QString::fromUtf8(resp);
    };

    auto writeLine = [&](const QByteArray& line) {
        socket.write(line + "\r\n");
        socket.flush();
        socket.waitForBytesWritten(5000);
    };

    QString banner = readResponse(5000);
    Q_UNUSED(banner);

    writeLine("EHLO localhost");
    readResponse(5000);

    // AUTH LOGIN
    writeLine("AUTH LOGIN");
    readResponse(5000);
    writeLine(toBase64(username));
    readResponse(5000);
    writeLine(toBase64(password));
    readResponse(5000);

    // MAIL FROM / RCPT TO
    writeLine(QString("MAIL FROM:<%1>").arg(fromAddr).toUtf8());
    readResponse(5000);
    writeLine(QString("RCPT TO:<%1>").arg(receiver).toUtf8());
    readResponse(5000);

    // DATA
    writeLine("DATA");
    readResponse(5000);

    // Headers
    QByteArray msg;
    msg += "From: " + fromAddr.toUtf8() + "\r\n";
    msg += "To: " + receiver.toUtf8() + "\r\n";
    msg += "Subject: " + subject.toUtf8() + "\r\n";
    msg += "Content-Type: text/plain; charset=utf-8\r\n";
    msg += "\r\n";
    msg += body.toUtf8();
    msg += "\r\n.\r\n";

    socket.write(msg);
    socket.flush();
    socket.waitForBytesWritten(5000);
    readResponse(5000);

    writeLine("QUIT");
    readResponse(3000);

    socket.disconnectFromHost();
}

bool sendEmailAlertAsync(const QString& subject, const QString& body) {
    // Check global switch in config
    if (!ConfigManager::getSendAlert(false)) {
        qDebug() << "[EmailAlert] sendAlert disabled in config; skipping alert";
        return false;
    }

    QJsonObject sender = ConfigManager::getEmailSenderConfig();
    QString receiver = ConfigManager::getEmailReceiver();

    if (sender.isEmpty() || receiver.isEmpty()) {
        qDebug() << "[EmailAlert] Email sender or receiver not configured; skipping alert";
        return false;
    }

    // Run send in detached std::thread to avoid blocking caller
    std::thread t([sender, receiver, subject, body]() {
        doSendEmail(sender, receiver, subject, body);
    });
    t.detach();
    return true;
}

} // namespace Maintenance
