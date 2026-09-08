#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <QJsonDocument>
#include <QLineEdit>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QSettings>
#include <QTemporaryDir>
#include "LoginDialog.h"
#include "TcpClient.h"
#include "protocol.h"

class LanConnectionTest : public QObject {
    Q_OBJECT
    QTemporaryDir settingsDir;
    QTcpServer first, second;
    bool respond = true;
    void serve(QTcpServer &server, const QString &name) {
        connect(&server, &QTcpServer::newConnection, this, [this, &server, name] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
            connect(socket, &QTcpSocket::readyRead, this, [this, socket, name] {
                while (socket->canReadLine()) {
                    auto request = QJsonDocument::fromJson(socket->readLine()).object();
                    if (!respond) continue;
                    QJsonObject reply{{"type", request["type"]}, {"ok", true}, {"server", name}};
                    socket->write(QJsonDocument(reply).toJson(QJsonDocument::Compact) + '\n');
                }
            });
        });
    }
private slots:
    void initTestCase() {
        QVERIFY(settingsDir.isValid());
        QCoreApplication::setOrganizationName("ChargingLanTests");
        QCoreApplication::setApplicationName("Isolated");
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, settingsDir.path());
        first.setProxy(QNetworkProxy::NoProxy);
        second.setProxy(QNetworkProxy::NoProxy);
        QVERIFY(first.listen(QHostAddress::LocalHost, 0));
        QVERIFY(second.listen(QHostAddress::LocalHost, 0));
        serve(first, "first"); serve(second, "second");
    }
    void switchEndpointAndBypassProxy() {
        // 系统代理不可用时，局域网 TCP 仍必须直连。
        QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::Socks5Proxy, "127.0.0.1", 1));
        auto &client = TcpClient::instance();
        QVERIFY(client.setEndpoint("127.0.0.1", first.serverPort()));
        QVERIFY(client.ensureConnected());
        bool ok = false;
        QCOMPARE(client.request(Protocol::ReqHeartbeat, {}, 1000, &ok)["server"].toString(), QString("first"));
        QVERIFY(ok);
        QVERIFY(client.setEndpoint("127.0.0.1", second.serverPort()));
        QVERIFY(client.ensureConnected());
        QCOMPARE(client.request(Protocol::ReqHeartbeat, {}, 1000, &ok)["server"].toString(), QString("second"));
        QVERIFY(ok);
        QNetworkProxy::setApplicationProxy(QNetworkProxy::NoProxy);
    }
    void rejectInvalidEndpoints() {
        auto &client = TcpClient::instance();
        QVERIFY(!client.setEndpoint("0.0.0.0", 9527));
        QVERIFY(!client.setEndpoint("255.255.255.255", 9527));
        QVERIFY(!client.setEndpoint("224.0.0.1", 9527));
        QVERIFY(!client.setEndpoint("127.0.0.1", 0));
        QVERIFY(!client.setEndpoint("127.0.0.1", 65536));
    }
    void failedConnectionThenRetry() {
        QTcpServer temporary;
        temporary.setProxy(QNetworkProxy::NoProxy);
        QVERIFY(temporary.listen(QHostAddress::LocalHost, 0));
        const auto unused = temporary.serverPort();
        temporary.close();
        auto &client = TcpClient::instance();
        QVERIFY(client.setEndpoint("127.0.0.1", unused));
        QString error;
        QVERIFY(!client.ensureConnected(500, &error));
        QVERIFY(error.contains("拒绝连接"));
        QVERIFY(error.contains(QString::number(unused)));
        QVERIFY(client.setEndpoint("127.0.0.1", first.serverPort()));
        QVERIFY(client.ensureConnected());
    }
    void timeoutDoesNotPoisonNextConnection() {
        auto &client = TcpClient::instance();
        QVERIFY(client.setEndpoint("127.0.0.1", second.serverPort()));
        // 零等待主动触发取消路径；本机连接有时可能先完成，均须允许随后切换。
        client.ensureConnected(0);
        QVERIFY(client.setEndpoint("127.0.0.1", first.serverPort()));
        QVERIFY(client.ensureConnected());
        bool ok = false;
        QCOMPARE(client.request(Protocol::ReqHeartbeat, {}, 1000, &ok)["server"].toString(), QString("first"));
        QVERIFY(ok);
    }
    void loginUiValidationAndPersistence() {
        LoginDialog dialog;
        dialog.show();
        auto *host = dialog.findChild<QLineEdit*>("serverHostEdit");
        auto *port = dialog.findChild<QSpinBox*>("serverPortSpin");
        auto *button = dialog.findChild<QPushButton*>("connectBtn");
        auto *status = dialog.findChild<QLabel*>("connectionStatus");
        QVERIFY(host && port && button && status);
        QCOMPARE(port->minimum(), 1); QCOMPARE(port->maximum(), 65535);
        host->setText("999.1.1.1");
        button->click();
        QVERIFY(status->text().contains("有效"));
        host->setText("127.0.0.1"); port->setValue(second.serverPort());
        button->click();
        QVERIFY(status->text().contains("已连接"));
        QVERIFY(button->isEnabled());
        QCOMPARE(QSettings().value("network/port").toInt(), int(second.serverPort()));
        LoginDialog restored;
        QCOMPARE(restored.findChild<QLineEdit*>("serverHostEdit")->text(), QString("127.0.0.1"));
        QCOMPARE(restored.findChild<QSpinBox*>("serverPortSpin")->value(), int(second.serverPort()));
        QVERIFY(dialog.grab().save("/tmp/charging-lan-login.png"));
        // 只有 TCP 接通而无协议应答，不能显示连接成功或覆盖保存的地址。
        host->setText("127.0.0.1"); port->setValue(first.serverPort());
        respond = false;
        button->click();
        QVERIFY(status->text().contains("连接失败"));
        QCOMPARE(QSettings().value("network/port").toInt(), int(second.serverPort()));
        QVERIFY(button->isEnabled());
        respond = true;
        button->click();
        QVERIFY(status->text().contains("已连接"));
    }
};
QTEST_MAIN(LanConnectionTest)
#include "lan_connection_test.moc"
