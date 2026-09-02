#include <QApplication>
#include <QFile>
#include <QMessageBox>

#include "DatabaseManager.h"
#include "Session.h"
#include "LoginDialog.h"
#include "AdminMainWindow.h"
#include "UserMainWindow.h"

int main(int argc, char *argv[])
{
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QApplication a(argc, argv);
    a.setOrganizationName("Neusoft");
    a.setApplicationName("ChargingPlatform");
    QApplication::setStyle("Fusion");

    // 全局样式表
    QFile globalQss(":/qss/global.qss");
    if (globalQss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        a.setStyleSheet(QString::fromUtf8(globalQss.readAll()));
        globalQss.close();
    }

    // 初始化数据库(打开 / 建表 / 默认数据), 失败时给出明确提示
    QString dbErr;
    if (!DatabaseManager::instance().init(&dbErr)) {
        QMessageBox::critical(nullptr, "数据库初始化失败", dbErr);
        return 1;
    }

    // 登录
    LoginDialog dlg;
    if (dlg.exec() != QDialog::Accepted)
        return 0;

    // 根据登录身份进入对应主界面
    if (Session::instance().isAdmin) {
        AdminMainWindow w;
        w.show();
        return a.exec();
    }

    UserMainWindow w;
    w.show();
    return a.exec();
}
