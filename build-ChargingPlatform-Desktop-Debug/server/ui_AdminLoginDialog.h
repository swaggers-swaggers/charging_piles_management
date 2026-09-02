/********************************************************************************
** Form generated from reading UI file 'AdminLoginDialog.ui'
**
** Created by: Qt User Interface Compiler version 6.2.4
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ADMINLOGINDIALOG_H
#define UI_ADMINLOGINDIALOG_H

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

class Ui_AdminLoginDialog
{
public:
    QVBoxLayout *rootLayout;
    QLabel *titleLabel;
    QLabel *subtitleLabel;
    QSpacerItem *titleSpacer;
    QWidget *card;
    QVBoxLayout *cardLayout;
    QLabel *cardTitle;
    QLineEdit *nameEdit;
    QLineEdit *pwdEdit;
    QHBoxLayout *optionRow;
    QCheckBox *rememberChk;
    QSpacerItem *optionSpacer;
    QPushButton *loginBtn;
    QSpacerItem *midSpacer;
    QLabel *hintLabel;
    QPushButton *exitBtn;

    void setupUi(QDialog *AdminLoginDialog)
    {
        if (AdminLoginDialog->objectName().isEmpty())
            AdminLoginDialog->setObjectName(QString::fromUtf8("AdminLoginDialog"));
        AdminLoginDialog->setObjectName(QString::fromUtf8("loginDialog"));
        AdminLoginDialog->resize(420, 470);
        rootLayout = new QVBoxLayout(AdminLoginDialog);
        rootLayout->setSpacing(10);
        rootLayout->setObjectName(QString::fromUtf8("rootLayout"));
        rootLayout->setContentsMargins(32, 30, 32, 16);
        titleLabel = new QLabel(AdminLoginDialog);
        titleLabel->setObjectName(QString::fromUtf8("titleLabel"));
        titleLabel->setAlignment(Qt::AlignCenter);

        rootLayout->addWidget(titleLabel);

        subtitleLabel = new QLabel(AdminLoginDialog);
        subtitleLabel->setObjectName(QString::fromUtf8("subtitleLabel"));
        subtitleLabel->setAlignment(Qt::AlignCenter);

        rootLayout->addWidget(subtitleLabel);

        titleSpacer = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);

        rootLayout->addItem(titleSpacer);

        card = new QWidget(AdminLoginDialog);
        card->setObjectName(QString::fromUtf8("card"));
        cardLayout = new QVBoxLayout(card);
        cardLayout->setSpacing(12);
        cardLayout->setObjectName(QString::fromUtf8("cardLayout"));
        cardLayout->setContentsMargins(26, 24, 26, 24);
        cardTitle = new QLabel(card);
        cardTitle->setObjectName(QString::fromUtf8("cardTitle"));

        cardLayout->addWidget(cardTitle);

        nameEdit = new QLineEdit(card);
        nameEdit->setObjectName(QString::fromUtf8("nameEdit"));

        cardLayout->addWidget(nameEdit);

        pwdEdit = new QLineEdit(card);
        pwdEdit->setObjectName(QString::fromUtf8("pwdEdit"));
        pwdEdit->setEchoMode(QLineEdit::Password);

        cardLayout->addWidget(pwdEdit);

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
        loginBtn->setObjectName(QString::fromUtf8("adminLoginBtn"));
        loginBtn->setCursor(QCursor(Qt::PointingHandCursor));

        cardLayout->addWidget(loginBtn);


        rootLayout->addWidget(card);

        midSpacer = new QSpacerItem(20, 6, QSizePolicy::Minimum, QSizePolicy::Fixed);

        rootLayout->addItem(midSpacer);

        hintLabel = new QLabel(AdminLoginDialog);
        hintLabel->setObjectName(QString::fromUtf8("hintLabel"));
        hintLabel->setAlignment(Qt::AlignCenter);

        rootLayout->addWidget(hintLabel);

        exitBtn = new QPushButton(AdminLoginDialog);
        exitBtn->setObjectName(QString::fromUtf8("exitBtn"));
        exitBtn->setCursor(QCursor(Qt::PointingHandCursor));

        rootLayout->addWidget(exitBtn, 0, Qt::AlignRight);


        retranslateUi(AdminLoginDialog);

        QMetaObject::connectSlotsByName(AdminLoginDialog);
    } // setupUi

    void retranslateUi(QDialog *AdminLoginDialog)
    {
        AdminLoginDialog->setWindowTitle(QCoreApplication::translate("AdminLoginDialog", "\344\270\234\350\275\257\347\224\265\345\212\250\346\261\275\350\275\246\345\205\205\347\224\265\346\241\251\345\272\224\347\224\250\347\256\241\347\220\206\345\271\263\345\217\260 - \346\234\215\345\212\241\347\253\257", nullptr));
        titleLabel->setText(QCoreApplication::translate("AdminLoginDialog", "\342\232\241 \345\205\205\347\224\265\346\241\251\347\256\241\347\220\206\345\271\263\345\217\260", nullptr));
        subtitleLabel->setText(QCoreApplication::translate("AdminLoginDialog", "\346\234\215 \345\212\241 \347\253\257 \302\267 \347\256\241 \347\220\206 \345\221\230 \347\231\273 \345\275\225", nullptr));
        cardTitle->setText(QCoreApplication::translate("AdminLoginDialog", "\347\256\241\347\220\206\345\221\230\347\231\273\345\275\225", nullptr));
        nameEdit->setPlaceholderText(QCoreApplication::translate("AdminLoginDialog", "\350\257\267\350\276\223\345\205\245\347\256\241\347\220\206\345\221\230\350\264\246\345\217\267", nullptr));
        pwdEdit->setPlaceholderText(QCoreApplication::translate("AdminLoginDialog", "\350\257\267\350\276\223\345\205\245\347\256\241\347\220\206\345\221\230\345\257\206\347\240\201", nullptr));
        rememberChk->setText(QCoreApplication::translate("AdminLoginDialog", "\350\256\260\344\275\217\350\264\246\345\217\267", nullptr));
        loginBtn->setText(QCoreApplication::translate("AdminLoginDialog", "\347\231\273 \345\275\225", nullptr));
        hintLabel->setText(QCoreApplication::translate("AdminLoginDialog", "\351\273\230\350\256\244\350\264\246\345\217\267:admin  \345\257\206\347\240\201:123456 (\346\234\254\346\234\272\346\225\260\346\215\256\345\272\223\346\240\241\351\252\214)", nullptr));
        exitBtn->setText(QCoreApplication::translate("AdminLoginDialog", "\351\200\200\345\207\272\347\250\213\345\272\217", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AdminLoginDialog: public Ui_AdminLoginDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ADMINLOGINDIALOG_H
