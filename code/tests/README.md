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
