# 东软电动汽车充电桩应用管理平台（ChargingPlatform）

基于 **Qt（C++17）** 实现的电动汽车充电桩应用管理平台，覆盖「用户端 — 服务端 — 数据库 — Web 大屏」全链条。项目按《开发计划书》拆分为 **服务端 + 用户客户端 + Web 大数据可视化大屏** 三部分，是课程/实训交付项目。

> 源码位于 [`code/`](code/)，详细编译运行、通信协议、开发进度与常见问题见 [`code/README.md`](code/README.md)。

## 仓库结构

| 目录 | 说明 |
| ---- | ---- |
| [code/](code/) | 全部源代码（Qt 工程：服务端 + 客户端 + Web 大屏） |
| [database/](database/) | 提交的演示数据库 `test.db`（开箱即用，含完整种子数据） |
| [docs/](docs/) | 项目说明书、开发计划书、测试清单与参考资料 |

## 系统组成

| 程序 | 形态 | 核心功能 |
| ---- | ---- | ---- |
| **ChargingServer** 服务端 | Qt 界面程序（管理后台）+ TCP 服务端 | 管理员登录、销售业绩、电桩状态、充电桩管理、充电站管理、用户管理；处理客户端业务；持有数据库；内置 HTTP 服务器托管 Web 大屏 |
| **ChargingClient** 用户客户端 | Qt 界面程序（模拟手机端） | 手机号免密登录、附近充电站（腾讯地图）、一键导航、用户信息维护、电动汽车充电；**不直接访问数据库**，业务全部经 Socket 交互 |
| **Web 大数据可视化大屏** | Web 页面（本地化 ECharts） | 营收指标、电桩状态分布、营收趋势、24h 负荷预测，每 5 秒自动刷新 |

## 技术栈

- **语言/框架**：C++17 + Qt（Qt 5.15 与 Qt 6 均兼容；Ubuntu 22.04 及以上 + Qt Creator 6.2+）
- **数据库**：SQLite（QSQLITE 驱动随 Qt 自带，无需服务端进程；WAL 模式 + busy_timeout，多线程并发读写）
- **图表**：Qt Charts（未安装时自动降级为自绘折线图）+ Apache ECharts（Web 大屏，本地化）
- **地图**：腾讯地图 WebService API（客户端，密钥见 `client/mapconfig.h`，从 `.example` 模板生成，已被 git 忽略）
- **网络**：TCP 长连接，UTF-8 单行 JSON，`\n` 分帧（完整协议见 `common/protocol.h`）

## 快速开始

### 1. 编译运行

Qt Creator：打开 [`code/ChargingPlatform.pro`](code/ChargingPlatform.pro)（subdirs 根工程，会同时配置两个子项目），分别构建运行；也可单独打开 `server/ChargingServer.pro` 或 `client/ChargingClient.pro`。

命令行：

```bash
cd code
qmake && make            # 根工程会同时构建两个子项目
./server/ChargingServer  # 1. 先启动服务端
./client/ChargingClient  # 2. 再启动客户端
```

> 销售业绩页的折线图优先使用 QChart（需 `sudo apt install libqt5charts5-dev` 或 `libqt6charts6-dev`），未安装时自动降级为自绘折线图，均可正常编译运行。

### 2. 配置（可选，均有默认值）

```bash
export CHARGING_SERVER_HOST=127.0.0.1   # 服务器地址（默认同机）
export CHARGING_SERVER_PORT=9527        # TCP 端口
export CHARGING_DB=/path/to/test.db     # 服务端数据库路径（可选）
export CHARGING_WEB_PORT=8080           # Web 大屏 HTTP 端口
```

### 3. 访问 Web 大屏

服务端内置 HTTP 服务器，启动后浏览器访问：

```
http://服务器IP:8080
```

即可看到 4 块图表的大屏（营收趋势 / 电桩状态 / 各站营收 / 24h 负荷预测），每 5 秒自动刷新。**请勿**双击 `web/index.html` 打开——`file://` 协议下浏览器会拦截数据请求导致空白。

## 测试账号

| 身份 | 程序 | 账号 | 说明 |
| ---- | ---- | ---- | ---- |
| 管理员 | ChargingServer | `admin` / `123456` | 首次运行自动写入 admin 表，本机数据库校验 |
| 用户 | ChargingClient | 任意 `1` 开头的 11 位手机号 | 经服务端校验；未注册自动注册，被冻结账号拦截 |

## 数据库与演示数据

- 仅**服务端**访问数据库，客户端一律经 Socket 交互。
- 数据库文件查找顺序：`CHARGING_DB` → 工作目录 `test.db` → 可执行文件目录附近 → 都没有则新建；启动时自动建表（admin / user / station / pile / charge_order / op_log）。
- **首次运行即自动生成完整演示数据，开箱即用**（无需手动配置）：
  - 默认管理员 `admin / 123456`；
  - 12 个北京真实充电站 + 88 个电桩（含少量在用/故障样例）；
  - 近 30 天滚动演示订单（约 800 笔 + 10 个演示用户，按真实充电规律确定性生成：早晚通勤双高峰、晚高峰 17-19 点最忙、周末/工作日单量不同，随运行日滚动到今天，保证各图表始终有数据）。
- 仓库已提交一份开箱即用的演示库：`database/test.db`。
- **数据安全**：管理员密码使用加盐 SHA-256 摘要存储（自动升级旧 MD5/明文记录）；用户手机号仅存 SHA-256 哈希 + 脱敏列（如 `138****5678`），旧明文自动迁移；SQL 全预处理防注入。

## 目录结构

```
dian/
├── README.md                  # 本文件
├── code/                      # 全部源代码（详见 code/README.md）
│   ├── ChargingPlatform.pro   # subdirs 根工程：server + client
│   ├── common/                # 双端共享：protocol.h(协议) / types.h / GeoUtil
│   ├── server/                # ChargingServer（管理后台 + TCP 服务端 + 数据库持有者）
│   ├── client/                # ChargingClient（用户客户端，不含 sql 模块）
│   ├── resources/             # 共享 qss / qrc
│   └── web/                   # 大屏页面：index.html + echarts.min.js
├── database/                  # 提交的演示数据库 test.db
└── docs/                      # 说明书 / 开发计划书 / 测试清单 / 参考资料
```

## 文档

- [docs/开发计划书.md](docs/开发计划书.md) — 开发计划书（阶段 0-6 里程碑、WBS、分工、风险应对）
- [docs/测试清单.md](docs/测试清单.md) — 全流程测试清单
- `docs/01.项目说明书-东软电动汽车充电桩应用管理平台.doc` — 项目说明书
- `docs/说明书提取.txt` / `docs/说明书全文.txt` — 说明书文本提取
- `docs/qt使用sqlite.docx`、`docs/qtlogin.docx` — Qt/SQLite 与登录参考

## 开发进度

阶段 0-6 全部功能已代码完成（详见 [`code/README.md`](code/README.md#开发进度对照开发计划书)），当前处于**联调验收/回归测试**阶段。
