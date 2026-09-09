QT += core gui widgets network
TEMPLATE = app
TARGET = appoint_msg_test
CONFIG += c++17
CONFIG -= app_bundle
INCLUDEPATH += ../.. ../../client ../../common
SOURCES += test_main.cpp \
           ../../client/MessageCenter.cpp \
           ../../client/network/TcpClient.cpp \
           ../../client/network/TcpClientWorker.cpp \
           ../../client/ClientSession.cpp
HEADERS += \
           ../../client/MessageCenter.h \
           ../../client/network/TcpClient.h \
           ../../client/network/TcpClientWorker.h \
           ../../client/ClientSession.h

busy_test.target = busy_test
busy_test.depends = sub_srcs
