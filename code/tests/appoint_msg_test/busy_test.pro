QT += core gui widgets network
TEMPLATE = app
TARGET = busy_test
CONFIG += c++17
CONFIG -= app_bundle
INCLUDEPATH += ../.. ../../client ../../common
SOURCES += busy_test.cpp \
           ../../client/network/TcpClient.cpp \
           ../../client/network/TcpClientWorker.cpp \
           ../../client/ClientSession.cpp
HEADERS += \
           ../../client/network/TcpClient.h \
           ../../client/network/TcpClientWorker.h \
           ../../client/ClientSession.h
