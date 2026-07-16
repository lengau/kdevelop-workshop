#ifndef WORKSHOPAPI_H
#define WORKSHOPAPI_H

#include <QJsonDocument>
#include <QString>
#include <QByteArray>

namespace WorkshopApi {
    QJsonDocument query(const QString& path, const QByteArray& postData = QByteArray(), const QString& method = QStringLiteral("GET"));
}

#endif // WORKSHOPAPI_H
