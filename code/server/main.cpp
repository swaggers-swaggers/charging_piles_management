#include <QApplication>
#include <QFile>
#include <QHostAddress>
#include <QMessageBox>
#include <QtGlobal>

#include "AdminLoginDialog.h"
#include "AdminMainWindow.h"
#include "DataExporter.h"
#include "DatabaseManager.h"
#include "network/TcpServer.h"
#include "protocol.h"

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);   // Qt5 生效, Qt6 默认已开启
#endif

    QApplication a(argc, argv);
    a.setOrganizationName("Neusoft");
    a.setApplicationName("ChargingServer");
    QApplication::setStyle("Fusion");

    // 全局样式表
    QFile globalQss(":/qss/global.qss");
    if (globalQss.open(QIODevice::ReadOnly | QIODevice::Text)) {
        a.setStyleSheet(QString::fromUtf8(globalQss.readAll()));
        globalQss.close();
    }

    // 初始化数据库(服务端是数据库唯一持有者)
    QString dbErr;
    if (!DatabaseManager::instance().init(&dbErr)) {
        QMessageBox::critical(nullptr, "数据库初始化失败", dbErr);
        return 1;
    }

    // 面向用户客户端的 TCP 服务
    TcpServer server;
    QString serverInfo;
    if (server.listen(QHostAddress::AnyIPv4, static_cast<quint16>(Protocol::serverPort())))
        serverInfo = QString("服务端口 %1 监听中").arg(server.serverPort());
    else
        serverInfo = QString("端口监听失败: %1").arg(server.errorString());

    // 大屏数据定时导出(web/data.json)
    DataExporter exporter;
    serverInfo += QString("    |    大屏数据: %1/data.json").arg(DataExporter::exportDir());

    // 管理员登录 → 管理后台
    AdminLoginDialog dlg;
    if (dlg.exec() != QDialog::Accepted)
        return 0;

    AdminMainWindow w(serverInfo);
    w.show();
    return a.exec();
}
