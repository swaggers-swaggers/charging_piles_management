#ifndef USERINFOPAGE_H
#define USERINFOPAGE_H

#include <QWidget>

// 用户信息维护页 (阶段 2 实现, 数据经 Socket 由服务端提供)
// 计划功能(项目说明书 1.4):
//   1. 展示用户头像(默认灰色头像) / 昵称 / 钱包余额(元)
//   2. 更换头像: 从本地选择图片上传
//   3. 修改昵称: 输入框编辑保存
//   4. 余额充值: 输入充值金额(元), 模拟支付成功, 余额实时更新
class UserInfoPage : public QWidget
{
    Q_OBJECT

public:
    explicit UserInfoPage(QWidget *parent = nullptr);
};

#endif // USERINFOPAGE_H
