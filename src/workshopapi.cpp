#include "workshopapi.h"
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
        return {};
    }

    QByteArray body = responseData.mid(headerEnd + 4);
    return QJsonDocument::fromJson(body);
}

}
