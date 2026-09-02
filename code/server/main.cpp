#include <QApplication>
#include <QFile>
#include <QHostAddress>
#include <QMessageBox>
#include <QtGlobal>

#include "AdminLoginDialog.h"
#include "AdminMainWindow.h"
#include "DataExporter.h"
#include "DatabaseManager.h"
#include "HttpServer.h"
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

    // 内置 HTTP 服务: 为 Web 大数据可视化大屏提供页面与数据
    // 浏览器访问 http://本机IP:8080 即可看到大屏(不要再双击 index.html, file:// 下浏览器会拦截数据请求)
    HttpServer http;
    http.setRoot(DataExporter::exportDir());
    bool portOk = false;
    quint16 webPort = static_cast<quint16>(
        qEnvironmentVariable("CHARGING_WEB_PORT").toUShort(&portOk));
    if (!portOk || webPort == 0)
        webPort = 8080;
    if (http.listen(QHostAddress::AnyIPv4, webPort))
        serverInfo += QString("    |    大屏访问: http://localhost:%1").arg(http.serverPort());
    else
        serverInfo += QString("    |    大屏 HTTP 服务启动失败: %1").arg(http.errorString());

    // 管理员登录 → 管理后台
    AdminLoginDialog dlg;
    if (dlg.exec() != QDialog::Accepted)
        return 0;

    AdminMainWindow w(serverInfo);
    w.show();
    return a.exec();
}
