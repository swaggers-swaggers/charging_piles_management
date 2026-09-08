# 开发与运行说明

项目功能概览见 [仓库 README](../README.md)。本文说明工程构建、配置和代码入口。

## 构建准备

| 依赖 | 用途 | 是否必需 |
| --- | --- | --- |
| C++17 编译器、qmake、构建工具 | 编译 Qt 工程 | 是 |
| Qt Core / Gui / Widgets / Network | 两端界面与通信 | 是 |
| Qt Sql 与 QSQLITE 驱动 | 服务端数据库访问 | 服务端必需 |
| Qt Charts | 销售业绩图表；缺失时使用自绘图表 | 否 |
| Qt WebEngine Widgets | 应用内地图；缺失时使用原生降级画布 | 否 |
| Qt Test | 自动化回归测试 | 运行测试时必需 |

使用同一个 Qt Kit 的头文件、库和 qmake。Qt Creator 可打开 [根工程](ChargingPlatform.pro)，也可分别打开 [服务端工程](server/ChargingServer.pro) 与 [客户端工程](client/ChargingClient.pro)。新增可选模块后，重新运行 qmake 并构建。

### 命令行构建

以下为 Linux / Qt 6 示例，从**仓库根目录**执行，使用独立构建目录：

```bash
mkdir -p build/server build/client
cd build/server
qmake6 ../../code/server/ChargingServer.pro
make -j4
cd ../client
qmake6 ../../code/client/ChargingClient.pro
make -j4
cd ../..
```

Qt 5 使用所选 Kit 对应的 `qmake` 替代 `qmake6`。Windows 等平台使用对应 Kit 的构建工具及程序路径。

运行两个程序时分别使用一个终端，以下命令均从仓库根目录执行：

```bash
# 终端一：先启动服务端
./build/server/ChargingServer
```

```bash
# 终端二：启动用户客户端
./build/client/ChargingClient
```

Qt Creator 中分别选择两个可执行目标运行即可；日常连接可完全通过登录界面完成。

## 登录与局域网连接

1. 服务端启动后完成管理员登录，默认新库账号为 `admin / 123456`。
2. 管理后台顶部显示可供客户端使用的 IP 和实际 TCP 监听端口，可选择网卡、复制地址或刷新。
3. 客户端登录页输入 IP、端口，点击“连接服务器”。程序验证服务端响应，成功后保存连接配置。
4. 输入 `1` 开头的 11 位手机号并登录。未注册手机号自动注册，被冻结账号会被拦截。

同机使用 `127.0.0.1`；跨电脑使用服务端的局域网 IP。默认 TCP 端口为 `9527`，监听所有 IPv4 接口。具体排查方法见 [局域网连接说明](../docs/局域网连接说明.md)。

## 配置项

| 配置项 | 作用范围 | 默认行为 |
| --- | --- | --- |
| `CHARGING_SERVER_HOST` | 客户端首次填写地址的默认值 | `127.0.0.1` |
| `CHARGING_SERVER_PORT` | 服务端 TCP 监听端口；客户端首次填写端口的默认值 | `9527`，非法端口回退到默认值 |
| `CHARGING_DB` | 服务端数据库文件路径 | 按下面的查找顺序选择 |
| `CHARGING_WEB_PORT` | 服务端 HTTP 端口 | `8080` |
| `CHARGING_WEB_DIR` | 大屏页面和数据导出目录 | 自动寻找 Web 目录 |

客户端已经保存过成功连接的地址时，登录页优先恢复保存值；本次输入决定本次连接目标。修改服务端端口后，客户端直接在登录页填写新端口即可。

需要指定数据库位置时，先创建可写的父目录，再设置 `CHARGING_DB`。服务端状态栏显示实际数据库路径。以上环境变量应在程序启动前设置；Qt Creator 可通过项目的运行环境配置。

## 数据库与演示模式

[DatabaseManager.cpp](server/DatabaseManager.cpp) 按以下顺序选择数据库：

1. `CHARGING_DB` 指定的文件。
2. 当前工作目录已存在的 `test.db`。
3. 从可执行文件目录开始，最多检查三层目录中的 `test.db` 和 `database/test.db`。
4. 未找到时，在当前工作目录创建 `test.db`。

服务端使用 SQLite WAL、3 秒 busy timeout 和外键约束。主要表包括 `admin`、`user`、`station`、`pile`、`charge_order`、`price_rule`、`charge_reservation`、`recharge_log` 和 `op_log`。

启动时执行建表、已有迁移逻辑和种子数据初始化。默认站点种子为 12 个示例站点、96 个电桩；订单为空时生成近 30 天模拟订单，已有订单不会自动刷新为当天数据。仓库自带数据库会随演示操作改变，不能将种子数量当作运行时固定数量。如果旧库保留了站点或导入标记但充电桩表为空，服务端会在启动时自动恢复内置站点数据。

