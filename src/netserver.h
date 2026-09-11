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

    void setArmed(bool v) { m_armed = v; }
    bool armed() const { return m_armed; }
    void drain() { m_queue.clear(); }

    QQueue<Sample>& queue() { return m_queue; }

signals:
    void statusChanged(const QString& msg);

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
    QElapsedTimer m_clock;
};
