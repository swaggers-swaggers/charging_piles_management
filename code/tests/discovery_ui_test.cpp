#include "UiMotion.h"
#include "ChargingParticles.h"
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
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <QDialog>
#include <QStackedWidget>
#include <QTableWidget>
#include <QHeaderView>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QSettings>
#include <QDoubleSpinBox>
#include "MessageCenter.h"
#include "MessagePage.h"
#include "UserInfoPage.h"
#include "TcpClient.h"
#include "protocol.h"
#include "AppTheme.h"
#include "ChargingPowerModel.h"
#include "UserMainWindow.h"
#include "HomePage.h"
#include "ClientSession.h"
#include "NearbyStationsPage.h"
#include "NavigationPage.h"
#include "ChargingPage.h"
#include "OrderHistoryPage.h"

class DiscoveryTest : public QObject {
    Q_OBJECT
    QTemporaryDir settingsDir;
    QTcpServer server;
    UserMainWindow *window = nullptr;
    bool failed = false;
    bool activeOrder = false;
    bool reservationCardActive = false;
    int lastPileStation = -1;
    int stationRequestCount = 0;
    double lastLon = 0;
    double lastLat = 0;
    int replyDelay = 0;
    QJsonArray stations() {
        QJsonArray result;
        const QStringList names{"中关村绿色能源站", "五道口城市快充站", "学院路社区充电站"};
        for (int i=0; i<3; ++i)
            result.append(QJsonObject{{"id", 11+i}, {"name", names[i]}, {"address", "北京市海淀区 · 停车场地面入口"},
                {"longitude",116.31+i*.01},{"latitude",39.96},{"price",1.2-i*.1},{"distance",1.3+i},
                {"totalPiles",i==2?0:6},{"idlePiles",i==0?3:0},{"predictIdle",.5}});
        return result;
    }
    QJsonArray orders() {
        return QJsonArray{
            QJsonObject{{"orderId",102},{"pileCode","HD-DC-08"},{"stationName","中关村绿色能源站"},
                {"startTime","2026-09-08 18:06:00"},{"endTime","2026-09-08 18:48:00"},
                {"energy",23.88},{"amount",28.65},{"priceSnapshot",1.20},{"simMinutes",42},{"status",OrderFinished}},
            QJsonObject{{"orderId",101},{"pileCode","WDK-SC-03"},{"stationName","五道口城市快充站"},
                {"startTime","2026-09-08 21:32:00"},{"endTime",""},
                {"energy",8.46},{"amount",9.31},{"priceSnapshot",1.10},{"simMinutes",17},{"status",OrderCharging}},
            QJsonObject{{"orderId",99},{"pileCode","XYL-06"},{"stationName","学院路社区充电站"},
                {"startTime","2026-09-07 09:20:00"},{"endTime","2026-09-07 09:36:00"},
                {"energy",5.40},{"amount",6.48},{"priceSnapshot",1.20},{"simMinutes",16},
                {"refundAmount",2.00},{"status",OrderAbnormal}}
        };
    }
    QJsonArray reservations() {
        return QJsonArray{
            QJsonObject{{"reservationId",21},{"type",ReserveAppoint},{"pileCode","HD-DC-02"},
                {"stationName","中关村绿色能源站"},{"createTime","2026-09-08 20:00:00"},
                {"reserveDate","2026-09-09"},{"reserveStart","09:00"},{"reserveEnd","10:00"},
                {"status",reservationCardActive ? ReservationActive : ReservationFulfilled}},
            QJsonObject{{"reservationId",18},{"type",ReserveQueue},{"pileCode","WDK-SC-03"},
                {"stationName","五道口城市快充站"},{"createTime","2026-09-08 19:20:00"},
                {"queuePos",2},{"status",ReservationFulfilled}}
        };
    }
    QPushButton *button(QWidget *parent, const QString &text) {
        for (auto *b : parent->findChildren<QPushButton*>()) if (b->text()==text) return b;
        return nullptr;
    }
    QPushButton *accessibleButton(QWidget *parent, const QString &name) {
        for (auto *b : parent->findChildren<QPushButton*>())
            if (b->accessibleName()==name) return b;
        return nullptr;
    }
private slots:
    void interactionEffects() {
        QWidget root;
        root.resize(340,180);
        UiMotion::install(&root);
        root.show();
        // Created after installation: dynamic buttons must receive the same feedback.
        auto *b=new QPushButton("dynamic",&root); b->setGeometry(20,20,180,40); b->show();
        const QRect originalGeometry=b->geometry();
        QEvent enterEvent(QEvent::Enter);
        QApplication::sendEvent(b,&enterEvent); QTest::qWait(220);
        auto *scaleEffect=dynamic_cast<UiMotion::MicroInteractionEffect*>(b->graphicsEffect());
        QVERIFY(scaleEffect);
        QVERIFY(scaleEffect->scaleFactor()>1.03);
        QVERIFY(!b->findChild<QWidget*>("hoverOverlay"));
        QCOMPARE(b->geometry(),originalGeometry); // 绘制级缩放不应扰动布局
        QTest::mousePress(b,Qt::LeftButton,Qt::NoModifier,QPoint(12,15));
        auto *ripple=b->findChild<QWidget*>("motionOverlay");
        QVERIFY(ripple);
        QCOMPARE(ripple->property("rippleOrigin").toPointF(),QPointF(12,15));
        QTest::mouseRelease(b,Qt::LeftButton,Qt::NoModifier,QPoint(12,15));
        auto *edit=new QLineEdit(&root); edit->setGeometry(20,80,180,40); edit->show();
        root.activateWindow(); edit->setFocus(); QTest::qWait(30);
        QVERIFY(edit->findChild<QWidget*>("focusBreathingOverlay"));
        b->setFocus(); QTest::qWait(30);
        QVERIFY(!edit->findChild<QWidget*>("focusBreathingOverlay"));
        QTest::qWait(500);
        QVERIFY(!b->findChild<QWidget*>("motionOverlay"));
    }
    void dialogChromeAndParticles() {
        QDialog dialog;
        auto *layout=new QVBoxLayout(&dialog);
        auto *input=new QLineEdit(&dialog); layout->addWidget(input);
        QTimer::singleShot(100,&dialog,[&]{
            QVERIFY(dialog.windowFlags().testFlag(Qt::FramelessWindowHint));
            QVERIFY(dialog.findChild<QWidget*>("customTitleBar"));
            QVERIFY(dialog.testAttribute(Qt::WA_TranslucentBackground));
            QCOMPARE(dialog.contentsMargins(), QMargins(0,0,0,0));
            QVERIFY(input->isVisible());
            dialog.accept();
        });
        QCOMPARE(dialog.exec(),int(QDialog::Accepted));
        ChargingParticles particles; particles.resize(400,300); particles.show();
        QVERIFY(particles.animationRunning());
        QTest::qWait(80); const auto frame=particles.grab().toImage();
        QTest::qWait(80); QVERIFY(frame!=particles.grab().toImage());
        particles.hide(); QVERIFY(!particles.animationRunning());
        particles.show(); QVERIFY(particles.animationRunning());
    }
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
        QVERIFY(settingsDir.isValid());
        QSettings::setDefaultFormat(QSettings::IniFormat);
        QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settingsDir.path());
        QCoreApplication::setOrganizationName("ChargingUiTests");
        QCoreApplication::setApplicationName("Isolated");
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
                    if(type==Protocol::ReqGetUserInfo) { reply["nickname"]="体验用户"; reply["balance"]=128.50; }
                    if(type==Protocol::ReqVehicleList) { reply["vehicles"]=QJsonArray{
                        QJsonObject{{"vehicleId",2},{"plateNumber","京A·E6288"},{"brandModel","Tesla Model 3"},
                            {"energyType","纯电"},{"batteryCapacity",60.0},{"isDefault",true}},
                        QJsonObject{{"vehicleId",1},{"plateNumber","京AD·30917"},{"brandModel","蔚来 ET5 Touring"},
                            {"energyType","纯电"},{"batteryCapacity",75.0},{"isDefault",false}}}; }
                    if(type==6) { ++stationRequestCount; reply["stations"]=stations(); lastLon=req["lon"].toDouble(); lastLat=req["lat"].toDouble();
                        if(failed) { reply["ok"]=false; reply["error"]="test unavailable"; } }
                    if(type==7) { lastPileStation=req["stationId"].toInt();
                        reply["piles"]=QJsonArray{QJsonObject{{"id",101},{"stationId",lastPileStation},{"code","DC-01"},
                            {"type",0},{"power",120},{"status",lastPileStation==12?1:0}}}; }
                    if(type==8) { reply["hasOrder"]=activeOrder;
                        reply["order"]=QJsonObject{{"id",99},{"pileId",101},{"pileCode","DC-01"},{"stationName","正在充电的站点"},{"status",0}}; }
                    if(type==Protocol::ReqOrderHistory) { reply["orders"]=orders(); reply["total"]=orders().size(); }
                    if(type==Protocol::ReqOrderDetail) {
                        const int id=req["orderId"].toInt();
                        for(const auto &value:orders()) if(value.toObject()["orderId"].toInt()==id) reply["order"]=value;
                    }
                    if(type==Protocol::ReqMyReservations) reply["reservations"]=reservations();
                    const auto bytes = QJsonDocument(reply).toJson(QJsonDocument::Compact)+'\n';
                    QTimer::singleShot(replyDelay, socket, [socket, bytes] { socket->write(bytes); });
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
        ClientSession::instance().phone="138****8000";
        ClientSession::instance().balance=128.50;
        window=new UserMainWindow;
        window->resize(1200,820); window->show();
        auto *nav=window->findChild<QListWidget*>("navList");
        QCOMPARE(nav->currentRow(),0);
        QCOMPARE(nav->count(),6);
        auto *home=window->findChild<HomePage*>();
        QVERIFY(home);
        QCOMPARE(home->findChildren<QPushButton*>("bentoCard").size(),6);
        QVERIFY(home->findChild<QFrame*>("homeMapCard"));
        QVERIFY(home->findChild<MapCanvas*>("homeMapCanvas"));
        QVERIFY(accessibleButton(home,"附近充电站"));
        accessibleButton(home,"附近充电站")->click();
        QCOMPARE(nav->currentRow(),1);
        QTRY_COMPARE(window->findChildren<QFrame*>("stationCard").size(),3);
        auto *pageTimer = window->findChild<QTimer *>("pageAutoRefreshTimer");
        QVERIFY(pageTimer);
        QVERIFY(pageTimer->isActive());
        QCOMPARE(pageTimer->interval(), 5000);
        int refreshablePages = 0;
        for (auto *page : window->findChildren<QWidget *>())
            if (page->metaObject()->indexOfMethod("refreshPage()") >= 0) ++refreshablePages;
        QCOMPARE(refreshablePages, 6);
        const int requestsBeforeTick = stationRequestCount;
        QVERIFY(QMetaObject::invokeMethod(pageTimer, "timeout", Qt::DirectConnection));
        QTRY_VERIFY(stationRequestCount > requestsBeforeTick);
    }
    void homepageAndFilters() {
        auto *nav=window->findChild<QListWidget*>("navList");
        QCOMPARE(nav->count(),6); QCOMPARE(nav->item(0)->text(),QString("首页"));
        QCOMPARE(nav->item(1)->text(),QString("附近充电站"));
        nav->setCurrentRow(0); QTest::qWait(20);
        auto *home=window->findChild<HomePage*>();
        auto *stationPreview=accessibleButton(home,"附近充电站");
        QTRY_VERIFY(stationPreview->findChildren<QLabel*>("homeDetailLine").first()->text().contains("中关村"));
        auto *orderPreview=accessibleButton(home,"预约与订单");
        QTRY_VERIFY(orderPreview->findChildren<QLabel*>("homeDetailLine").first()->text().contains("kWh"));
        auto *hero=accessibleButton(home,"今日补能概览");
        QVERIFY(hero);
        QVERIFY(hero->geometry().width() > stationPreview->geometry().width());
        auto *mapCard=home->findChild<QFrame*>("homeMapCard");
        auto *powerCard=accessibleButton(home,"实时充电功率");
        QVERIFY(mapCard && powerCard);
        QVERIFY(mapCard->geometry().width()>powerCard->geometry().width());
        QTRY_VERIFY(home->findChild<QLabel*>("homeMapBadge")->text().contains("3 站"));
        emit TcpClient::instance().pushReceived(QJsonObject{
            {"type",Protocol::PushOrderProgress},{"orderId",99},{"energy",12.4},
            {"amount",14.88},{"minutes",18},{"power",86.5}});
        QTRY_COMPARE(home->findChild<QLabel*>("homePowerValue")->text(),QString("86.5 kW"));
        QTest::mouseMove(hero,hero->rect().center()); QTest::qWait(220);
        QVERIFY(hero->graphicsEffect());
        QVERIFY(window->grab().save("/tmp/charging-bento-home.png"));
        for (auto *area : window->findChildren<QScrollArea*>()) {
            if (area->widget()!=home) continue;
            area->verticalScrollBar()->setValue(area->verticalScrollBar()->maximum());
            QTest::qWait(20);
            QVERIFY(window->grab().save("/tmp/charging-bento-home-details.png"));
            area->verticalScrollBar()->setValue(0);
        }
        window->resize(800,600); QTest::qWait(40);
        QVERIFY(mapCard->geometry().width()>500);
        QVERIFY(powerCard->geometry().width()>500);
        QVERIFY(window->grab().save("/tmp/charging-bento-home-compact.png"));
        window->resize(1200,820); QTest::qWait(20);
        accessibleButton(home,"附近充电站")->click();
        QCOMPARE(nav->currentRow(),1);
        const auto cards=window->findChildren<QFrame*>("stationCard");
        QVERIFY(!button(cards.last(),"预约时段")->isEnabled());
        auto *page=window->findChild<NearbyStationsPage*>();
        QVERIFY(!page->findChild<QLineEdit*>("addrEdit"));
        QCOMPARE(page->findChildren<QLineEdit*>().size(),1);
        auto *search=page->findChildren<QLineEdit*>().constFirst();
        auto *region=page->findChild<QComboBox*>("regionCombo");
        auto *idle=page->findChild<QCheckBox*>();
        auto *sort=page->findChild<QComboBox*>("stationSort");
        QVERIFY(region && idle && sort);
        const int rowY=search->geometry().center().y();
        QVERIFY(qAbs(region->geometry().center().y()-rowY)<=2);
        QVERIFY(qAbs(idle->geometry().center().y()-rowY)<=2);
        QVERIFY(qAbs(sort->geometry().center().y()-rowY)<=2);
        search->setText("五道口"); QTest::qWait(10);
        QCOMPARE(page->findChildren<QFrame*>("stationCard").size(),1);
        QVERIFY(button(page,"预约时段"));
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
        button(window->findChild<NearbyStationsPage*>(),"预约时段")->click();
        auto *combo=window->findChild<QComboBox*>("stationCombo");
        QCOMPARE(combo->currentData().toInt(),12); QCOMPARE(lastPileStation,12);
        auto *page=window->findChild<ChargingPage*>();
        QVERIFY(button(page,"预约时段"));
        button(page,"刷新")->click(); QCOMPARE(combo->currentData().toInt(),12);
        auto *stationSearch=page->findChild<QLineEdit*>("chargingStationSearch");
        QVERIFY(stationSearch);
        stationSearch->setText("学院路"); QTest::qWait(300);
        QCOMPARE(combo->count(),1); QCOMPARE(combo->currentData().toInt(),13);
        stationSearch->setText("不存在的站点"); QTest::qWait(300);
        QCOMPARE(combo->count(),0);
        QVERIFY(page->findChild<QLabel*>("emptyState"));
        stationSearch->clear(); QTest::qWait(300);
        QCOMPARE(combo->count(),3);
        QVERIFY(window->grab().save("/tmp/charging-pile-selection.png"));
    }
    void chargeSetupHasNoFreeze() {
        auto *page=window->findChild<ChargingPage*>();
        auto *combo=window->findChild<QComboBox*>("stationCombo");
        combo->setCurrentIndex(combo->findData(11));
        QTRY_VERIFY(button(page,"立即充电"));
        QTimer::singleShot(80,this,[]{
            auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QString labels;
            for (QLabel *label : dialog->findChildren<QLabel*>())
                labels += label->text();
            QVERIFY(!labels.contains("预授权"));
            QVERIFY(!labels.contains("冻结"));
            QVERIFY(labels.contains("实际充电量"));
            QVERIFY(labels.contains("余额用完后自动停止"));
            QVERIFY(dialog->grab().save("/tmp/charging-setup-no-freeze.png"));
            dialog->reject();
        });
        button(page,"立即充电")->click();
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
    void navigationWhileRequestPending() {
        auto *nearby = window->findChild<NearbyStationsPage*>();
        for (int attempt = 0; attempt < 2; ++attempt) {
            replyDelay = 120;
            QTimer::singleShot(10, nearby, [nearby] {
                emit nearby->navigationRequested(12, 116.461, 39.9087);
            });
            const auto reply = TcpClient::instance().request(Protocol::ReqStationList);
            replyDelay = 0;
            QVERIFY(reply.value("ok").toBool());
            auto *dialog = window->findChild<QDialog*>();
            QVERIFY(dialog);
            QVERIFY(!dialog->testAttribute(Qt::WA_TranslucentBackground));
            QVERIFY(!dialog->windowFlags().testFlag(Qt::FramelessWindowHint));
            QVERIFY(!dialog->findChild<QWidget*>("customTitleBar"));
            auto *page = dialog->findChild<NavigationPage*>();
            QVERIFY(page);
            auto *dest = page->findChild<QComboBox*>("destCombo");
            QTRY_COMPARE(dest->currentData().toInt(), 12);
            QCOMPARE(lastLon, 116.461);
            QCOMPARE(lastLat, 39.9087);
            QVERIFY(dialog->findChildren<QMessageBox*>().isEmpty());
            QVERIFY(dialog->grab().save("/tmp/charging-navigation-fixed.png"));
            dialog->accept();
            QTRY_VERIFY(window->findChildren<QDialog*>().isEmpty());
        }
    }
    void navigationFailureRetry() {
        NavigationPage page;
        page.setDestination(12, 116.461, 39.9087);
        failed = true;
        page.show();
        auto *status = page.findChild<QLabel*>("navResult");
        QTRY_VERIFY(status->text().contains("站点加载失败"));
        QVERIFY(page.findChildren<QMessageBox*>().isEmpty());
        QVERIFY(!button(&page, "开始导航")->isEnabled());
        failed = false;
        page.findChild<QPushButton*>("navigationRefresh")->click();
        QTRY_COMPARE(page.findChild<QComboBox*>("destCombo")->currentData().toInt(), 12);
        QVERIFY(button(&page, "开始导航")->isEnabled());
    }
    void failureAndRecovery() {
        failed=true;
        window->findChild<QListWidget*>("navList")->setCurrentRow(1); QTest::qWait(30);
        auto *page=window->findChild<NearbyStationsPage*>();
        QCOMPARE(page->findChildren<QFrame*>("stationCard").size(),0);
        QVERIFY(page->findChild<QLabel*>("discoverySummary")->text().contains("失败"));
        failed=false; button(page,"刷新站点")->click(); QTest::qWait(10);
        QCOMPARE(page->findChildren<QFrame*>("stationCard").size(),3);
    }
    void lightPagesAndTables() {
        auto *nav = window->findChild<QListWidget *>("navList");
        for (int i = 0; i < nav->count(); ++i) {
            nav->setCurrentRow(i);
            QTest::qWait(15);
            QVERIFY(window->grab().save(QString("/tmp/charging-page-%1.png").arg(i)));
        }
        for (auto *table : window->findChildren<QTableWidget *>()) {
            QVERIFY(table->horizontalHeader()->stretchLastSection());
            QCOMPARE(table->viewport()->palette().color(QPalette::Base), QColor("#FFFFFF"));
        }
        nav->setCurrentRow(1); QTest::qWait(15);
    }
    void orderJourneyCards() {
        reservationCardActive=true;
        auto *nav=window->findChild<QListWidget*>("navList");
        nav->setCurrentRow(3); QTest::qWait(80);
        auto *page=window->findChild<OrderHistoryPage*>();
        QVERIFY(page);
        QVERIFY(page->findChildren<QTableWidget*>().isEmpty());
        QCOMPARE(page->findChildren<QFrame*>("orderJourneyCard").size(),3);
        QVERIFY(page->findChild<QLabel*>("orderSummary")->text().contains("3 笔旅程"));
        QVERIFY(button(page,"查看票据  ↗"));
        QVERIFY(window->grab().save("/tmp/charging-orders-cards.png"));
        QTimer::singleShot(80,this,[]{
            auto *dialog=qobject_cast<QDialog*>(QApplication::activeModalWidget());
            QVERIFY(dialog);
            QVERIFY(dialog->findChild<QFrame*>("orderReceiptHero"));
            QVERIFY(dialog->findChild<QLabel*>("orderReceiptAmount"));
            QVERIFY(dialog->grab().save("/tmp/charging-order-receipt.png"));
            dialog->accept();
        });
        button(page,"查看票据  ↗")->click();
        auto *tabs=page->findChild<QTabWidget*>("orderTabs");
        tabs->setCurrentIndex(1); QTest::qWait(40);
        QCOMPARE(page->findChildren<QFrame*>("reservationTicket").size(),1);
        QVERIFY(window->grab().save("/tmp/charging-reservation-cards.png"));
        reservationCardActive=false;
        tabs->setCurrentIndex(0);
        window->resize(800,600); QTest::qWait(30);
        QVERIFY(window->grab().save("/tmp/charging-orders-cards-compact.png"));
        window->resize(1200,820);
    }
    void accountAndMessageCards() {
        auto *nav = window->findChild<QListWidget*>("navList");
        nav->setCurrentRow(5); QTest::qWait(250);
        auto *account = window->findChild<UserInfoPage*>();
        QCOMPARE(account->findChild<QLabel*>("walletAmount")->text(),QString("128.50"));
        QTRY_COMPARE(account->findChildren<QFrame*>("vehicleCard").size(),2);
        QCOMPARE(account->findChildren<QLabel*>("vehicleDefaultBadge").size(),1);
        button(account,"200 元")->click();
        QCOMPARE(account->findChild<QDoubleSpinBox*>("rechargeSpin")->value(),200.0);
        QVERIFY(window->grab().save("/tmp/charging-account-redesign.png"));
        nav->setCurrentRow(4); QTest::qWait(250);
        auto *page = window->findChild<MessagePage*>();
        QVERIFY(window->grab().save("/tmp/charging-messages-empty.png"));
        const int before = MessageCenter::instance().unreadCount();
        QTimer popupCloser;
        connect(&popupCloser,&QTimer::timeout,this,[]{
            for(auto *w : QApplication::topLevelWidgets())
                if(auto *box=qobject_cast<QMessageBox*>(w)) box->accept();
        });
        popupCloser.start(10);
        for(int event : {2,6,8}) emit TcpClient::instance().pushReceived(QJsonObject{
            {"type",Protocol::PushOrderEvent},{"event",event},
            {"message",event==2 ? "本次充电已完成，费用已按实际用量结算，可前往我的订单查看电量与费用明细。" : "服务状态已更新，请查看最新通知。"}});
        popupCloser.stop();
        auto *list = page->findChild<QListWidget*>("messageList");
        QTRY_COMPARE(list->count(),3);
        QCOMPARE(MessageCenter::instance().unreadCount(),before+3);
        QVERIFY(list->visualItemRect(list->item(0)).height()>=120);
        QVERIFY(window->grab().save("/tmp/charging-messages-redesign.png"));
        QTest::mouseClick(list->viewport(),Qt::LeftButton,Qt::NoModifier,list->visualItemRect(list->item(0)).center());
        QTRY_COMPARE(MessageCenter::instance().unreadCount(),before+2);
        page->findChild<QComboBox*>("messageFilter")->setCurrentIndex(1);
        QCOMPARE(list->count(),2);
        button(page,"清空已读")->click(); QTest::qWait(20);
        QCOMPARE(list->count(),2);
        window->resize(800,600); QTest::qWait(20);
        QVERIFY(window->grab().save("/tmp/charging-messages-compact.png"));
        nav->setCurrentRow(5); QTest::qWait(20);
        QVERIFY(window->grab().save("/tmp/charging-account-compact.png"));
        window->resize(1200,820);
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
        QTest::qWait(250);
        QVERIFY(window->grab().save("/tmp/charging-active-particles.png"));
    }
    void cleanupTestCase() { delete window; }
};
QTEST_MAIN(DiscoveryTest)
#include "discovery_ui_test.moc"
