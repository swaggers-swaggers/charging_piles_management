# 东软电动汽车充电桩应用管理平台 — Qt5 双程序工程

按《开发计划书》（见 `docs/开发计划书.md`）拆分为 **服务端 + 用户客户端 + Web 大屏**：

| 程序 | 说明 |
| ---- | ---- |
| **ChargingServer** 服务端 | 管理后台（管理员登录/销售业绩/电桩状态/充电桩管理/充电站管理/用户管理）+ TCP 服务端（处理客户端业务）+ **数据库唯一持有者** |
| **ChargingClient** 用户客户端 | 手机端体验（手机号免密登录/附近充电站/一键导航/用户信息/充电），**不直接访问数据库**，业务全部经 Socket 与服务端交互 |

## 开发环境

- 虚拟机：VMware 17 + Ubuntu 22.04（及以上）
- 开发工具：Qt Creator 6.2 及以上
- Qt 版本：Qt 5.15 与 Qt 6 均可编译（代码做了双版本兼容；Ubuntu 22.04 默认源下 `qt6-base-dev` / `qt5-default` 装哪个就用哪个 Kit）
- 数据库：SQLite（QSQLITE 驱动随 Qt 自带，无需服务端进程）

## 编译运行

Qt Creator：打开根目录 `ChargingPlatform.pro`（subdirs 工程，会同时配置两个子项目），分别构建运行；也可单独打开 `server/ChargingServer.pro` 或 `client/ChargingClient.pro`。

命令行：

```bash
cd code
qmake && make            # 根工程会同时构建两个子项目
./server/ChargingServer  # 1. 先启动服务端
./client/ChargingClient  # 2. 再启动客户端
```

> **Qt Charts 说明**：销售业绩页的营收折线图优先使用 QChart（需 `sudo apt install libqt5charts5-dev` 或 `libqt6charts6-dev`）；未安装该组件时工程会自动降级为自绘折线图，两种情况下都能正常编译运行（详见 `docs/开发计划书.md` 风险应对）。

**运行顺序**：客户端登录时才需要服务端在线，但演示请先启动服务端。默认服务器地址 `127.0.0.1:9527`（同机），可用环境变量覆盖：

```bash
export CHARGING_SERVER_HOST=127.0.0.1
export CHARGING_SERVER_PORT=9527
export CHARGING_DB=/path/to/test.db    # 服务端数据库文件位置(可选)
export CHARGING_WEB_PORT=8080          # Web 大屏 HTTP 端口(可选)
```

### Web 大数据可视化大屏

服务端**内置 HTTP 服务器**，直接把大屏页面和 `data.json` 数据一起托管，无需另起 Web 服务。服务端启动后，浏览器访问：

```
http://服务器IP:8080
```

即可看到 4 块图表的大屏（营收趋势 / 电桩状态 / 各站营收 / 24h 负荷预测），每 5 秒自动刷新。请**不要**双击 `web/index.html` 打开——`file://` 协议下浏览器会拦截数据请求导致空白。

## 测试账号

| 身份 | 程序 | 账号 | 说明 |
| ---- | ---- | ---- | ---- |
| 管理员 | ChargingServer | admin / 123456 | 首次运行自动写入 admin 表，本机数据库校验 |
| 用户 | ChargingClient | 任意 1 开头的 11 位手机号 | 经服务端校验；未注册自动注册（昵称"用户+手机号后4位"），被冻结账号拦截 |

## 数据库

- 仅**服务端**访问数据库；文件查找顺序：`CHARGING_DB` → 工作目录 `test.db` → 可执行文件目录附近 → 都没有则在工作目录新建。
- 启动时自动建表（admin / user / station / pile / charge_order，字段约定见 `server/DatabaseManager.cpp`）。
- **首次运行即自动生成完整演示数据**（克隆后无需任何手动配置，开箱即用）：
  - 默认管理员 `admin / 123456`
  - 12 个北京真实充电站（海淀/朝阳/东城/西城/大兴/通州/丰台，坐标来自地图POI公开信息）+ 88 个电桩（含少量"在用/故障"样例）
  - **近 30 天滚动演示订单**（约 800 笔 + 10 个演示用户，确定性生成：金额/电量分布固定；订单按真实充电规律分布——早晚通勤双高峰、晚高峰 17-19 点最忙、深夜低谷，工作日/周末单量不同；随运行日滚动到今天，保证今日/本月/总营收和 7/30 日趋势图始终有数据）
