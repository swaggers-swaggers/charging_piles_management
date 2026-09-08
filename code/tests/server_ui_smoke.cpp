#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
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
#include <QSplitter>
#include <QDebug>
#include <QTimer>
#include <QtTest>
#include "AppTheme.h"
#include "DonutChart.h"
#include "AdminMainWindow.h"
#include "DatabaseManager.h"
#include "ChargingEngine.h"
#include "ChargingPowerModel.h"
#include "dao/OrderDao.h"
#include "dao/PileDao.h"
#include "dao/UserDao.h"
#include <QSemaphore>
#include <thread>
int main(int argc,char **argv) {
    QApplication app(argc,argv);
    QPalette dark; dark.setColor(QPalette::Window,Qt::black); dark.setColor(QPalette::Base,Qt::black);
    app.setPalette(dark); AppTheme::apply(app);
    QTemporaryDir temp;
    qputenv("CHARGING_DB", (temp.path()+"/test.db").toUtf8());
    QString error;
    if(!DatabaseManager::instance().init(&error)) {qCritical()<<error;return 1;}
    QSqlQuery q;
    if (!q.exec("SELECT COUNT(*) FROM charge_order") || !q.next()
        || q.value(0).toInt() < 90 || q.value(0).toInt() > 240) return 23;
    if (!q.exec("SELECT COUNT(*) FROM charge_order o LEFT JOIN pile p ON p.id=o.pile_id"
                " WHERE p.id IS NULL OR o.energy<=0 OR o.amount<=0 OR o.sim_minutes NOT BETWEEN 15 AND 89")
        || !q.next() || q.value(0).toInt()!=0) return 24;
    q.finish();
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
    // 第二连接先读到旧进度，再由主连接结算，重现迟到心跳的确定性时序。
    const QString lateConnection = "late-progress-test";
    auto lateDb = QSqlDatabase::addDatabase("QSQLITE", lateConnection);
    lateDb.setDatabaseName(DatabaseManager::instance().databasePath());
    if (!lateDb.open()) return 25;
    const auto stale = OrderDao::getContext(result.order.id, nullptr, lateConnection);
    if (!stale.exists) return 26;
    auto settled=ChargingEngine::instance().settleOrder(result.order.id,FinishByUser,"UI test");
    if(!settled.ok) return 8;
    UserInfo afterSettle;
    if (!UserDao::getById(userId,&afterSettle)) return 27;
    if (std::abs(afterSettle.balance-(10000-settled.order.amount))>0.001) return 28;
    if (OrderDao::updateProgress(result.order.id, stale.energy+10, stale.amount+10,
                                 stale.simMinutes+1, &error, lateConnection)) return 29;
    const auto unchanged = OrderDao::getById(result.order.id);
    if (unchanged.amount!=settled.order.amount || unchanged.energy!=settled.order.energy
        || unchanged.simMinutes!=settled.order.simMinutes || unchanged.status!=settled.order.status) return 30;
    if (ChargingEngine::instance().settleOrder(result.order.id,FinishByUser,"duplicate",lateConnection).ok) return 31;
    UserInfo afterDuplicate;
    if (!UserDao::getById(userId,&afterDuplicate) || afterDuplicate.balance!=afterSettle.balance) return 32;
    lateDb.close(); lateDb=QSqlDatabase(); QSqlDatabase::removeDatabase(lateConnection);

    // 写进度失败时，不得按未落库的数据自动结算。
    auto failOrder=ChargingEngine::startCharging(userId,pileId,TargetMinutes,1,QString());
    if (!failOrder.ok) return 33;
    if (!q.exec("CREATE TEMP TRIGGER reject_progress BEFORE UPDATE OF energy ON charge_order"
                " WHEN NEW.sim_minutes>OLD.sim_minutes BEGIN SELECT RAISE(FAIL,'test write failure'); END")) return 34;
    QMetaObject::invokeMethod(&ChargingEngine::instance(),"onTick",Qt::DirectConnection);
    const auto failedProgress=OrderDao::getById(failOrder.order.id);
    if (!q.exec("DROP TRIGGER reject_progress")) return 35;
    if (failedProgress.status!=OrderCharging || failedProgress.energy!=0 || failedProgress.amount!=0) return 36;
    QMetaObject::invokeMethod(&ChargingEngine::instance(),"onTick",Qt::DirectConnection);
    if (OrderDao::getById(failOrder.order.id).status!=OrderFinished) return 37;
    qInfo()<<"PASS: fresh database seeding, late progress rejection, duplicate settlement and failed-write recovery";
    // 两个工作线程、独立数据库连接，同时为同一用户抢不同的空闲桩。
    q.finish();
    if (!q.exec("SELECT id FROM pile WHERE status=0 LIMIT 2")) return 38;
    QList<int> racePiles;
    while(q.next()) racePiles.append(q.value(0).toInt());
    q.finish();
    if (racePiles.size()!=2) return 39;
    const QString dbPath=DatabaseManager::instance().databasePath();
    for (int round=0; round<20; ++round) {
        if (!q.exec(QString("UPDATE user SET balance=60 WHERE id=%1").arg(userId))) return 40;
        QSemaphore ready, go;
        ChargingEngine::StartResult results[2];
        auto worker=[&](int index) {
            const QString connection=QString("concurrent-start-%1").arg(index);
            {
                auto db=QSqlDatabase::addDatabase("QSQLITE",connection);
                db.setDatabaseName(dbPath);
                const bool opened=db.open();
                { QSqlQuery config(db); config.exec("PRAGMA busy_timeout=3000"); }
                ready.release(); go.acquire();
                if (opened) results[index]=ChargingEngine::startCharging(userId,racePiles[index],TargetAmount,50,connection);
                db.close();
            }
            QSqlDatabase::removeDatabase(connection);
        };
        std::thread first(worker,0), second(worker,1);
        ready.acquire(2); go.release(2); first.join(); second.join();
        if (int(results[0].ok)+int(results[1].ok)!=1) return 41;
        const auto &winner=results[results[0].ok?0:1];
        const auto &loser=results[results[0].ok?1:0];
        if (loser.errorCode!=Protocol::ErrOrderExists) return 42;
        UserInfo account;
        if (!UserDao::getById(userId,&account) || account.balance!=10) return 43;
        if (!q.exec(QString("SELECT COUNT(*) FROM charge_order WHERE user_id=%1 AND status=0").arg(userId))
            || !q.next() || q.value(0).toInt()!=1) return 44;
        q.finish();
        const int loserPile=racePiles[results[0].ok?1:0];
        if (!q.exec(QString("SELECT status FROM pile WHERE id=%1").arg(loserPile))
            || !q.next() || q.value(0).toInt()!=PileIdle) return 45;
        q.finish();
        // 绕过引擎直接插入，也必须被数据库保护拒绝。
        if (q.exec(QString("INSERT INTO charge_order(user_id,pile_id,station_id,status) "
                           "SELECT %1,id,station_id,0 FROM pile WHERE id=%2").arg(userId).arg(loserPile))) return 46;
        if (!ChargingEngine::instance().settleOrder(winner.order.id,FinishByUser,"concurrent test").ok) return 47;
        if (!UserDao::getById(userId,&account) || account.balance!=60) return 48;
    }
    // 冻结失败不能改变余额；校验失败释放事务锁，下一次正常请求可继续。
    if (UserDao::adjustBalance(userId,-61,&error)) return 49;
    if (UserDao::adjustBalance(-1,-1,&error)) return 50;
    if (ChargingEngine::startCharging(userId,-1,TargetAmount,50,QString()).ok) return 51;
    if (!q.exec("CREATE TEMP TRIGGER reject_new_order BEFORE INSERT ON charge_order "
                "BEGIN SELECT RAISE(FAIL,'test insert failure'); END")) return 52;
    if (ChargingEngine::startCharging(userId,racePiles[0],TargetAmount,50,QString()).ok) return 53;
    if (!q.exec("DROP TRIGGER reject_new_order")) return 54;
    UserInfo rollbackUser;
    if (!UserDao::getById(userId,&rollbackUser) || rollbackUser.balance!=60) return 55;
    auto retry=ChargingEngine::startCharging(userId,racePiles[0],TargetAmount,50,QString());
    if (!retry.ok || !ChargingEngine::instance().settleOrder(retry.order.id,FinishByUser,"retry").ok) return 56;
    qInfo()<<"PASS: 20 concurrent starts, single freeze, database duplicate guard, insufficient balance and rollback retry";
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
    auto *pageTimer = window.findChild<QTimer *>("pageAutoRefreshTimer");
    if (!pageTimer || !pageTimer->isActive() || pageTimer->interval() != 5000) return 57;
    int refreshablePages = 0;
    for (auto *page : window.findChildren<QWidget *>())
        if (page->metaObject()->indexOfMethod("refreshPage()") >= 0) ++refreshablePages;
    if (refreshablePages != 5) return 58;
    auto *nav=window.findChild<QListWidget*>("navList");
    if (!nav || nav->count() != 5
        || nav->item(2)->text() != QStringLiteral("充电站与电桩管理")) return 59;
    nav->setCurrentRow(2);
    QTest::qWait(50);
    auto *stationTable = window.findChild<QTableWidget *>("stationTable");
    auto *pileTable = window.findChild<QTableWidget *>("pileTable");
    auto *searchEdit = window.findChild<QLineEdit *>("stationPileSearch");
    auto *statusFilter = window.findChild<QComboBox *>("pileStatusFilter");
    auto *faultButton = window.findChild<QPushButton *>("faultButton");
    auto *stationPileSplitter = window.findChild<QSplitter *>("stationPileSplitter");
    if (!stationTable || !pileTable || !searchEdit || !statusFilter || !faultButton)
        return 60;
    if (!stationPileSplitter || stationPileSplitter->orientation()!=Qt::Horizontal)
        return 76;
    if (statusFilter->findData(PileIdle) < 0 || statusFilter->findData(PileInUse) < 0
        || statusFilter->findData(PileFault) < 0) return 61;

    // 客户端开始充电后，数据库和当前管理页都应立即显示“使用中”。
    if (!q.exec("SELECT p.id,p.station_id,p.code FROM pile p WHERE p.status=0 LIMIT 1")
        || !q.next()) return 62;
    const int livePileId = q.value(0).toInt();
    const int liveStationId = q.value(1).toInt();
    const QString livePileCode = q.value(2).toString();
    q.finish();
    if (!q.exec(QString("UPDATE user SET balance=1000 WHERE id=%1").arg(userId))) return 63;

    auto findDataRow=[](QTableWidget *table, int column, int value) {
        for (int row=0; row<table->rowCount(); ++row) {
            QTableWidgetItem *item=table->item(row,column);
            if (item && item->data(Qt::UserRole).toInt()==value) return row;
        }
        return -1;
    };
    auto findTextRow=[](QTableWidget *table, int column, const QString &value) {
        for (int row=0; row<table->rowCount(); ++row) {
            QTableWidgetItem *item=table->item(row,column);
            if (item && item->text()==value) return row;
        }
        return -1;
    };
    int stationRow=findDataRow(stationTable,0,liveStationId);
    if (stationRow<0) return 64;
    stationTable->selectRow(stationRow);
    QCoreApplication::processEvents();
    const int oldInUse=stationTable->item(stationRow,6)->text().toInt();
    auto liveCharge=ChargingEngine::startCharging(userId,livePileId,TargetNone,0,QString());
    if (!liveCharge.ok || PileDao::getById(livePileId).status!=PileInUse) return 65;
    QCoreApplication::processEvents();
    stationRow=findDataRow(stationTable,0,liveStationId);
    const int livePileRow=findTextRow(pileTable,0,livePileCode);
    if (stationRow<0 || stationTable->item(stationRow,6)->text().toInt()!=oldInUse+1
        || stationTable->item(stationRow,8)->text()!=QStringLiteral("使用中")
        || livePileRow<0 || pileTable->item(livePileRow,3)->text()!=QStringLiteral("使用中"))
        return 66;
    if (!ChargingEngine::instance().settleOrder(liveCharge.order.id,FinishByUser,
                                                 QStringLiteral("状态联动测试")).ok)
        return 67;
    QCoreApplication::processEvents();
    stationRow=findDataRow(stationTable,0,liveStationId);
    const int idlePileRow=findTextRow(pileTable,0,livePileCode);
    if (PileDao::getById(livePileId).status!=PileIdle || stationRow<0
        || stationTable->item(stationRow,6)->text().toInt()!=oldInUse
        || idlePileRow<0 || pileTable->item(idlePileRow,3)->text()!=QStringLiteral("闲置"))
        return 68;

    // 电桩编号搜索、状态筛选和故障/恢复按钮应共同作用于合并页面。
    searchEdit->setText(livePileCode);
    QMetaObject::invokeMethod(searchEdit,"returnPressed",Qt::DirectConnection);
    QCoreApplication::processEvents();
    if (stationTable->rowCount()!=1 || pileTable->rowCount()!=1
        || pileTable->item(0,0)->text()!=livePileCode) return 69;
    searchEdit->clear();
    statusFilter->setCurrentIndex(statusFilter->findData(PileFault));
    QCoreApplication::processEvents();
    for (int row=0; row<pileTable->rowCount(); ++row)
        if (pileTable->item(row,3)->text()!=QStringLiteral("故障")) return 70;
    for (int row=0; row<stationTable->rowCount(); ++row)
        if (stationTable->item(row,7)->text().toInt()<=0) return 71;
    statusFilter->setCurrentIndex(statusFilter->findData(-1));
    QCoreApplication::processEvents();
    stationRow=findDataRow(stationTable,0,liveStationId);
    if (stationRow<0) return 72;
    stationTable->selectRow(stationRow);
    QCoreApplication::processEvents();
    int pileRow=findTextRow(pileTable,0,livePileCode);
    if (pileRow<0) return 73;
    pileTable->selectRow(pileRow);
    QCoreApplication::processEvents();
    auto acceptQuestion=[] {
        for (QWidget *widget : QApplication::topLevelWidgets()) {
            if (auto *box=qobject_cast<QMessageBox *>(widget)) {
                if (QAbstractButton *yes=box->button(QMessageBox::Yes))
                    yes->click();
            }
        }
    };
    QTimer::singleShot(50,acceptQuestion);
    faultButton->click();
    if (PileDao::getById(livePileId).status!=PileFault
        || faultButton->text()!=QStringLiteral("恢复正常")) {
        qCritical()<<"fault toggle failed"<<PileDao::getById(livePileId).status
                   <<faultButton->text()<<faultButton->isEnabled();
        return 74;
    }
    QTimer::singleShot(50,acceptQuestion);
    faultButton->click();
    if (PileDao::getById(livePileId).status!=PileIdle
        || faultButton->text()!=QStringLiteral("设为故障")) return 75;

    for(int i=0;i<nav->count();++i) {
        nav->setCurrentRow(i);QTest::qWait(500);
        if(!window.grab().save(QString("/tmp/charging-admin-%1.png").arg(i))) return 9;
    }
    for(auto *table:window.findChildren<QTableWidget*>()) {
        if(!table->horizontalHeader()->stretchLastSection()) return 10;
        if(table->isVisible() && table->horizontalHeader()->length()<table->viewport()->width()-2) {
            qCritical() << "visible table does not fill card" << table->objectName()
                        << table->horizontalHeader()->length() << table->viewport()->width();
            return 11;
        }
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
    qInfo()<<"PASS: charging state linkage, merged station/pile management, search/status filters, fault toggle, TCP login, auto refresh and five admin pages";
    return 0;
}
