#include "netserver.h"

#include <QTcpServer>
#include <QTcpSocket>

#include "config.h"

TcpServer::TcpServer(QObject* parent) : QObject(parent) {
    m_clock.start();
}

TcpServer::~TcpServer() {
    if (m_server) {
        m_server->close();
    }
}

bool TcpServer::startListen(quint16 port, const QHostAddress& host) {
    m_server = new QTcpServer(this);
    m_port = port;
    if (!m_server->listen(host, port)) {
        m_listening = false;
        m_lastError = m_server->errorString();
        emit statusChanged(QStringLiteral("✗ 监听失败：%1 — %2")
                           .arg(port).arg(m_server->errorString()));
        return false;
    }
    m_listening = true;
    m_lastError.clear();
    connect(m_server, &QTcpServer::newConnection, this, &TcpServer::onNewConnection);
    emit statusChanged(QStringLiteral("◎ 监听 %1 端口（所有网卡），等待 KUKA 连接…").arg(port));
    return true;
}

void TcpServer::onNewConnection() {
    while (m_server->hasPendingConnections()) {
        QTcpSocket* s = m_server->nextPendingConnection();
        m_sockets.append(s);
        m_buffers.insert(s, QByteArray());
        connect(s, &QTcpSocket::readyRead, this, &TcpServer::onReadyRead);
        connect(s, &QTcpSocket::disconnected, this, &TcpServer::onDisconnected);
        emit statusChanged(QStringLiteral("● KUKA 已连接：%1").arg(s->peerAddress().toString()));
        emit clientChanged(true);
    }
}

void TcpServer::onReadyRead() {
    QTcpSocket* s = qobject_cast<QTcpSocket*>(sender());
    if (s) {
        handleData(s);
    }
}

void TcpServer::onDisconnected() {
    QTcpSocket* s = qobject_cast<QTcpSocket*>(sender());
    if (!s) {
        return;
    }
    m_sockets.removeAll(s);
    m_buffers.remove(s);
    s->deleteLater();
    emit statusChanged(QStringLiteral("○ KUKA 已断线，等待重新连接…"));
    emit disconnected();
    emit clientChanged(!m_sockets.isEmpty());
}

void TcpServer::handleData(QTcpSocket* s) {
    QByteArray& buf = m_buffers[s];
    buf += s->readAll();

    const QByteArray closeTag("</ECM>");
    const QByteArray openTag("<ECM>");
    int closeIdx;
    while ((closeIdx = buf.indexOf(closeTag)) >= 0) {
        int openIdx = buf.indexOf(openTag);
        int end = closeIdx + closeTag.size();
        if (openIdx < 0 || openIdx > closeIdx) {
            // 半帧/前导垃圾：丢弃到 </ECM> 结尾
            buf.remove(0, end);
            continue;
        }
        QByteArray pkt = buf.mid(openIdx, end - openIdx);
        buf.remove(0, end);
        dispatch(pkt);
    }
}

void TcpServer::dispatch(const QByteArray& pkt) {
    Sample s;
    if (!parsePacket(QString::fromUtf8(pkt), s)) {
        return;  // 解析失败只丢这一帧
    }
    m_lastFrameMs = m_clock.elapsed();
    if (!m_armed) {
        return;
    }
    s.ts = m_clock.elapsed() / 1000.0;
    m_queue.enqueue(s);
}

QString TcpServer::peerAddress() const {
    return m_sockets.isEmpty() ? QString() : m_sockets.first()->peerAddress().toString();
}

qint64 TcpServer::msSinceLastFrame() const {
    return m_lastFrameMs < 0 ? -1 : m_clock.elapsed() - m_lastFrameMs;
}
