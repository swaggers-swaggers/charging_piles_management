/********************************************************************************
** Form generated from reading UI file 'LoginDialog.ui'
**
** Created by: Qt User Interface Compiler version 6.2.4
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_LOGINDIALOG_H
#define UI_LOGINDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_LoginDialog
{
public:
    QVBoxLayout *rootLayout;
    QLabel *titleLabel;
    QLabel *subtitleLabel;
    QSpacerItem *titleSpacer;
    QWidget *card;
    QVBoxLayout *cardLayout;
    QLabel *cardTitle;
    QLineEdit *phoneEdit;
    QHBoxLayout *optionRow;
    QCheckBox *rememberChk;
    QSpacerItem *optionSpacer;
    QPushButton *loginBtn;
    QLabel *tipLabel;
    QSpacerItem *cardSpacer;
    QSpacerItem *midSpacer;
    QLabel *hintLabel;
    QPushButton *exitBtn;

    void setupUi(QDialog *LoginDialog)
    {
        if (LoginDialog->objectName().isEmpty())
            LoginDialog->setObjectName(QString::fromUtf8("LoginDialog"));
        LoginDialog->setObjectName(QString::fromUtf8("loginDialog"));
        LoginDialog->resize(440, 560);
        rootLayout = new QVBoxLayout(LoginDialog);
        rootLayout->setSpacing(10);
        rootLayout->setObjectName(QString::fromUtf8("rootLayout"));
        rootLayout->setContentsMargins(36, 34, 36, 18);
        titleLabel = new QLabel(LoginDialog);
        titleLabel->setObjectName(QString::fromUtf8("titleLabel"));
        titleLabel->setAlignment(Qt::AlignCenter);

        rootLayout->addWidget(titleLabel);

        subtitleLabel = new QLabel(LoginDialog);
        subtitleLabel->setObjectName(QString::fromUtf8("subtitleLabel"));
        subtitleLabel->setAlignment(Qt::AlignCenter);

        rootLayout->addWidget(subtitleLabel);

        titleSpacer = new QSpacerItem(20, 14, QSizePolicy::Minimum, QSizePolicy::Fixed);

        rootLayout->addItem(titleSpacer);

        card = new QWidget(LoginDialog);
        card->setObjectName(QString::fromUtf8("card"));
        cardLayout = new QVBoxLayout(card);
        cardLayout->setSpacing(12);
        cardLayout->setObjectName(QString::fromUtf8("cardLayout"));
        cardLayout->setContentsMargins(26, 24, 26, 22);
        cardTitle = new QLabel(card);
        cardTitle->setObjectName(QString::fromUtf8("cardTitle"));

        cardLayout->addWidget(cardTitle);

        phoneEdit = new QLineEdit(card);
        phoneEdit->setObjectName(QString::fromUtf8("phoneEdit"));
        phoneEdit->setMaxLength(11);

        cardLayout->addWidget(phoneEdit);

        optionRow = new QHBoxLayout();
        optionRow->setObjectName(QString::fromUtf8("optionRow"));
        rememberChk = new QCheckBox(card);
        rememberChk->setObjectName(QString::fromUtf8("rememberChk"));

        optionRow->addWidget(rememberChk);

        optionSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        optionRow->addItem(optionSpacer);


        cardLayout->addLayout(optionRow);

        loginBtn = new QPushButton(card);
        loginBtn->setObjectName(QString::fromUtf8("loginBtn"));
        loginBtn->setObjectName(QString::fromUtf8("userLoginBtn"));
        loginBtn->setCursor(QCursor(Qt::PointingHandCursor));

        cardLayout->addWidget(loginBtn);

        tipLabel = new QLabel(card);
        tipLabel->setObjectName(QString::fromUtf8("tipLabel"));
        tipLabel->setAlignment(Qt::AlignCenter);

        cardLayout->addWidget(tipLabel);

        cardSpacer = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);

        cardLayout->addItem(cardSpacer);


        rootLayout->addWidget(card);

        midSpacer = new QSpacerItem(20, 6, QSizePolicy::Minimum, QSizePolicy::Fixed);

        rootLayout->addItem(midSpacer);

        hintLabel = new QLabel(LoginDialog);
        hintLabel->setObjectName(QString::fromUtf8("hintLabel"));
        hintLabel->setAlignment(Qt::AlignCenter);

        rootLayout->addWidget(hintLabel);

        exitBtn = new QPushButton(LoginDialog);
        exitBtn->setObjectName(QString::fromUtf8("exitBtn"));
        exitBtn->setCursor(QCursor(Qt::PointingHandCursor));

        rootLayout->addWidget(exitBtn, 0, Qt::AlignRight);


        retranslateUi(LoginDialog);

        QMetaObject::connectSlotsByName(LoginDialog);
    } // setupUi

    void retranslateUi(QDialog *LoginDialog)
    {
        LoginDialog->setWindowTitle(QCoreApplication::translate("LoginDialog", "\344\270\234\350\275\257\347\224\265\345\212\250\346\261\275\350\275\246\345\205\205\347\224\265\346\241\251\345\272\224\347\224\250\347\256\241\347\220\206\345\271\263\345\217\260 - \347\224\250\346\210\267\347\231\273\345\275\225", nullptr));
        titleLabel->setText(QCoreApplication::translate("LoginDialog", "\342\232\241 \344\270\234\350\275\257\345\205\205\347\224\265", nullptr));
        subtitleLabel->setText(QCoreApplication::translate("LoginDialog", "\347\224\265 \345\212\250 \346\261\275 \350\275\246 \345\205\205 \347\224\265 \346\234\215 \345\212\241", nullptr));
        cardTitle->setText(QCoreApplication::translate("LoginDialog", "\346\211\213\346\234\272\345\217\267\347\231\273\345\275\225", nullptr));
        phoneEdit->setPlaceholderText(QCoreApplication::translate("LoginDialog", "\350\257\267\350\276\223\345\205\24511\344\275\215\346\211\213\346\234\272\345\217\267", nullptr));
        rememberChk->setText(QCoreApplication::translate("LoginDialog", "\350\256\260\344\275\217\346\211\213\346\234\272\345\217\267", nullptr));
        loginBtn->setText(QCoreApplication::translate("LoginDialog", "\347\231\273\345\275\225 / \346\263\250\345\206\214", nullptr));
        tipLabel->setText(QCoreApplication::translate("LoginDialog", "\346\211\213\346\234\272\345\217\267\351\246\226\346\254\241\347\231\273\345\275\225\345\260\206\350\207\252\345\212\250\346\263\250\345\206\214\346\226\260\347\224\250\346\210\267", nullptr));
        hintLabel->setText(QCoreApplication::translate("LoginDialog", "\351\234\200\350\246\201\345\205\210\345\220\257\345\212\250\346\234\215\345\212\241\347\253\257 ChargingServer", nullptr));
        exitBtn->setText(QCoreApplication::translate("LoginDialog", "\351\200\200\345\207\272\347\250\213\345\272\217", nullptr));
    } // retranslateUi

};

namespace Ui {
    class LoginDialog: public Ui_LoginDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_LOGINDIALOG_H
