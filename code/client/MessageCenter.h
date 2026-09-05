#ifndef MESSAGECENTER_H
#define MESSAGECENTER_H

#include <QDateTime>
#include <QList>
#include <QObject>
#include <QString>

// 客户端统一消息中心: 接收服务端所有 PushOrderEvent 推送, 持久化存储,
// 供消息中心页面展示和未读角标使用. PushOrderProgress(充电进度)不存储.
struct AppMessage {
    int id = 0;
    int type = 0;       // 1=系统 2=订单 3=退款 4=预约 5=排队
    QString title;
    QString content;
    QDateTime time;
    int orderId = 0;
    bool read = false;
};

class MessageCenter : public QObject
{
    Q_OBJECT
public:
    static MessageCenter &instance();

    QList<AppMessage> messages() const { return m_messages; }
    int unreadCount() const;

    void markRead(int id);
    void clearRead();

signals:
    void messageReceived(const AppMessage &msg);
    void unreadCountChanged(int count);

private:
    explicit MessageCenter(QObject *parent = nullptr);
    void onPushReceived(const QJsonObject &msg);
    void save();
    void load();

    QList<AppMessage> m_messages;
    int m_nextId = 1;
};

#endif // MESSAGECENTER_H
