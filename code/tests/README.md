# 用户首页交互回归

QtTest 在临时本机端口启动模拟服务端，不使用实际账号、数据库或地图接口。

```bash
mkdir -p /tmp/discovery-test-build
cd /tmp/discovery-test-build
qmake6 /path/to/charging/code/tests/discovery_ui.pro
make -j4
QT_QPA_PLATFORM=offscreen ./discovery_ui_test
```

覆盖首页入口、站点搜索与空闲筛选、无桩禁用、满桩选站传递、刷新保留站点、导航起点及终点传递、加载失败恢复、已有充电订单保留，以及 800×600 窗口尺寸。测试截图保存到 `/tmp/charging-*.png`。

该测试编译原生地图降级视图，不连接外部导航、执行真实扣费或创建真实预约；正式客户端构建仍自动检测并启用 Qt WebEngine。

## 浅色主题与功率模型回归

`discovery_ui_test` 现包含 9 项检查（含初始化和清理）：强制深色调色板后应用浅色主题、遍历全部客户端页面、检查表格末列自动补宽、功率范围和可重复性、波动与后段降功率。

服务端集成验证：在独立构建目录中使用 `qmake6 /path/to/charging/code/tests/server_ui.pro && make -j4`，然后运行 `QT_QPA_PLATFORM=offscreen ./server_ui_smoke`。它会创建临时数据库，模拟 12 次充电推进，检查累计电量与功率积分、金额一致性和结算，并遍历六个管理页面。成功返回 0，临时数据库自动清理，截图输出到 `/tmp/charging-admin-*.png`。

## 局域网连接回归

在独立目录使用 `qmake6 /path/to/charging/code/tests/lan_connection.pro` 和 `make -j4` 构建，运行 `QT_QPA_PLATFORM=offscreen ./lan_connection_test`。

测试使用临时本机端口和隔离的 QSettings 目录，覆盖服务器切换、失败重试、连接取消、无效 IP、端口范围、成功配置保存、无协议响应以及绕过不可用系统代理。登录页截图保存到 `/tmp/charging-lan-login.png`。运行环境必须允许监听本机端口。

`server_ui_smoke` 还验证真实 TCP 手机号登录、服务端显示实际监听端口、地址复制，以及关闭监听后的状态刷新。

## 界面修饰与动效

服务端回归还检查所有管理表格的卡片包装、环状图在 260×520、700×300、360×360 下保持圆形，以及连续切页后动画覆盖层自动清理。环状图截图输出至 `/tmp/charging-donut-宽x高.png`。客户端回归增加连续切页清理检查，现为 10 项（含初始化和清理）。
