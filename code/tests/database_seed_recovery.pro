QT += core sql
CONFIG += console c++17
CONFIG -= app_bundle
TEMPLATE = app
TARGET = database_seed_recovery

INCLUDEPATH += ../server ../common
SOURCES += database_seed_recovery.cpp \
    ../server/DatabaseManager.cpp
HEADERS += ../server/DatabaseManager.h
RESOURCES += ../server/server_web.qrc
