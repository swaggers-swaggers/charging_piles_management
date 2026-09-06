# 双端共享的头文件(通信协议 / 业务类型), 由 server 与 client 的 .pro include
INCLUDEPATH += $$PWD

HEADERS += \
    $$PWD/protocol.h \
    $$PWD/types.h \
    $$PWD/GeoUtil.h \
    $$PWD/IconFactory.h

HEADERS += $$PWD/AppTheme.h $$PWD/ChargingPowerModel.h

HEADERS += $$PWD/UiMotion.h $$PWD/AdminTableCard.h $$PWD/DonutChart.h
