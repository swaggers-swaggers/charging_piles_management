QT += widgets network testlib
CONFIG += testcase c++17
TEMPLATE = app
TARGET = discovery_ui_test
INCLUDEPATH += ../client ../client/pages ../client/network ../common
SOURCES += discovery_ui_test.cpp \
    ../client/ClientSession.cpp ../client/MessageCenter.cpp ../client/UserMainWindow.cpp \
    ../client/pages/HomePage.cpp \
    ../client/pages/NearbyStationsPage.cpp ../client/pages/NavigationPage.cpp \
    ../client/pages/ChargingPage.cpp ../client/pages/ChargeChartWidget.cpp \
    ../client/pages/OrderHistoryPage.cpp ../client/pages/MessagePage.cpp ../client/pages/UserInfoPage.cpp \
    ../client/network/TcpClient.cpp ../client/network/TcpClientWorker.cpp
HEADERS += ../client/UserMainWindow.h ../client/MessageCenter.h \
    ../client/pages/HomePage.h \
    ../client/pages/NearbyStationsPage.h ../client/pages/NavigationPage.h \
    ../client/pages/ChargingPage.h ../client/pages/ChargeChartWidget.h \
    ../client/pages/OrderHistoryPage.h ../client/pages/MessagePage.h ../client/pages/UserInfoPage.h \
    ../client/network/TcpClient.h ../client/network/TcpClientWorker.h
RESOURCES += ../resources/res.qrc
