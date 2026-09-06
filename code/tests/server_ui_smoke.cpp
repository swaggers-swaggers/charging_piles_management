#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QClipboard>
#include <QTcpSocket>
#include <QNetworkProxy>
#include <QJsonDocument>
#include "network/TcpServer.h"
#include "protocol.h"
#include <QTemporaryDir>
#include <QListWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QtTest>
#include "AppTheme.h"
#include "DonutChart.h"
#include "AdminMainWindow.h"
#include "DatabaseManager.h"
#include "ChargingEngine.h"
#include "ChargingPowerModel.h"
#include "dao/OrderDao.h"
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    QPalette dark; dark.setColor(QPalette::Window,Qt::black); dark.setColor(QPalette::Base,Qt::black);
    app.setPalette(dark); AppTheme::apply(app);
    QTemporaryDir temp;
    qputenv("CHARGING_DB", (temp.path()+"/test.db").toUtf8());
    QString error;
    if(!DatabaseManager::instance().init(&error)) {qCritical()<<error;return 1;}
    QSqlQuery q;
    if(!q.exec("SELECT id FROM user WHERE status=0 LIMIT 1")||!q.next()) return 2;
    const int userId=q.value(0).toInt();
    q.exec(QString("UPDATE user SET balance=10000 WHERE id=%1").arg(userId));
    if(!q.exec("SELECT id,power FROM pile WHERE status=0 AND power>0 LIMIT 1")||!q.next()) return 3;
    const int pileId=q.value(0).toInt(); const double rated=q.value(1).toDouble();
    auto result=ChargingEngine::startCharging(userId,pileId,TargetNone,0,QString());
    if(!result.ok) {qCritical()<<result.error;return 4;}
    double previous=0,low=1e9,high=0;
    for(int i=0;i<12;++i) {
        const auto before=OrderDao::getById(result.order.id);
        const double expected=ChargingPowerModel::averageKw(rated,result.order.id,i+.5,before.energy);
        QMetaObject::invokeMethod(&ChargingEngine::instance(),"onTick",Qt::DirectConnection);
        const auto order=OrderDao::getById(result.order.id);
        if(order.energy<previous || std::abs(order.energy-before.energy-expected/60)>0.0011) return 5;
        if(std::abs(order.amount-order.energy*order.priceSnapshot)>0.02) return 6;
        const double delta=order.energy-previous; low=qMin(low,delta);high=qMax(high,delta);previous=order.energy;
    }
    if(high-low<0.001) return 7;
    auto settled=ChargingEngine::instance().settleOrder(result.order.id,FinishByUser,"UI test");
    if(!settled.ok) return 8;
    TcpServer server;
    server.setProxy(QNetworkProxy::NoProxy);
    if (!server.listen(QHostAddress::AnyIPv4, 0)) return 12;
    QTcpSocket client;
    client.setProxy(QNetworkProxy::NoProxy);
    client.connectToHost(QHostAddress::LocalHost, server.serverPort());
    if (!client.waitForConnected(1000)) return 13;
    client.write(QJsonDocument(QJsonObject{{"type", Protocol::ReqUserLogin}, {"phone", "13800000001"}}).toJson(QJsonDocument::Compact) + '\n');
    QElapsedTimer wait;
    wait.start();
    while (!client.canReadLine() && wait.elapsed() < 3000) QTest::qWait(10);
    const auto login = QJsonDocument::fromJson(client.readLine()).object();
    if (!login.value("ok").toBool() || login.value("userId").toInt() <= 0) return 14;
    client.disconnectFromHost();
    QTest::qWait(100);
    AdminMainWindow window("测试数据库",QString());
    window.showConnectionInfo(&server);
    auto *addresses = window.findChild<QComboBox*>("lanAddressCombo");
    if (!addresses || addresses->count() < 1) return 15;
    if (!addresses->currentData().toString().endsWith(":" + QString::number(server.serverPort()))) return 16;
    auto *panel = window.findChild<QWidget*>("lanConnectionPanel");
    for (auto *button : panel->findChildren<QPushButton*>()) {
        if (button->text() == "复制地址") button->click();
    }
    if (QApplication::clipboard()->text() != addresses->currentData().toString()) return 17;
    window.resize(1200,820);window.show();
    auto *nav=window.findChild<QListWidget*>("navList");
    for(int i=0;i<nav->count();++i) {
        nav->setCurrentRow(i);QTest::qWait(500);
        if(!window.grab().save(QString("/tmp/charging-admin-%1.png").arg(i))) return 9;
    }
    for(auto *table:window.findChildren<QTableWidget*>()) {
        if(!table->horizontalHeader()->stretchLastSection()) return 10;
        if(table->isVisible() && table->horizontalHeader()->length()<table->viewport()->width()-2) return 11;
    }
    if (window.findChildren<QFrame*>("adminDataCard").size() != window.findChildren<QTableWidget*>().size()) {
        for (auto *table:window.findChildren<QTableWidget*>()) qCritical()<<table->objectName()<<table->property("cardDecorated");
        return 19;
    }
    DonutChart donut(8,12,3);
    for (const QSize size : {QSize(260,520),QSize(700,300),QSize(360,360)}) {
        donut.resize(size); donut.show(); QTest::qWait(450);
        const auto ring=donut.ringRect();
        if (qAbs(ring.width()-ring.height())>.01 || !QRectF(donut.rect()).contains(ring)) return 20;
        if (!donut.grab().save(QString("/tmp/charging-donut-%1x%2.png").arg(size.width()).arg(size.height()))) return 21;
    }
    nav->setCurrentRow(1);nav->setCurrentRow(2);nav->setCurrentRow(4);QTest::qWait(300);
    if (!window.findChildren<QWidget*>("motionOverlay").isEmpty()) return 22;
    server.close();
    for (auto *button : panel->findChildren<QPushButton*>()) {
        if (button->text() == "刷新") button->click();
    }
    if (!window.findChild<QLabel*>("lanStatusLabel")->text().contains("未启动")) return 18;
    qInfo()<<"PASS: real TCP login, LAN address/port, clipboard, listener failure, variable energy, billing consistency, settlement, six admin pages and tables";
    return 0;
}
