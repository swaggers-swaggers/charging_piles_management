#include "UserInfoPage.h"

#include <QLabel>
#include <QVBoxLayout>

UserInfoPage::UserInfoPage(QWidget *parent)
    : QWidget(parent)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(24, 20, 24, 24);
    layout->setSpacing(16);

    QLabel *title = new QLabel("用户信息", this);
    title->setObjectName("pageTitle");

    QLabel *hint = new QLabel(this);
    hint->setObjectName("pageHint");
    hint->setAlignment(Qt::AlignCenter);
    hint->setText("功能开发中(阶段2)...\n\n"
                  "计划实现:\n"
                  "  1. 展示用户头像(默认灰色头像) / 昵称 / 钱包余额\n"
                  "  2. 更换头像 (从本地选择图片上传)\n"
                  "  3. 修改昵称\n"
                  "  4. 余额充值 (模拟支付, 余额实时更新)");

    layout->addWidget(title);
    layout->addStretch();
    layout->addWidget(hint);
    layout->addStretch();
}
