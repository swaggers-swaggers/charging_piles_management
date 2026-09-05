#include "MessageCenter.h"

#include "network/TcpClient.h"
#include "protocol.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>

MessageCenter &MessageCenter::instance()
{
    static MessageCenter s;
    return s;
}

MessageCenter::MessageCenter(QObject *parent)
    : QObject(parent)
{
    connect(&TcpClient::instance(), &TcpClient::pushReceived,
            this, &MessageCenter::onPushReceived);
    load();
}

int MessageCenter::unreadCount() const
{
    int n = 0;
    for (const AppMessage &m : m_messages)
        if (!m.read) ++n;
    return n;
}

void MessageCenter::markRead(int id)
{
    for (AppMessage &m : m_messages) {
        if (m.id == id) { m.read = true; break; }
    }
    save();
    emit unreadCountChanged(unreadCount());
}

void MessageCenter::clearRead()
{
    QList<AppMessage> unread;
    for (const AppMessage &m : m_messages)
        if (!m.read) unread.append(m);
    m_messages = unread;
    save();
    emit unreadCountChanged(unreadCount());
}

void MessageCenter::onPushReceived(const QJsonObject &msg)
{
    const int type = msg.value("type").toInt();
    if (type != Protocol::PushOrderEvent)
        return;   // 充电进度推送不存消息

    const int event = msg.value("event").toInt();
    AppMessage m;
    m.id = m_nextId++;
    m.time = QDateTime::currentDateTime();
    m.orderId = msg.value("orderId").toInt();
    m.read = false;

    switch (event) {
    case 1:
        m.type = 5;
        m.title = QStringLiteral("排队轮到您了");
        m.content = msg.value("message").toString(QStringLiteral("电桩已空闲，请尽快确认开始充电（超时将顺延）"));
        break;
    case 2:
        m.type = 2;
        m.title = QStringLiteral("充电已结束");
        m.content = msg.value("message").toString(QStringLiteral("订单已完成结算，冻结金额已解冻"));
        break;
    case 3:
        m.type = 2;
        m.title = QStringLiteral("充电异常中断");
        m.content = msg.value("message").toString(QStringLiteral("订单因故障中断，已自动结算并释放冻结金额"));
        break;
    case 4:
        m.type = 5;
        m.title = QStringLiteral("排队位置更新");
        m.content = msg.value("message").toString(QStringLiteral("您的排队位置已更新"));
        break;
    case 5:
        m.type = 2;
        m.title = QStringLiteral("充电已开始");
        m.content = msg.value("message").toString(QStringLiteral("充电已开始，预授权冻结金额已扣除"));
        break;
    case 6:
        m.type = 4;
        m.title = QStringLiteral("预约提醒");
        m.content = msg.value("message").toString(QStringLiteral("您预约的电桩即将开放，请准备到场"));
        break;
    case 7:
        m.type = 4;
        m.title = QStringLiteral("预约/排队通知");
        m.content = msg.value("message").toString(QStringLiteral("您的预约/排队状态已更新"));
        break;
    case 8:
        m.type = 3;
        m.title = QStringLiteral("退款到账");
        m.content = msg.value("message").toString(QStringLiteral("退款已到账，请查收"));
        break;
    case 9:
        m.type = 1;
        m.title = QStringLiteral("充值成功");
        m.content = msg.value("message").toString(QStringLiteral("充值已到账"));
        break;
    default:
        return;
    }

    m_messages.prepend(m);
    if (m_messages.size() > 100)
        m_messages = m_messages.mid(0, 100);
    save();
    emit messageReceived(m);
    emit unreadCountChanged(unreadCount());
}

void MessageCenter::save()
{
    QSettings s;
    QJsonArray arr;
    for (const AppMessage &m : m_messages) {
        QJsonObject o;
        o["id"] = m.id;
        o["type"] = m.type;
        o["title"] = m.title;
        o["content"] = m.content;
        o["time"] = m.time.toString(Qt::ISODate);
        o["orderId"] = m.orderId;
        o["read"] = m.read;
        arr.append(o);
    }
    s.setValue("messages", arr);
    s.setValue("nextMsgId", m_nextId);
}

void MessageCenter::load()
{
    QSettings s;
    m_nextId = s.value("nextMsgId", 1).toInt();
    const QJsonArray arr = s.value("messages").toJsonArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        AppMessage m;
        m.id = o["id"].toInt();
        m.type = o["type"].toInt();
        m.title = o["title"].toString();
        m.content = o["content"].toString();
        m.time = QDateTime::fromString(o["time"].toString(), Qt::ISODate);
        m.orderId = o["orderId"].toInt();
        m.read = o["read"].toBool();
        m_messages.append(m);
    }
}
