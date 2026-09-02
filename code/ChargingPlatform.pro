#-------------------------------------------------
# 东软电动汽车充电桩应用管理平台
# 双程序工程: server(服务端/管理后台) + client(用户客户端)
# 逐个进入 server/ 与 client/ 目录查看各自的 .pro
#-------------------------------------------------

TEMPLATE = subdirs

SUBDIRS += \
    server \
    client

server.file = $$PWD/server/ChargingServer.pro
client.file = $$PWD/client/ChargingClient.pro
