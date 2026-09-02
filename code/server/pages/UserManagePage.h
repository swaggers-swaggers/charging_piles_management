#ifndef USERMANAGEPAGE_H
#define USERMANAGEPAGE_H

#include <QWidget>

// 用户管理页 (阶段 1 实现)
// 计划功能(项目说明书 1.4):
//   1. 列表展示: 用户ID / 手机号 / 昵称 / 钱包余额 / 注册时间 / 状态(正常冻结)
//   2. 冻结 / 解冻 用户账号 (风控场景)
//   3. 按手机号模糊搜索用户
class UserManagePage : public QWidget
{
    Q_OBJECT

public:
    explicit UserManagePage(QWidget *parent = nullptr);
};

#endif // USERMANAGEPAGE_H
