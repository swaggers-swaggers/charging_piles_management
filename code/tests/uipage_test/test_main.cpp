#include <QApplication>
#include <QTimer>
#include <QDebug>
#include "pages/UserInfoPage.h"
#include "network/TcpClient.h"
#include "protocol.h"
#include "ClientSession.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    ClientSession &s = ClientSession::instance();
    // 模拟登录(真实连接本机服务端)
    QJsonObject login = TcpClient::instance().request(
        Protocol::ReqUserLogin, QJsonObject{{"phone", "13800000001"}});
    if (login.value("ok").toBool()) {
        s.userId = login.value("userId").toInt();
        s.phone = login.value("phone").toString();
        s.nickname = login.value("nickname").toString();
        s.balance = login.value("balance").toDouble();
        qDebug() << "登录成功 userId=" << s.userId;
    } else {
        qDebug() << "登录失败:" << login.value("error").toString();
    }

    UserInfoPage page;
    page.resize(1040, 820);
    page.show();
    QTimer::singleShot(6000, &app, &QCoreApplication::quit);
    int rc = app.exec();
    qDebug() << "页面渲染完成, 未崩溃, exit=" << rc;
    return rc;
}
