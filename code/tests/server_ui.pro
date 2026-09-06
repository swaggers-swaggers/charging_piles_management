QT += widgets network sql testlib
CONFIG += c++17
TEMPLATE = app
TARGET = server_ui_smoke
qtHaveModule(charts) {
    QT += charts
    DEFINES += HAVE_QTCHARTS
}
INCLUDEPATH += ../server ../server/pages ../server/network ../server/dao ../common
SOURCES += server_ui_smoke.cpp \
    $$files($$PWD/../server/*.cpp) \
    $$files($$PWD/../server/pages/*.cpp) \
    $$files($$PWD/../server/dao/*.cpp) \
    $$files($$PWD/../server/network/*.cpp)
SOURCES -= $$PWD/../server/main.cpp
HEADERS += $$files($$PWD/../server/*.h) \
    $$files($$PWD/../server/pages/*.h) \
    $$files($$PWD/../server/network/*.h)
FORMS += ../server/AdminLoginDialog.ui
RESOURCES += ../resources/res.qrc ../server/server_web.qrc
