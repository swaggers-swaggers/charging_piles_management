#include <QApplication>
#include "AppTheme.h"
#include <QFile>
#include <QHostAddress>
#include <QMessageBox>
#include <QtGlobal>

#include "AdminLoginDialog.h"
#include "AdminMainWindow.h"
#include "ChargingEngine.h"
#include "DataExporter.h"
#include "DatabaseManager.h"
#include "HttpServer.h"
#include "LogManager.h"
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

    // 系统运行日志: 落盘 + 控制台, 管理端界面不展示
    LogManager::init();
    qInfo() << "[main] 服务端启动";

    AppTheme::apply(a);

    // 初始化数据库(服务端是数据库唯一持有者)
    QString dbErr;
    if (!DatabaseManager::instance().init(&dbErr)) {
        qCritical() << "[main] 数据库初始化失败:" << dbErr;
        QMessageBox::critical(nullptr, "数据库初始化失败", dbErr);
        LogManager::shutdown();
        return 1;
    }

    // 启动全局充电引擎: 恢复在充订单/释放孤儿桩, 之后每 3 秒推进一次
    ChargingEngine::instance().start();

    // 面向用户客户端的 TCP 服务
    TcpServer server;
    QString serverInfo;
    if (server.listen(QHostAddress::AnyIPv4, static_cast<quint16>(Protocol::serverPort()))) {
        serverInfo = QString("服务端口 %1 监听中").arg(server.serverPort());
        qInfo() << "[TcpServer] 客户端 TCP 服务端口" << server.serverPort() << "监听中";
    } else {
        serverInfo = QString("端口监听失败: %1").arg(server.errorString());
        qWarning() << "[TcpServer] 端口监听失败:" << server.errorString();
    }

    // 大屏数据定时导出(web/data.json)
    DataExporter exporter;
    serverInfo += QString("    |    大屏数据: %1/data.json").arg(DataExporter::exportDir());
    qInfo() << "[DataExporter] 大屏数据目录:" << DataExporter::exportDir();

    // 内置 HTTP 服务: 为 Web 大数据可视化大屏提供页面与数据
    // 浏览器访问 http://本机IP:8080 即可看到大屏(不要再双击 index.html, file:// 下浏览器会拦截数据请求)
    HttpServer http;
    http.setRoot(DataExporter::exportDir());
    http.setExporter(&exporter);   // /data.json 实时从数据库聚合, 不依赖落盘文件
    bool portOk = false;
    quint16 webPort = static_cast<quint16>(
        qEnvironmentVariable("CHARGING_WEB_PORT").toUShort(&portOk));
    if (!portOk || webPort == 0)
        webPort = 8080;
    QString webUrl;
    if (http.listen(QHostAddress::AnyIPv4, webPort)) {
        webUrl = QString("http://localhost:%1").arg(http.serverPort());
        serverInfo += QString("    |    大屏访问: %1").arg(webUrl);
        qInfo() << "[HttpServer] 大屏 HTTP 服务" << webUrl;
    } else {
        serverInfo += QString("    |    大屏 HTTP 服务启动失败: %1").arg(http.errorString());
        qWarning() << "[HttpServer] 大屏 HTTP 服务启动失败:" << http.errorString();
    }

    // 管理员登录 → 管理后台
    AdminLoginDialog dlg;
    if (dlg.exec() != QDialog::Accepted) {
        qInfo() << "[main] 管理员取消登录, 服务端退出";
        LogManager::shutdown();
        return 0;
    }
    qInfo() << "[main] 管理员登录成功, 进入管理后台";

    AdminMainWindow w(serverInfo, webUrl);
    w.showConnectionInfo(&server);
    w.show();
    const int rc = a.exec();
    qInfo() << "[main] 服务端退出";
    LogManager::shutdown();
    return rc;
}
