#include "workshopapi.h"
#include "debug.h"
#include "workshopsettings.h"
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

namespace WorkshopApi {

QJsonDocument query(const QString& path, const QByteArray& postData, const QString& method)
{
    QString socketPath = WorkshopSettings::self()->socketPath().trimmed();
    if (socketPath.isEmpty())
        socketPath = WorkshopSettings::defaultSocketPathValue();

    QNetworkAccessManager nam;
    QNetworkRequest request(QUrl(QStringLiteral("unix+http://localhost") + path));
    request.setAttribute(QNetworkRequest::FullLocalServerNameAttribute, socketPath);
    if (!postData.isEmpty())
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QNetworkReply* reply = nullptr;
    if (method == QStringLiteral("POST")) {
        reply = nam.post(request, postData);
    } else if (method == QStringLiteral("GET")) {
        reply = nam.get(request);
    } else {
        reply = nam.sendCustomRequest(request, method.toUtf8(), postData);
    }

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    if (reply->error() != QNetworkReply::NoError) {
        qCWarning(PLUGIN_KDEVELOP_WORKSHOP) << "Request failed for" << method << path << ":" << reply->errorString();
        reply->deleteLater();
        return {};
    }

    const QByteArray body = reply->readAll();
    reply->deleteLater();

    auto result = QJsonDocument::fromJson(body);
    if (result.isNull()) {
        qCWarning(PLUGIN_KDEVELOP_WORKSHOP)
            << "Failed to parse JSON body for" << method << path << "- body prefix:" << body.left(120);
    }
    return result;
}

}