管理员密码保存在 `password_hash` 与 `salt` 字段，验证使用 SHA-256；用户手机号保存哈希与脱敏展示列。客户端记住手机号使用 XOR 混淆后编码保存，这不是强加密。项目采用免密手机号登录和模拟充值，面向课程演示。

需要重新演示时，优先指定一个新的数据库文件，保留已有库用于追溯，不必删除原始数据。

## 地图与 Web 大屏

### 地图和导航

- MapLibre GL JS + OpenFreeMap：应用内地图，需要 Qt WebEngine 和可用的外部网络。
- OSRM：路线预览；失败时界面提供提示和重试入口。
- 高德链接：由系统浏览器打开外部导航，不需要 WebService Key。
- 用户选择演示起点；坐标转换逻辑见 [GeoUtil.h](common/GeoUtil.h)。

局域网业务 TCP 固定直连；地图网络配置见 [MapNetwork.h](client/network/MapNetwork.h)。地图连接异常时可使用导航页的重新连接入口；外部浏览器的代理由系统或浏览器管理。

### Web 大屏

服务端内置 HTTP 服务，默认浏览器地址为 [http://127.0.0.1:8080](http://127.0.0.1:8080)。页面大约每 5 秒请求数据，`/data.json` 优先实时从数据库聚合；服务端另按 10 秒间隔导出数据文件。

页面及 ECharts 位于 [web/](web/)。自定义部署路径时，可通过 `CHARGING_WEB_DIR` 指定包含 `index.html` 和 `echarts.min.js` 的目录。HTTP 资源处理见 [HttpServer.cpp](server/HttpServer.cpp)，导出目录选择见 [DataExporter.cpp](server/DataExporter.cpp)。

## 代码导航

| 入口 | 职责 |
| --- | --- |
| [protocol.h](common/protocol.h)、[types.h](common/types.h) | 消息类型、错误码、业务枚举和数据结构 |
| [LoginDialog.cpp](client/LoginDialog.cpp) | 客户端地址配置、连接检查和登录 |
| [TcpClient.cpp](client/network/TcpClient.cpp)、[TcpClientWorker.cpp](client/network/TcpClientWorker.cpp) | 独立网络线程、连接切换、超时处理和推送分发 |
| [ClientHandler.cpp](server/network/ClientHandler.cpp) | 服务端请求处理与客户端会话 |
| [ChargingEngine.cpp](server/ChargingEngine.cpp) | 模拟充电推进、结算、恢复、排队和预约扫描 |
| [dao/](server/dao/) | 数据库读写 |
| [AdminMainWindow.cpp](server/AdminMainWindow.cpp) | 六个管理页面和局域网信息面板 |
| [UserMainWindow.cpp](client/UserMainWindow.cpp) | 用户页面导航和会话信息 |
| [AdminTableCard.h](common/AdminTableCard.h)、[DonutChart.h](common/DonutChart.h)、[UiMotion.h](common/UiMotion.h) | 表格卡片、等比例环状图和轻量动效 |
| [resources/qss/](resources/qss/) | 全局、客户端和登录页样式 |

登录界面使用 Qt Designer `.ui` 文件，其余主要页面通过 C++ 布局构建。源码使用 UTF-8。

TCP 采用 UTF-8 单行 JSON，每条消息以换行符 `\n` 结束。请求与应答带 `type`，应答使用 `ok` 和可选的 `error`；`type >= 100` 为服务端推送。具体参数以 [协议头文件](common/protocol.h) 和处理代码为准。

## 测试

构建和执行方法见 [自动化测试说明](tests/README.md)，完整业务验收参考 [手工测试清单](../docs/测试清单.md)。新增功能应补充对应的交互或业务验证，不将历史测试结果作为当前版本已通过的证明。

## 常见问题

| 现象 | 检查方法 |
| --- | --- |
| 客户端连接失败 | 检查服务端是否监听、IP/端口是否一致、防火墙是否允许连接；虚拟机跨电脑访问参考局域网说明 |
| TCP 端口监听失败 | 检查端口占用，必要时修改服务端端口，并在客户端登录页更新 |
| QSQLITE 驱动不可用 | 确认当前 Qt Kit 安装了 SQLite 驱动且运行时可加载插件，仅添加 Sql 编译模块不足以解决插件缺失 |
| 数据库打不开 | 查看状态栏或错误提示中的路径，检查文件及父目录权限 |
| 大屏空白或没有数据 | 使用 HTTP 地址访问，检查 HTTP 服务状态和数据；演示历史订单不会每天重新生成 |
| 地图显示降级画布 | 检查 Qt WebEngine Widgets 是否可用，安装对应模块后重新运行 qmake 并构建 |
| 底图或路线请求失败 | 检查网络和代理，使用界面重试入口；该问题与局域网 TCP 连接独立 |
