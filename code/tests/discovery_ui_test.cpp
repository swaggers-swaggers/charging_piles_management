#include "UiMotion.h"
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QFile>
#include <QFrame>
#include <QLabel>
#include <QListWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QCheckBox>
#include <QPushButton>
#include <QTimer>
#include <QDialog>
#include <QStackedWidget>
#include <QTableWidget>
#include <QHeaderView>
#include "AppTheme.h"
#include "ChargingPowerModel.h"
#include "UserMainWindow.h"
#include "ClientSession.h"
#include "NearbyStationsPage.h"
#include "NavigationPage.h"
#include "ChargingPage.h"

class DiscoveryTest : public QObject {
    Q_OBJECT
    QTcpServer server;
    UserMainWindow *window = nullptr;
    bool failed = false;
    bool activeOrder = false;
    int lastPileStation = -1;
    double lastLon = 0;
    double lastLat = 0;
    QJsonArray stations() {
        QJsonArray result;
        const QStringList names{"中关村绿色能源站", "五道口城市快充站", "学院路社区充电站"};
        for (int i=0; i<3; ++i)
            result.append(QJsonObject{{"id", 11+i}, {"name", names[i]}, {"address", "北京市海淀区 · 停车场地面入口"},
                {"longitude",116.31+i*.01},{"latitude",39.96},{"price",1.2-i*.1},{"distance",1.3+i},
                {"totalPiles",i==2?0:6},{"idlePiles",i==0?3:0},{"predictIdle",.5}});
        return result;
    }
    QPushButton *button(QWidget *parent, const QString &text) {
        for (auto *b : parent->findChildren<QPushButton*>()) if (b->text()==text) return b;
        return nullptr;
    }
private slots:
    void animationCleanup() {
        QWidget root;
        auto *stack = new QStackedWidget(&root);
        stack->resize(300,200); stack->addWidget(new QWidget); stack->addWidget(new QWidget);
        UiMotion::install(&root); root.show();
        stack->setCurrentIndex(1);stack->setCurrentIndex(0);stack->setCurrentIndex(1);
        QTest::qWait(300);
        QVERIFY(root.findChildren<QWidget*>("motionOverlay").isEmpty());
        QCOMPARE(stack->currentIndex(),1);
    }
    void initTestCase() {
        QVERIFY(server.listen(QHostAddress::LocalHost,0));
        qputenv("CHARGING_SERVER_PORT", QByteArray::number(server.serverPort()));
        qputenv("CHARGING_SERVER_HOST", "127.0.0.1");
        connect(&server, &QTcpServer::newConnection, this, [this] {
            auto *socket = server.nextPendingConnection();
            connect(socket, &QTcpSocket::readyRead, this, [this,socket] {
                while(socket->canReadLine()) {
                    auto req=QJsonDocument::fromJson(socket->readLine()).object();
                    int type=req["type"].toInt();
                    QJsonObject reply{{"type",type},{"ok",true}};
                    if(type==6) { reply["stations"]=stations(); lastLon=req["lon"].toDouble(); lastLat=req["lat"].toDouble();
                        if(failed) { reply["ok"]=false; reply["error"]="test unavailable"; } }
                    if(type==7) { lastPileStation=req["stationId"].toInt();
                        reply["piles"]=QJsonArray{QJsonObject{{"id",101},{"stationId",lastPileStation},{"code","DC-01"},
                            {"type",0},{"power",120},{"status",lastPileStation==12?1:0}}}; }
                    if(type==8) { reply["hasOrder"]=activeOrder;
                        reply["order"]=QJsonObject{{"id",99},{"pileId",101},{"pileCode","DC-01"},{"stationName","正在充电的站点"},{"status",0}}; }
                    if(type==17) reply["reservations"]=QJsonArray{};
                    socket->write(QJsonDocument(reply).toJson(QJsonDocument::Compact)+'\n');
                }
            });
        });
        QPalette dark;
        dark.setColor(QPalette::Window, Qt::black);
        dark.setColor(QPalette::Base, Qt::black);
        qApp->setPalette(dark);
        AppTheme::apply(*qApp);
        QCOMPARE(qApp->palette().color(QPalette::Window), QColor("#F3F7F6"));
        QCOMPARE(qApp->palette().color(QPalette::Base), QColor("#FFFFFF"));
        ClientSession::instance().userId=1;
        ClientSession::instance().nickname="体验用户";
        ClientSession::instance().balance=128.50;
        window=new UserMainWindow;
        window->resize(1200,820); window->show();
        QTRY_COMPARE(window->findChildren<QFrame*>("stationCard").size(),3);
    }
    void homepageAndFilters() {
        auto *nav=window->findChild<QListWidget*>("navList");
        QCOMPARE(nav->count(),5); QCOMPARE(nav->item(0)->text(),QString("附近充电站"));
        const auto cards=window->findChildren<QFrame*>("stationCard");
        QVERIFY(!button(cards.last(),"预约 / 排队")->isEnabled());
        auto *page=window->findChild<NearbyStationsPage*>();
        auto *search=page->findChildren<QLineEdit*>().last();
        search->setText("五道口"); QTest::qWait(10);
        QCOMPARE(page->findChildren<QFrame*>("stationCard").size(),1);
        QVERIFY(button(page,"预约 / 排队"));
        search->clear(); QTest::qWait(10);
        auto *filter=page->findChild<QCheckBox*>(); filter->setChecked(true); QTest::qWait(10);
        QCOMPARE(page->findChildren<QFrame*>("stationCard").size(),1);
        filter->setChecked(false); QTest::qWait(10);
        QVERIFY(window->grab().save("/tmp/charging-home-desktop.png"));
        window->resize(800,600); QTest::qWait(20);
        QCOMPARE(window->height(),600);
        QVERIFY(window->grab().save("/tmp/charging-home-compact.png"));
        window->resize(1200,820);
    }
    void busyStationAndRefresh() {
        button(window->findChild<NearbyStationsPage*>(),"预约 / 排队")->click();
        auto *combo=window->findChild<QComboBox*>("stationCombo");
        QCOMPARE(combo->currentData().toInt(),12); QCOMPARE(lastPileStation,12);
        auto *page=window->findChild<ChargingPage*>();
        QVERIFY(button(page,"排队等待")); QVERIFY(button(page,"预约时段"));
        button(page,"刷新")->click(); QCOMPARE(combo->currentData().toInt(),12);
        QVERIFY(window->grab().save("/tmp/charging-pile-selection.png"));
    }
    void navigationContext() {
        NavigationPage page;
        page.setDestination(12,116.461,39.9087);
        page.show(); QTest::qWait(40);
        QCOMPARE(lastLon,116.461); QCOMPARE(lastLat,39.9087);
        bool selected=false;
        for(auto *c:page.findChildren<QComboBox*>()) if(c->currentText().contains("五道口")) selected=true;
        QVERIFY(selected);
    }
    void failureAndRecovery() {
        failed=true;
        window->findChild<QListWidget*>("navList")->setCurrentRow(0); QTest::qWait(30);
        auto *page=window->findChild<NearbyStationsPage*>();
        QCOMPARE(page->findChildren<QFrame*>("stationCard").size(),0);
        QVERIFY(page->findChild<QLabel*>("discoverySummary")->text().contains("失败"));
        failed=false; button(page,"刷新站点")->click(); QTest::qWait(10);
        QCOMPARE(page->findChildren<QFrame*>("stationCard").size(),3);
    }
    void lightPagesAndTables() {
        auto *nav = window->findChild<QListWidget *>("navList");
        for (int i = 1; i < nav->count(); ++i) {
            nav->setCurrentRow(i);
            QTest::qWait(15);
            QVERIFY(window->grab().save(QString("/tmp/charging-page-%1.png").arg(i)));
        }
        for (auto *table : window->findChildren<QTableWidget *>()) {
            QVERIFY(table->horizontalHeader()->stretchLastSection());
            QCOMPARE(table->viewport()->palette().color(QPalette::Base), QColor("#FFFFFF"));
        }
        nav->setCurrentRow(0); QTest::qWait(15);
    }
    void variablePowerModel() {
        double energy = 0, minimum = 120, maximum = 0;
        for (int minute = 0; minute < 90; ++minute) {
            const double power = ChargingPowerModel::averageKw(120, 99, minute + .5, energy);
            QVERIFY(power >= 0 && power <= 120);
            QCOMPARE(power, ChargingPowerModel::averageKw(120, 99, minute + .5, energy));
            const double previous = energy;
            energy += power / 60.0;
            QVERIFY(energy >= previous);
            if (minute > 4 && minute < 15) { minimum = qMin(minimum, power); maximum = qMax(maximum, power); }
        }
        QVERIFY(maximum - minimum > 5);
        QVERIFY(ChargingPowerModel::averageKw(120, 99, 10, 45) < ChargingPowerModel::averageKw(120, 99, 10, 5));
        QCOMPARE(ChargingPowerModel::averageKw(0, 99, 10, 5), 0.0);
        ChargeChartWidget chart;
        chart.resize(800,260);
        energy=0;
        for (int minute=0; minute<45; ++minute) {
            const double power=ChargingPowerModel::averageKw(120,99,minute+.5,energy);
            energy+=power/60.0;
            chart.addPoint(minute+1,energy,energy*1.2,power);
        }
        chart.show();
        QVERIFY(chart.grab().save("/tmp/charging-power-curve.png"));
    }
    void activeChargeIsPreserved() {
        activeOrder=true;
        button(window->findChild<NearbyStationsPage*>(),"立即充电")->click();
        auto *stack=window->findChild<ChargingPage*>()->findChild<QStackedWidget*>();
        QCOMPARE(stack->currentIndex(),1);
    }
    void cleanupTestCase() { delete window; }
};
QTEST_MAIN(DiscoveryTest)
#include "discovery_ui_test.moc"
