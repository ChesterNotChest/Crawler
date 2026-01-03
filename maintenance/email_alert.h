#ifndef EMAIL_ALERT_H
#define EMAIL_ALERT_H

#include <QString>

namespace Maintenance {

// Send an email alert asynchronously (non-blocking). Returns true if the send job was queued.
bool sendEmailAlertAsync(const QString& subject, const QString& body);

} // namespace Maintenance

#endif // EMAIL_ALERT_H
