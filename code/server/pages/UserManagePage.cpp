#include "UserManagePage.h"

#include <QLabel>
#include <QVBoxLayout>

UserManagePage::UserManagePage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("用户管理", this);
    title->setObjectName("pageTitle");

    QLabel *hint = new QLabel(this);
    hint->setObjectName("pageHint");
    hint->setAlignment(Qt::AlignCenter);
    hint->setText("功能开发中(阶段1)...\n\n"
                  "计划实现:\n"
                  "  1. 用户列表: ID / 手机号 / 昵称 / 余额 / 注册时间 / 状态\n"
                  "  2. 冻结 / 解冻 用户账号\n"
                  "  3. 按手机号模糊搜索用户");

    layout->addWidget(title);
    layout->addStretch();
    layout->addWidget(hint);
    layout->addStretch();
}
