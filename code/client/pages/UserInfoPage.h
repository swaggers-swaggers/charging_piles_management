#ifndef USERINFOPAGE_H
#define USERINFOPAGE_H

#include <QWidget>

class QLabel;
class QBoxLayout;
class QLineEdit;
class QDoubleSpinBox;
class QPushButton;
class QTimer;

// 用户信息维护页: 头像(默认灰色, 可换) / 昵称修改 / 余额充值, 全部经 Socket 由服务端处理
class UserInfoPage : public QWidget
{
    Q_OBJECT

public:
    explicit UserInfoPage(QWidget *parent = nullptr);

protected:
    void resizeEvent(QResizeEvent *event) override;
    void showEvent(QShowEvent *event) override;

private slots:
    void onRefresh();
    void onChangeAvatar();
    void onSaveNickname();
    void onRecharge();

private:
    QBoxLayout *m_overview;
    QBoxLayout *m_settings;
    QLabel *m_nameLabel;
    QLabel *m_avatarLabel;
    QLabel *m_phoneLabel;
    QLabel *m_balanceLabel;
    QLineEdit *m_nickEdit;
    QPushButton *m_saveNickBtn;
    QDoubleSpinBox *m_rechargeSpin;
    QPushButton *m_rechargeBtn;
    QTimer *m_autoRefresh = nullptr;   // 每 5 秒自动刷新余额/资料, 无需重新进入页面
    bool m_autoSilent = false;         // 自动刷新期间失败不弹窗, 避免打断操作
};

#endif // USERINFOPAGE_H
