#include "workshopapi.h"
#include "debug.h"
#include "workshopsettings.h"

#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkRequestFactory>
#include <QRestAccessManager>
#include <QRestReply>
#include <QUrl>

namespace WorkshopApi {

void queryAsync(const QString& path, QObject* context, QueryCallback callback)
{
    queryAsync(path, QByteArray(), QStringLiteral("GET"), context, std::move(callback));
}

void queryAsync(const QString& path, const QByteArray& postData, const QString& method, QObject* context,
                QueryCallback callback)
{
    QString socketPath = WorkshopSettings::self()->socketPath().trimmed();
    if (socketPath.isEmpty())
        socketPath = WorkshopSettings::defaultSocketPathValue();

    auto* nam = new QNetworkAccessManager();
    auto* rest = new QRestAccessManager(nam, context);
    nam->setParent(rest);

    QNetworkRequestFactory requestFactory(QUrl(QStringLiteral("unix+http://localhost")));
    requestFactory.setAttribute(QNetworkRequest::FullLocalServerNameAttribute, socketPath);
    QNetworkRequest request = requestFactory.createRequest(path);
    if (!postData.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));
    }

    auto replyHandler = [callback = std::move(callback), method, path, rest](QRestReply& reply) mutable {
        QJsonDocument responseDoc;

        if (reply.hasError()) {
            qCWarning(PLUGIN_KDEVELOP_WORKSHOP) << "Request failed for" << method << path << ":" << reply.errorString();
        } else {
            const QByteArray body = reply.readBody();
            const QJsonDocument parsedDoc = QJsonDocument::fromJson(body);
            if (parsedDoc.isNull()) {
                qCWarning(PLUGIN_KDEVELOP_WORKSHOP)
                    << "Failed to parse JSON body for" << method << path << "- body prefix:" << body.left(120);
            } else {
                responseDoc = parsedDoc;
            }
        }

        callback(responseDoc);
        rest->deleteLater();
    };

    if (method == QStringLiteral("POST")) {
        rest->post(request, postData, context, std::move(replyHandler));
    } else if (method == QStringLiteral("GET")) {
        rest->get(request, context, std::move(replyHandler));
    } else {
        rest->sendCustomRequest(request, method.toUtf8(), postData, context, std::move(replyHandler));
    }
}

}
