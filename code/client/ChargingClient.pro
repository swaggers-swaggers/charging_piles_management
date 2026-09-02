#-------------------------------------------------
# 用户客户端 ChargingClient
# 手机端交互体验: 免密登录/充电站查询/导航/充电
# 注意: 客户端不直接访问数据库, 业务全部经 Socket 与服务端交互
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 阶段 2 视需要启用: QT += webenginewidgets

TARGET   = ChargingClient
TEMPLATE = app
CONFIG   += c++17

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD $$PWD/pages $$PWD/network
include(../common/common.pri)

SOURCES += \
    main.cpp \
    ClientSession.cpp \
    LoginDialog.cpp \
    UserMainWindow.cpp \
    pages/NearbyStationsPage.cpp \
    pages/NavigationPage.cpp \
    pages/UserInfoPage.cpp \
    pages/ChargingPage.cpp \
    network/TcpClient.cpp \
    network/TcpClientWorker.cpp

HEADERS += \
    ClientSession.h \
    LoginDialog.h \
    UserMainWindow.h \
    pages/NearbyStationsPage.h \
    pages/NavigationPage.h \
    pages/UserInfoPage.h \
    pages/ChargingPage.h \
    network/TcpClient.h \
    network/TcpClientWorker.h

FORMS += \
    LoginDialog.ui

RESOURCES += ../resources/res.qrc
