#pragma once

#include <QObject>
#include <QQueue>
#include <QHash>
#include <QList>
#include <QHostAddress>
#include <QElapsedTimer>

#include "protocol.h"

class QTcpServer;
class QTcpSocket;

// TCP 服务器：接收 KUKA EKI 主动连入的数据流。
// 全部运行在 GUI 线程（Qt 事件循环），因此无需跨线程加锁。
// armed 为 true 时接收线程才入队，暂停时置 false 避免队列无限增长。
class TcpServer : public QObject {
    Q_OBJECT
public:
    explicit TcpServer(QObject* parent = nullptr);
    ~TcpServer() override;

    bool startListen(quint16 port, const QHostAddress& host = QHostAddress::AnyIPv4);

    // 监听是否成功（失败时 startListen 返回 false，这里也能查）
    bool listening() const { return m_listening; }
    QString lastError() const { return m_lastError; }
    quint16 port() const { return m_port; }
    // 当前是否有 KUKA 连着（用于界面上的连接指示灯）
    bool hasClient() const { return !m_sockets.isEmpty(); }
    // 对端地址（多个客户端时取第一个）与"距最后一帧过去了多久"
    QString peerAddress() const;
    qint64 msSinceLastFrame() const;

    void setArmed(bool v) { m_armed = v; }
    bool armed() const { return m_armed; }
    void drain() { m_queue.clear(); }

    QQueue<Sample>& queue() { return m_queue; }

signals:
    void statusChanged(const QString& msg);
    // 连接状态变化（true = 有客户端接入）
    void clientChanged(bool connected);
    // 连接断开：记录层必须借此封住当前周期，否则重连后两件数据会被并成一组
    void disconnected();

private slots:
    void onNewConnection();
    void onReadyRead();
    void onDisconnected();

private:
    void handleData(QTcpSocket* s);
    void dispatch(const QByteArray& pkt);

    QTcpServer* m_server = nullptr;
    QList<QTcpSocket*> m_sockets;
    QHash<QTcpSocket*, QByteArray> m_buffers;
    QQueue<Sample> m_queue;
    bool m_armed = false;
    bool m_listening = false;
    QString m_lastError;
    quint16 m_port = 0;
    QElapsedTimer m_clock;
    qint64 m_lastFrameMs = -1;      // 最近一帧的到达时刻（m_clock 毫秒）
};