- **手机号隐私保护**：`user.phone` 仅存 **SHA-256 哈希**（不可逆），另存 `phone_masked` 脱敏列（如 `138****5678`）供管理端展示/搜索；旧库明文手机号启动时自动迁移。客户端"记住手机号"也加密存储。
- SQLite 已开启 WAL 模式 + busy_timeout，支持管理界面与网络线程并发读写。
- 客户端状态栏/登录页会显示实际使用的服务器地址，服务端状态栏显示数据库路径与大屏地址，便于排查。

## 目录结构

```
code/
├── ChargingPlatform.pro          # subdirs 根工程
├── common/                       # 双端共享
│   ├── common.pri
│   ├── protocol.h                # 通信协议: 消息类型/字段/错误码(即协议文档)
│   └── types.h                   # 业务枚举 + DTO(JSON 互转)
├── server/                       # 服务端(管理后台 + TCP 服务端)
│   ├── main.cpp                  # 初始化数据库 → 启动监听 → 管理员登录 → 后台
│   ├── DatabaseManager.*         # 数据库单例(打开/建表/种子/登录查询, 支持指定连接名)
│   ├── ServerSession.*           # 管理员会话
│   ├── AdminLoginDialog.*        # 管理员登录(本机校验)
│   ├── AdminMainWindow.*         # 管理后台主窗口
│   ├── pages/                    # 5 个管理功能页(占位, 注释含计划功能)
│   └── network/                  # TcpServer(每连接一线程) + ClientHandler(业务处理)
├── client/                       # 用户客户端(不含 sql 模块)
│   ├── main.cpp
│   ├── ClientSession.*           # 用户会话
│   ├── LoginDialog.*             # 手机号登录(Socket 校验)
│   ├── UserMainWindow.* / pages/ # 4 个用户功能页(占位)
│   └── network/                  # TcpClient(请求-响应+超时+推送分发, 独立线程)
├── resources/                    # 共享 qss / qrc
└── web/                          # (阶段4) 大屏页面
```

登录页使用 **`.ui` 文件**（Qt Designer 可视化布局，见 `LoginDialog.ui` / `AdminLoginDialog.ui`），其余页面代码构建 + 全局/登录 QSS 主题（`resources/qss/`），源码为 UTF-8 编码。

## 通信协议速览

TCP 长连接，UTF-8 单行 JSON，`\n` 分帧；请求/应答回显相同 `type`，统一 `ok`/`error` 字段，`type>=100` 为服务端推送。完整定义见 `common/protocol.h`。

## 开发进度（对照开发计划书）

| 阶段 | 内容 | 状态 |
| ---- | ---- | ---- |
| 0 | 计划书落盘 + 双程序工程拆分 + Socket 登录 | ✅ 已验收 |
| 1 | 服务端框架完善 + 管理端 4 个数据页面 | ✅ 代码完成, 待验收 |
| 2 | 用户客户端 4 大功能 | ✅ 代码完成, 待验收 |
| 3 | 销售业绩 QChart + 大屏数据导出 | ✅ 代码完成, 待验收 |
| 4 | Web 可视化大屏 | ✅ 代码完成, 待验收 |
| 5 | 负荷预测（简化） | ✅ 代码完成, 待验收 |
| 6 | 错误处理/安全/测试/文档收尾 | ✅ 代码完成, 待回归测试（见 docs/测试清单.md） |

## 常见问题

- **客户端提示"无法连接服务器"**：先启动 ChargingServer；检查地址端口（登录页下方有提示）；虚拟机内如有防火墙放行 9527 端口。
- **服务端报"端口监听失败"**：9527 被占用，可用 `CHARGING_SERVER_PORT` 换端口（两端都要设）。
- **登录报"无法打开数据库"**：查看服务端状态栏路径；必要时 `export CHARGING_DB=/path/test.db`。
- **大屏打不开 / 空白**：用 `http://IP:8080` 访问（不要双击 index.html）；确认服务端已启动且状态栏显示"大屏访问: http://localhost:8080"；端口被占用可用 `CHARGING_WEB_PORT` 换。
- **销售业绩 / 大屏没数据**：确认服务端启动日志出现"已生成近30天固定演示订单数据"；若数据被改动，删除 `test.db` 后重启服务端即可重新生成演示数据。
- **提示 QSQLITE driver not loaded**：确认 .pro 有 `QT += sql` 并重新 qmake。
- **中文乱码**：确认源文件为 UTF-8 编码。
