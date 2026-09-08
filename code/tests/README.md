# 自动化测试说明

本目录包含三个独立的 Qt 回归测试工程。构建需要 Qt Widgets、Network、Test；服务端集成测试还需要 Qt Sql 和 QSQLITE 驱动。

## 测试范围

| 工程 | 可执行程序 | 主要覆盖 |
| --- | --- | --- |
| [discovery_ui.pro](discovery_ui.pro) | `discovery_ui_test` | 首页搜索与筛选、站点选择、导航传参与失败重试、账户与消息卡片、已有充电订单保留、主题与布局、功率模型、动画清理 |
| [lan_connection.pro](lan_connection.pro) | `lan_connection_test` | 服务器切换、无效地址与端口、失败重试、连接取消、登录页配置保存、无协议响应、绕过不可用系统代理 |
| [server_ui.pro](server_ui.pro) | `server_ui_smoke` | 真实 TCP 登录、局域网地址显示与复制、充电推进与结算、六个管理页面、表格卡片、环状图比例及动画清理 |
| [database_seed_recovery.pro](database_seed_recovery.pro) | `database_seed_recovery` | 验证全新数据库自动填充；模拟有导入标记但无充电桩的旧库，验证启动时自动恢复站点、充电桩、用户和演示订单 |

测试条目会随功能变化，以当前源码和执行输出为准。

## 构建与运行

以下命令从**仓库根目录**执行，适用于 Linux / Qt 6。Qt 5 请改用对应 Kit 的 qmake。

### 客户端交互

```bash
mkdir -p build/tests/discovery
cd build/tests/discovery
qmake6 ../../../code/tests/discovery_ui.pro
make -j4
QT_QPA_PLATFORM=offscreen ./discovery_ui_test
```

### 局域网连接

重新从仓库根目录执行：

```bash
mkdir -p build/tests/lan
cd build/tests/lan
qmake6 ../../../code/tests/lan_connection.pro
make -j4
QT_QPA_PLATFORM=offscreen ./lan_connection_test
```

### 服务端集成

重新从仓库根目录执行：

```bash
mkdir -p build/tests/server
cd build/tests/server
qmake6 ../../../code/tests/server_ui.pro
make -j4
QT_QPA_PLATFORM=offscreen ./server_ui_smoke
```

`offscreen` 用于无桌面窗口的界面测试。运行环境仍必须允许监听本机临时端口；沙箱禁止 socket 监听时，不能将环境失败视为业务测试结果。

## 数据隔离与结果

- 客户端交互测试启动本机模拟服务端，不访问实际业务数据库。
- 局域网测试使用本机临时端口和隔离的 QSettings 配置目录。
- 服务端测试通过 `QTemporaryDir` 创建临时数据库，验证真实 TCP 登录及模拟充电结算。
- QtTest 程序输出各条测试结果；服务端集成程序成功返回 `0`，失败返回非零值。
- 修改共享头文件、资源或工程依赖后，重新运行 qmake 并构建，确保测试二进制包含最新改动。

## 截图输出

| 路径 | 内容 |
| --- | --- |
| `/tmp/charging-*.png` | 客户端页面及相关预览 |
| `/tmp/charging-lan-login.png` | 客户端服务器连接配置界面 |
| `/tmp/charging-admin-*.png` | 六个服务端管理页面 |
| `/tmp/charging-donut-宽x高.png` | 环状图在不同尺寸下的绘制结果 |

服务端测试检查环状图在 260×520、700×300 和 360×360 尺寸下保持等比例，并检查连续切页后的动画覆盖层清理。

## 验证边界

这些测试不连接真实充电设备或支付渠道。客户端交互测试使用原生地图降级视图，不验证 Qt WebEngine、外部地图和导航服务的实际可达性；本机连接测试也不能替代两台实体电脑的局域网验证。

实际演示前，请结合 [手工测试清单](../../docs/测试清单.md) 和 [局域网连接说明](../../docs/局域网连接说明.md) 验证目标环境。
