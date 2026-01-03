#include "logger.h"
#include <QFile>
#include <QDir>
#include <QDate>
#include <QDateTime>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>
#include <QCoreApplication>
#include <QDebug>

static QFile g_logFile;
static QMutex g_logMutex;
static QString g_logDir;
static QString g_baseName;
static QString g_currentDateStr;
static bool g_initialized = false;

static QString makeLogFileName(const QString &dir, const QString &baseName, const QDate &date) {
    // use milliseconds since epoch to ensure uniqueness: baseName-YYYYMMDD-<ms>.log
    qint64 ms = QDateTime::currentMSecsSinceEpoch();
    return QDir(dir).filePath(QString("%1-%2-%3.log").arg(baseName, date.toString("yyyyMMdd")).arg(QString::number(ms)));
}

static void rotateLogFileIfNeeded() {
    QString today = QDate::currentDate().toString("yyyyMMdd");
    if (g_currentDateStr == today && g_logFile.isOpen()) return;

    if (g_logFile.isOpen()) {
        g_logFile.flush();
        g_logFile.close();
    }

    QDir d(g_logDir);
    if (!d.exists()) d.mkpath(".");

    QString fn = makeLogFileName(g_logDir, g_baseName, QDate::currentDate());
    g_logFile.setFileName(fn);
    g_logFile.open(QIODevice::Append | QIODevice::Text);
    g_currentDateStr = QDate::currentDate().toString("yyyyMMdd");
}

static void myMessageHandler(QtMsgType type, const QMessageLogContext &ctx, const QString &msg) {
    QMutexLocker locker(&g_logMutex);
    if (!g_initialized) {
        // fallback to default qDebug output: write raw message to stdout
        QByteArray localMsg = msg.toLocal8Bit();
        fprintf(stdout, "%s\n", localMsg.constData());
        fflush(stdout);
        if (type == QtFatalMsg) abort();
        return;
    }

    rotateLogFileIfNeeded();

    if (!g_logFile.isOpen()) {
        // if cannot open file, fallback to stdout
        QByteArray localMsg = msg.toLocal8Bit();
        fprintf(stdout, "%s\n", localMsg.constData());
        fflush(stdout);
        return;
    }

    QString level;
    switch (type) {
    case QtDebugMsg: level = QStringLiteral("DEBUG"); break;
    case QtInfoMsg: level = QStringLiteral("INFO"); break;
    case QtWarningMsg: level = QStringLiteral("WARN"); break;
    case QtCriticalMsg: level = QStringLiteral("CRIT"); break;
    case QtFatalMsg: level = QStringLiteral("FATAL"); break;
    }

    QString fileStr = ctx.file ? QString::fromUtf8(ctx.file) : QString();
    QString funcStr = ctx.function ? QString::fromUtf8(ctx.function) : QString();

    QString line = QString("%1 [%2] %3 (%4:%5 %6)\n")
        .arg(QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz"),
             level, msg, fileStr, QString::number(ctx.line), funcStr);

    QTextStream ts(&g_logFile);
    ts << line;
    ts.flush();

    // Mirror the original qDebug/qInfo/... message to stderr without adding
    // the file/level/time prefixes so the console shows the native output.
    QByteArray localMsg = msg.toLocal8Bit();
    fprintf(stdout, "%s\n", localMsg.constData());
    fflush(stdout);

    if (type == QtFatalMsg) {
        g_logFile.flush();
        abort();
    }
}

namespace Maintenance {

bool initializeLogger(const QString &logDir, const QString &baseName) {
    // Perform file/directory setup while holding the mutex, but do NOT call
    // any qDebug/qInstallMessageHandler while holding the lock to avoid
    // deadlocks where the message handler attempts to lock the same mutex.
    {
        QMutexLocker locker(&g_logMutex);
        if (g_initialized) return true;
        // Resolve relative path: if caller provided a relative `logDir` (e.g. "logs"),
        // prefer a `logs` folder located at the project root (where CMakeLists.txt resides).
        QString resolvedLogDir;
        QDir inDir(logDir);
        if (inDir.isRelative()) {
            QString appDir = QCoreApplication::applicationDirPath();
            QDir d(appDir);
            bool found = false;
            // climb up at most 6 levels to find CMakeLists.txt
            for (int i = 0; i < 6; ++i) {
                QString candidate = d.absoluteFilePath("CMakeLists.txt");
                if (QFile::exists(candidate)) { found = true; break; }
                if (!d.cdUp()) break;
            }
            QString projectDir = found ? d.absolutePath() : appDir;
            resolvedLogDir = QDir(projectDir).filePath(logDir);
        } else {
            resolvedLogDir = logDir;
        }

        g_logDir = resolvedLogDir;
        g_baseName = baseName;

        QDir d(g_logDir);
        if (!d.exists()) {
            if (!d.mkpath(".")) {
                return false;
            }
        }

        rotateLogFileIfNeeded();
    }

    // Install handler without holding the mutex so the handler can acquire
    // the lock when the first message is emitted.
    qInstallMessageHandler(myMessageHandler);

    // Now mark initialized under the mutex briefly.
    {
        QMutexLocker locker(&g_logMutex);
        g_initialized = true;
    }

    qDebug() << "Logger initialized. Log file:" << g_logFile.fileName();
    return true;
}

void shutdownLogger() {
    QMutexLocker locker(&g_logMutex);
    if (g_logFile.isOpen()) {
        g_logFile.flush();
        g_logFile.close();
    }
    g_initialized = false;
    // Note: cannot uninstall message handler, but we stop writing to file when not initialized
}

} // namespace Maintenance
