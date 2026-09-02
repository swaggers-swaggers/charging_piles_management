#-------------------------------------------------
# 服务端 ChargingServer
# 管理后台 + TCP 服务端(处理客户端业务) + 数据库唯一持有者
#-------------------------------------------------

QT       += core gui sql network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 阶段 3 启用: QT += charts   (营收趋势图表, 需安装 libqt5charts5-dev)

TARGET   = ChargingServer
TEMPLATE = app
CONFIG   += c++17

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD $$PWD/pages $$PWD/network
include(../common/common.pri)

SOURCES += \
    main.cpp \
    DatabaseManager.cpp \
    ServerSession.cpp \
    AdminLoginDialog.cpp \
    AdminMainWindow.cpp \
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
    pages/SalesPage.h \
    pages/PileStatusPage.h \
    pages/PileManagePage.h \
    pages/StationManagePage.h \
    pages/UserManagePage.h \
    network/TcpServer.h \
    network/ClientHandler.h

RESOURCES += ../resources/res.qrc
