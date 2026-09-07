#include <QApplication>
#include "AppTheme.h"
#include <QFile>
#include "network/MapNetwork.h"
#include <QtGlobal>

#include "ClientSession.h"
#include "LoginDialog.h"
#include "UserMainWindow.h"

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);   // Qt5 生效, Qt6 默认已开启
#endif

    QApplication a(argc, argv);
    a.setOrganizationName("Neusoft");
    a.setApplicationName("ChargingClient");
    QApplication::setStyle("Fusion");

    MapNetwork::configure();

    AppTheme::apply(a);

    // 客户端不访问数据库, 登录经 Socket 由服务端校验
    LoginDialog dlg;
    if (dlg.exec() != QDialog::Accepted)
        return 0;

    UserMainWindow w;
    w.show();
    return a.exec();
}
