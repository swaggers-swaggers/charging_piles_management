#-------------------------------------------------
# 用户客户端 ChargingClient
# 手机端交互体验: 免密登录/充电站查询/导航/充电/排队预约/订单
# 注意: 客户端不直接访问数据库, 业务全部经 Socket 与服务端交互
#-------------------------------------------------

QT       += core gui network

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 安装 Qt WebEngine 后自动启用 MapLibre/OpenFreeMap 真实底图；
# 未安装时仍可编译，并回退到原生自绘地图。
qtHaveModule(webenginewidgets) {
    QT += webenginewidgets
    DEFINES += CHARGING_HAS_WEBENGINE
} else {
    message("Qt WebEngine 未安装：导航页将使用降级画布，高德外部导航仍可用")
}

TARGET   = ChargingClient
TEMPLATE = app
CONFIG   += c++17

DEFINES += QT_DEPRECATED_WARNINGS

INCLUDEPATH += $$PWD $$PWD/pages $$PWD/network
include(../common/common.pri)

SOURCES += \
    main.cpp \
    ClientSession.cpp \
    MessageCenter.cpp \
    LoginDialog.cpp \
    UserMainWindow.cpp \
    pages/NearbyStationsPage.cpp \
    pages/NavigationPage.cpp \
    pages/UserInfoPage.cpp \
    pages/ChargingPage.cpp \
    pages/ChargeChartWidget.cpp \
    pages/OrderHistoryPage.cpp \
    pages/MessagePage.cpp \
    network/TcpClient.cpp \
    network/TcpClientWorker.cpp

HEADERS += \
    ClientSession.h \
    MessageCenter.h \
    LoginDialog.h \
    UserMainWindow.h \
    pages/NearbyStationsPage.h \
    pages/NavigationPage.h \
    pages/UserInfoPage.h \
    pages/ChargingPage.h \
    pages/ChargeChartWidget.h \
    pages/OrderHistoryPage.h \
    pages/MessagePage.h \
    network/TcpClient.h \
    network/TcpClientWorker.h

FORMS += \
    LoginDialog.ui

RESOURCES += ../resources/res.qrc
