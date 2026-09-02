#-------------------------------------------------
# 服务端 ChargingServer
# 管理后台 + TCP 服务端(处理客户端业务) + 数据库唯一持有者
#-------------------------------------------------

QT       += core gui sql network

# Qt Charts(销售业绩折线图): 安装了就启用 QChart, 未安装自动降级为自绘折线图
# 安装命令: Qt5 -> sudo apt install libqt5charts5-dev; Qt6 -> sudo apt install libqt6charts6-dev
qtHaveModule(charts) {
    QT += charts
    DEFINES += HAVE_QTCHARTS
}

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 营收趋势图表(Qt Charts): Qt5 需安装 libqt5charts5-dev, Qt6 需安装 libqt6charts6-dev

TARGET   = ChargingServer
TEMPLATE = app
CONFIG   += c++17

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD $$PWD/pages $$PWD/network $$PWD/dao
include(../common/common.pri)

SOURCES += \
    main.cpp \
    DatabaseManager.cpp \
    ServerSession.cpp \
    AdminLoginDialog.cpp \
    AdminMainWindow.cpp \
    Predictor.cpp \
    DataExporter.cpp \
    dao/UserDao.cpp \
    dao/StationDao.cpp \
    dao/PileDao.cpp \
    dao/OrderDao.cpp \
    dao/LogDao.cpp \
    pages/SalesPage.cpp \
    pages/PileStatusPage.cpp \
    pages/PileManagePage.cpp \
    pages/StationManagePage.cpp \
    pages/UserManagePage.cpp \
    network/TcpServer.cpp \
    network/ClientHandler.cpp

HEADERS += \
    DatabaseManager.h \
    ServerSession.h \
    AdminLoginDialog.h \
    AdminMainWindow.h \
    Predictor.h \
    DataExporter.h \
    dao/UserDao.h \
    dao/StationDao.h \
    dao/PileDao.h \
    dao/OrderDao.h \
    dao/LogDao.h \
    pages/SalesPage.h \
    pages/PileStatusPage.h \
    pages/PileManagePage.h \
    pages/StationManagePage.h \
    pages/UserManagePage.h \
    network/TcpServer.h \
    network/ClientHandler.h

FORMS += \
    AdminLoginDialog.ui

RESOURCES += ../resources/res.qrc
