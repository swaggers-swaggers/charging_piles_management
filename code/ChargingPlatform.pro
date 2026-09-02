#-------------------------------------------------
# 东软电动汽车充电桩应用管理平台
# 框架阶段: 登录系统(SQLite) + 管理端/用户端主界面骨架
# 环境: Ubuntu 22.04 / Qt 5.15 / Qt Creator 6.2+
#-------------------------------------------------

QT       += core gui sql

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

# 后续阶段按需启用(需系统安装对应开发包):
# QT += network            # Socket 通信
# QT += charts             # QChart 营收趋势图表
# QT += webenginewidgets   # 腾讯地图导航页面

TARGET   = ChargingPlatform
TEMPLATE = app
CONFIG   += c++11

DEFINES += QT_DEPRECATED_WARNINGS

# 统一头文件搜索路径, 源码中可直接 #include "DatabaseManager.h" 等
INCLUDEPATH += src/common \
               src/login \
               src/admin \
               src/admin/pages \
               src/user \
               src/user/pages

SOURCES += \
    src/main.cpp \
    src/common/DatabaseManager.cpp \
    src/common/Session.cpp \
    src/login/LoginDialog.cpp \
    src/admin/AdminMainWindow.cpp \
    src/admin/pages/SalesPage.cpp \
    src/admin/pages/PileStatusPage.cpp \
    src/admin/pages/PileManagePage.cpp \
    src/admin/pages/StationManagePage.cpp \
    src/admin/pages/UserManagePage.cpp \
    src/user/UserMainWindow.cpp \
    src/user/pages/NearbyStationsPage.cpp \
    src/user/pages/NavigationPage.cpp \
    src/user/pages/UserInfoPage.cpp \
    src/user/pages/ChargingPage.cpp

HEADERS += \
    src/common/DatabaseManager.h \
    src/common/Session.h \
    src/login/LoginDialog.h \
    src/admin/AdminMainWindow.h \
    src/admin/pages/SalesPage.h \
    src/admin/pages/PileStatusPage.h \
    src/admin/pages/PileManagePage.h \
    src/admin/pages/StationManagePage.h \
    src/admin/pages/UserManagePage.h \
    src/user/UserMainWindow.h \
    src/user/pages/NearbyStationsPage.h \
    src/user/pages/NavigationPage.h \
    src/user/pages/UserInfoPage.h \
    src/user/pages/ChargingPage.h

RESOURCES += resources/res.qrc
