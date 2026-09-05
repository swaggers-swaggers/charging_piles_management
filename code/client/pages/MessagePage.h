#ifndef MESSAGEPAGE_H
#define MESSAGEPAGE_H

#include <QWidget>

class QListWidget;
class QPushButton;
class QLabel;

// 用户端消息中心: 展示服务端推送的所有通知(订单结束/退款/预约/排队),
// 未读高亮, 点击标记已读, 支持清空已读. 数据来自 MessageCenter 单例.
class MessagePage : public QWidget
{
    Q_OBJECT
public:
    explicit MessagePage(QWidget *parent = nullptr);

private slots:
    void refresh();
    void onItemClicked(int row);
    void onClearRead();

private:
    QListWidget *m_list = nullptr;
    QPushButton *m_clearBtn = nullptr;
    QLabel *m_emptyLabel = nullptr;
};

#endif // MESSAGEPAGE_H
