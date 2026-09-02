# 东软电动汽车充电桩应用管理平台 — Qt5 项目框架

框架阶段成果：**一个可编译运行的应用程序**，包含数据库连接、美化版登录系统，
以及管理端 / 用户端两套主界面骨架（各功能页面为占位页，页面内注释了待实现的功能点）。

## 开发环境

- 虚拟机：VMware 17 + Ubuntu 22.04（及以上）
- 开发工具：Qt Creator 6.2 及以上（Qt 5.15）
- 数据库：SQLite（Qt 自带 QSQLITE 驱动，无需服务端）

## 编译运行

方式一（推荐）：Qt Creator 中 `文件 → 打开文件或项目`，选择 `code/ChargingPlatform.pro`，
配置 Kits 后直接 `Ctrl+R` 运行。

方式二（命令行）：

```bash
cd code
qmake
make
./ChargingPlatform
```

## 测试账号

| 身份 | 入口 | 账号 | 说明 |
| ---- | ---- | ---- | ---- |
| 管理员 | 登录界面"管理员登录"页 | admin / 123456 | 首次运行自动写入 admin 表 |
| 用户 | 登录界面"用户登录"页 | 任意 1 开头的 11 位手机号 | 已注册直接登录；不存在自动注册（昵称"用户+手机号后4位"） |

## 数据库说明

- 数据库文件查找优先级：环境变量 `CHARGING_DB` → 工作目录 `test.db` → 可执行文件目录附近的 `test.db` / `database/test.db` → 都没有时在工作目录新建 `test.db`。
- 程序启动时**自动建表**（已存在则跳过）并写入默认数据，无需手工执行 SQL。建表脚本见 `src/common/DatabaseManager.cpp` 的 `createTables()`。
- 登录界面下方的状态栏、主窗口状态栏都会显示实际使用的数据库路径，便于排查连错库的问题。
- 表结构（状态字段约定见 `createTables()` 内注释）：

| 表名 | 用途 |
| ---- | ---- |
| admin | 管理员账号 |
| user | 用户信息（手机号 / 昵称 / 余额 / 状态 / 注册时间） |
| station | 充电站（名称 / 地址 / 经纬度 / 电价） |
| pile | 充电桩（所属电站 / 编号 / 快慢充 / 功率 / 状态 / 累计次数与时长） |
| charge_order | 充电订单（用户 / 电桩 / 电站 / 起止时间 / 电量 / 金额 / 状态） |

首次运行会自动插入 1 个默认管理员和 3 个示例充电站（含若干电桩，含少量"在用/故障"样例），
示例数据只在充电站表为空时写入，供后续功能联调使用。

## 目录结构

```
code/
├── ChargingPlatform.pro          # qmake 工程文件
├── resources/
│   ├── res.qrc
│   └── qss/
│       ├── login.qss             # 登录界面样式（渐变背景 / 卡片式登录框）
│       └── global.qss            # 主窗口全局样式（侧边导航 / 页头 / 表格）
└── src/
    ├── main.cpp                  # 入口: 初始化数据库 → 登录 → 按身份进入主界面
    ├── common/
    │   ├── DatabaseManager.*     # 数据库单例: 打开/建表/默认数据/登录查询
    │   └── Session.*             # 当前登录会话(单例), 各页面直接读取
    ├── login/
    │   └── LoginDialog.*         # 登录对话框(用户登录/管理员登录两个页签)
    ├── admin/
    │   ├── AdminMainWindow.*     # 管理端主窗口(侧边导航 + 页面栈)
    │   └── pages/                # 销售业绩/电桩状态/充电桩管理/充电站管理/用户管理
    └── user/
        ├── UserMainWindow.*      # 用户端主窗口
        └── pages/                # 附近充电站/一键导航/用户信息/电动汽车充电
```

说明：界面全部手写代码实现（未使用 .ui 文件），便于版本管理与后续扩展；
UI 文案均为中文，源码保存为 UTF-8 编码（Qt Creator 默认即为 UTF-8）。

## 后续开发路线（对照项目说明书）

1. **管理端功能页**：把 `src/admin/pages/` 下 5 个占位页替换为真实实现，
   数据查询直接使用 `DatabaseManager` 里已建好的表；营收图表需在 .pro 中启用 `QT += charts`
   （Ubuntu 需安装 `libqt5charts5-dev`）。
2. **用户端功能页**：把 `src/user/pages/` 下 4 个占位页替换为真实实现；
   地图导航需在 .pro 中启用 `QT += webenginewidgets`。
3. **Socket + 多线程**：建议在 .pro 中启用 `QT += network`，新增 `src/network/` 目录，
   用户端作为客户端、管理端作为服务端（示例：充电桩"远程重启"通过 Socket 发送指令），
   使用 pthread/QThread 处理并发收发。
4. **大数据可视化大屏**（Web 端，ECharts）：独立于本 Qt 工程，可由服务端程序导出 JSON 数据供 Web 页面读取。
5. 公共约定：登录后的身份信息统一从 `Session::instance()` 读取；
   金额用 `double`（元）、时长用 `int`（分钟）、经纬度用 `double`。

## 常见问题

- **登录报"无法打开数据库"**：检查状态栏显示的路径；或用 `export CHARGING_DB=/path/test.db` 指定。
- **提示 QSQLITE driver not loaded**：确认 .pro 中有 `QT += sql`，并重新 qmake。
- **中文乱码**：确认源文件为 UTF-8 编码（Qt Creator: 工具→选项→文本编辑器→行为→默认编码 UTF-8）。
