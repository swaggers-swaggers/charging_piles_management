QT += widgets network testlib
CONFIG += testcase c++17
TEMPLATE = app
TARGET = lan_connection_test
INCLUDEPATH += ../client ../client/network ../common
SOURCES += lan_connection_test.cpp ../client/LoginDialog.cpp ../client/ClientSession.cpp \
    ../client/network/TcpClient.cpp ../client/network/TcpClientWorker.cpp
HEADERS += ../client/LoginDialog.h ../client/network/TcpClient.h ../client/network/TcpClientWorker.h
FORMS += ../client/LoginDialog.ui
RESOURCES += ../resources/res.qrc
