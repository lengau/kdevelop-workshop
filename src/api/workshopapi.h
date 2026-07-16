#ifndef WORKSHOPAPI_H
#define WORKSHOPAPI_H

#include <QJsonDocument>
#include <QString>
#include <QByteArray>
#include <functional>

class QObject;

namespace WorkshopApi {
using QueryCallback = std::function<void(const QJsonDocument&)>;

void queryAsync(const QString& path, QObject* context, QueryCallback callback);
void queryAsync(const QString& path, const QByteArray& postData, const QString& method, QObject* context,
                QueryCallback callback);
}

#endif // WORKSHOPAPI_H
