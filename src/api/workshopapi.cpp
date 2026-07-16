#include "workshopapi.h"
#include "debug.h"
#include "workshopsettings.h"

#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkRequestFactory>
#include <QRestAccessManager>
#include <QRestReply>
#include <optional>
#include <QNetworkRequest>

namespace WorkshopApi {

QJsonDocument query(const QString& path, const QByteArray& postData, const QString& method)
{
    QString socketPath = WorkshopSettings::self()->socketPath().trimmed();
    if (socketPath.isEmpty())
        socketPath = WorkshopSettings::defaultSocketPathValue();

    QNetworkAccessManager nam;
    QRestAccessManager rest(&nam);
    QNetworkRequestFactory requestFactory(QUrl(QStringLiteral("unix+http://localhost")));
    requestFactory.setAttribute(QNetworkRequest::FullLocalServerNameAttribute, socketPath);
    QNetworkRequest request = requestFactory.createRequest(path);
    if (!postData.isEmpty())
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    QEventLoop loop;
    std::optional<QJsonDocument> responseDoc;
    auto replyHandler = [&loop, &responseDoc, &method, &path](QRestReply& reply) {
        if (reply.hasError()) {
            qCWarning(PLUGIN_KDEVELOP_WORKSHOP) << "Request failed for" << method << path << ":" << reply.errorString();
            loop.quit();
            return;
        }

        const QByteArray body = reply.readBody();
        const QJsonDocument parsedDoc = QJsonDocument::fromJson(body);
        if (parsedDoc.isNull()) {
            qCWarning(PLUGIN_KDEVELOP_WORKSHOP)
                << "Failed to parse JSON body for" << method << path << "- body prefix:" << body.left(120);
        } else {
            responseDoc = parsedDoc;
        }

        loop.quit();
    };

    if (method == QStringLiteral("POST")) {
        rest.post(request, postData, &loop, replyHandler);
    } else if (method == QStringLiteral("GET")) {
        rest.get(request, &loop, replyHandler);
    } else {
        rest.sendCustomRequest(request, method.toUtf8(), postData, &loop, replyHandler);
    }

    loop.exec();

    if (!responseDoc.has_value()) {
        return {};
    }

    return *responseDoc;
}

}
