#ifndef USERINFOPAGE_H
#define USERINFOPAGE_H

#include <QWidget>

class QLabel;
class QLineEdit;
class QDoubleSpinBox;
class QPushButton;

// 用户信息维护页: 头像(默认灰色, 可换) / 昵称修改 / 余额充值, 全部经 Socket 由服务端处理
class UserInfoPage : public QWidget
{
    Q_OBJECT

public:
    explicit UserInfoPage(QWidget *parent = nullptr);

protected:
    void showEvent(QShowEvent *event) override;

private slots:
    void onRefresh();
    void onChangeAvatar();
    void onSaveNickname();
    void onRecharge();

private:
    QLabel *m_avatarLabel;
    QLabel *m_phoneLabel;
    QLabel *m_balanceLabel;
    QLineEdit *m_nickEdit;
    QPushButton *m_saveNickBtn;
    QDoubleSpinBox *m_rechargeSpin;
    QPushButton *m_rechargeBtn;
};

#endif // USERINFOPAGE_H
