QT += core gui widgets network
TEMPLATE = app
TARGET = uipage_test
CONFIG += c++17
CONFIG -= app_bundle
INCLUDEPATH += ../.. ../../client ../../common
SOURCES += test_main.cpp \
           ../../client/pages/UserInfoPage.cpp \
           ../../client/network/TcpClient.cpp \
           ../../client/network/TcpClientWorker.cpp \
           ../../client/ClientSession.cpp
HEADERS += \
           ../../client/pages/UserInfoPage.h \
           ../../client/network/TcpClient.h \
           ../../client/network/TcpClientWorker.h \
           ../../client/ClientSession.h
