#pragma once

#include <QString>

namespace Maintenance {

// Initialize logger. logDir will be created if missing (defaults to "logs").
// baseName will be used as prefix: e.g. baseName-YYYYMMDD-<ms>.log
bool initializeLogger(const QString &logDir = QStringLiteral("logs"), const QString &baseName = QStringLiteral("app"));

// Shutdown logger and flush/close file.
void shutdownLogger();

} // namespace Maintenance
