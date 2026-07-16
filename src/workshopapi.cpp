#include "workshopapi.h"
#include "debug.h"
#include <QLocalSocket>
#include <KSharedConfig>
#include <KConfigGroup>

namespace WorkshopApi {

QJsonDocument query(const QString& path, const QByteArray& postData, const QString& method)
{
    QLocalSocket socket;

    // Read user-defined socket path from configuration
    auto config = KSharedConfig::openConfig(QStringLiteral("kdevelop-workshoprc"));
    KConfigGroup group = config->group(QStringLiteral("General"));
    QString socketPath = group.readEntry("SocketPath", QString()).trimmed();

    // Default to the Snap-specific socket path if empty
    if (socketPath.isEmpty()) {
        socketPath = QStringLiteral("/var/snap/workshop/common/workshop/workshop.socket");
    }

    socket.connectToServer(socketPath);
    // 15 seconds connection timeout to allow systemd to spin up the socket-activated daemon
    if (!socket.waitForConnected(15000)) {
        qCWarning(PLUGIN_KDEVELOP_WORKSHOP) << "Failed to connect to socket:" << socketPath;
        return {};
    }

    QByteArray request;
    request.append(method.toUtf8() + " " + path.toUtf8() + " HTTP/1.1\r\n");
    request.append("Host: localhost\r\n");
    request.append("Connection: close\r\n");
    if (!postData.isEmpty()) {
        request.append("Content-Type: application/json\r\n");
        request.append("Content-Length: " + QByteArray::number(postData.length()) + "\r\n");
        request.append("\r\n");
        request.append(postData);
    } else {
        request.append("\r\n");
    }

    socket.write(request);
    socket.flush();

    QByteArray responseData;
    while (socket.state() == QLocalSocket::ConnectedState || socket.bytesAvailable() > 0) {
        if (socket.bytesAvailable() > 0) {
            responseData.append(socket.readAll());
        } else {
            // Wait up to 60 seconds for the response (necessary for blocking /wait requests)
            if (!socket.waitForReadyRead(60000)) {
                break;
            }
        }
    }

    int headerEnd = responseData.indexOf("\r\n\r\n");
    if (headerEnd == -1) {
        qCWarning(PLUGIN_KDEVELOP_WORKSHOP) << "No HTTP header terminator in response for" << method << path;
        return {};
    }

    QByteArray headers = responseData.left(headerEnd);
    QByteArray body = responseData.mid(headerEnd + 4);

    qCDebug(PLUGIN_KDEVELOP_WORKSHOP) << method << path << "→ headers:" << headers.left(200);

    // Decode chunked transfer encoding when the server uses it
    if (headers.toLower().contains("transfer-encoding: chunked")) {
        QByteArray decoded;
        int pos = 0;
        while (pos < body.size()) {
            int lineEnd = body.indexOf("\r\n", pos);
            if (lineEnd == -1)
                break;
            bool ok = false;
            int chunkSize = body.mid(pos, lineEnd - pos).toInt(&ok, 16);
            if (!ok || chunkSize == 0)
                break;
            pos = lineEnd + 2;
            decoded.append(body.mid(pos, chunkSize));
            pos += chunkSize + 2; // skip chunk data + trailing \r\n
        }
        body = decoded;
    }

    auto result = QJsonDocument::fromJson(body);
    if (result.isNull()) {
        qCWarning(PLUGIN_KDEVELOP_WORKSHOP)
            << "Failed to parse JSON body for" << method << path << "- body prefix:" << body.left(120);
    }
    return result;
}

}
