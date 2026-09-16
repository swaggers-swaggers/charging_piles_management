# 电动汽车充电桩应用管理平台

本项目由两部分组成：

1. **Qt 业务系统**：基于 C++17 与 Qt Widgets 的课程实训项目，包含用户客户端、服务端管理后台，演示找站、预约、充电、结算及运营管理流程。
2. **大数据预测与可视化大屏**：基于 PySpark / Spark MLlib / Hadoop 的数据治理、负荷预测、用户需求建模与四主题 Web 大屏，支持真实公开数据与模拟数据混合展示。

客户端通过 TCP 与服务端通信，SQLite 数据库由服务端统一访问。充电推进、账户充值与结算用于业务模拟，未接入真实充电设备或支付渠道。大数据模块独立于 Qt 业务系统，通过 Web 大屏展示分析结果。

## 功能概览

### Qt 业务系统

| 模块 | 主要功能 |
| --- | --- |
| 用户登录 | 手机号登录与自动注册；登录页配置服务器 IP、端口；验证连接并保存成功配置 |
| 附近充电站 | 站点搜索、空闲筛选、演示位置选择、地图展示与导航 |
| 充电服务 | 充电目标设置、预授权冻结、功率与进度展示、停止充电、自动结算 |
| 订单与预约 | 历史订单、订单详情、时段预约、消息通知 |
| 我的账户 | 用户资料、余额展示、模拟充值 |
| 管理后台 | 销售业绩、电桩状态、充电桩管理、订单管理、充电站管理、用户管理 |
| 局域网协作 | 服务端展示各网卡 IP 和实际监听端口，支持复制与刷新；客户端无需命令行即可连接 |
| 界面体验 | 数据卡片、状态徽标、等比例环状图、页面切换及按钮点击动效 |

### Web 数据大屏

| 视图 | 主要内容 |
| --- | --- |
| 运营总览 | 结算电量/收入/订单 KPI、站点健康排行、导航地图、行政区负荷、30 天趋势、设备状态与峰谷贡献 |
| AI 负荷预测 | 分站 H1/H6/H24 预测、占用与峰值预警、区间表、模型依据、近 7 日回测、低拥堵站点推荐 |
| 用户需求 | 168 时段热力图、次日需求分布、电量直方图、严格时间留出评估、冷启动分群 |
| 数据与模型健康 | 数据处理链路、质量雷达、质量规则表、任务审计、站点模型健康与回测 |

## 系统结构

```text
┌─────────────────────────────────────────────────────────┐
│                    Qt 业务系统                            │
│  ChargingClient（Qt 用户客户端）                          │
│          │ TCP / 单行 JSON                                │
│          ▼                                                │
│  ChargingServer（Qt 管理后台 + TCP 服务 + 充电引擎）       │
│          ├── SQLite：用户、站点、电桩、订单、预约、流水    │
│          └── HTTP 服务 ── Web 大屏（实时版）               │
└─────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────┐
│                  大数据预测流水线                          │
│  原始数据（北京模拟 / 北京 Figshare 真实 / Boulder 真实）   │
│          ▼                                                │
│  HDFS ODS → PySpark 质量检查 → DWD/DWS → 特征工程         │
│          ▼                                                │
│  Spark MLlib（GBT / ALS）+ PyTorch（残差 CNN）             │
│          ▼                                                │
│  ADS 七份 JSON → Web 大屏（独立 HTTP 服务）               │
└─────────────────────────────────────────────────────────┘
```

## 环境要求

### Qt 业务系统

- 支持 C++17 的编译器、qmake 和对应平台的构建工具。
- Qt 模块：Core、Gui、Widgets、Network；服务端还需要 Sql 和 QSQLITE 驱动。
- 可选模块：Qt Charts 用于销售图表；Qt WebEngine Widgets 用于应用内地图。缺少可选模块时有原生绘制降级视图。
- 自动化测试另需 Qt Test。

工程保留 Qt 5.15 / Qt 6 的兼容分支；近期验证环境为 macOS（Qt 6.11.2 Homebrew）与 Ubuntu 24.04（Qt 6.4.2）。其他操作系统和版本需在目标环境验证。

### 大数据与大屏

- Python 3.11（虚拟环境 `.venv-bigdata`）
- Java 8、Spark / PySpark 3.4.1、Hadoop 3.2.1（伪分布式或集群）
- 可选：PyTorch CPU（用于残差 CNN 对照实验）
- 详细依赖锁定见 `bigdata/requirements.txt` 与 `bigdata/conf/versions.env`。

## 快速开始

### 0. 首次生成本地数据库

仓库不提交运行数据库。拉取项目后，先将北京模拟数据集导入本地 SQLite：

```bash
python3 database/import_beijing_dataset.py
```

脚本读取仓库根目录的 `05.北京模拟充电数据集.zip`，创建 `database/test.db`，并生成 `analytics_beijing_*` 分析表。数据库已存在时会先创建备份；生成的数据库、备份及 WAL/SHM 文件均被 Git 忽略，不应提交。首次启动 `ChargingServer` 时会继续初始化原有业务表和演示数据。

### 1. 构建 Qt 程序

推荐使用 Qt Creator 打开 [code/ChargingPlatform.pro](code/ChargingPlatform.pro)，选择配置好依赖的 Kit，构建 `ChargingServer` 和 `ChargingClient`。两个程序也可通过各自的 `.pro` 独立构建。

命令行构建方法见 [开发与运行说明](code/README.md#命令行构建)。

### 2. 启动服务端

运行 `ChargingServer`，使用演示管理员 `admin / 123456` 登录。在管理后台顶部查看"客户端连接地址"，例如 `192.168.1.100:9527`。

默认管理员只在管理员表为空时创建；已有数据库中的账号以实际数据为准。

### 3. 连接客户端

运行 `ChargingClient`，在登录页填写服务端 IP 和端口，点击"连接服务器"，成功后输入手机号并登录。首次使用的手机号会自动注册；被冻结的账号无法登录。

- 同机测试：IP 填 `127.0.0.1`，默认端口 `9527`。
- 两台电脑：填写服务端显示的局域网 IP，双方网络须可互通。
- 成功连接的配置会自动保存；重新启动后可继续使用。

防火墙、VMware 桥接和多网卡选择说明见 [局域网连接说明](docs/局域网连接说明.md)。

### 4. 查看 Web 大屏

大屏有两种访问方式：

**方式一：通过 Qt 服务端（实时业务数据）**

服务端启动 HTTP 服务后，点击管理后台"打开大屏"，或在浏览器访问 `http://127.0.0.1:8080`。客户端的 TCP 端口与大屏的 HTTP 端口用途不同。

**方式二：独立 HTTP 服务（大数据 ADS 数据）**

```bash
cd code/web
./start.sh
```

浏览器访问 `http://127.0.0.1:8080`。此方式不依赖 Qt 服务端，直接加载 `code/web/data/` 下的大数据流水线输出 JSON。直接双击 HTML 文件不属于支持的运行方式。

### 5. 运行大数据流水线（可选）

完整的 Spark / Hadoop 数据治理与模型训练流程见 [bigdata/README.md](bigdata/README.md)。简要步骤：

```bash
python3 -m venv .venv-bigdata
.venv-bigdata/bin/pip install -r bigdata/requirements.txt
bash bigdata/scripts/check_environment.sh
export BIGDATA_ROOT="file://$PWD/.bigdata/lake"
export RUN_ID="$(date +%Y%m%d_%H%M%S)"
bash bigdata/scripts/run_full_pipeline.sh bigdata/conf/dev.yaml
```

## 数据来源说明

大屏支持三种数据源，按 `dataNature` 字段标识：

| 数据源 | 说明 |
| --- | --- |
| 模拟数据 | 北京模拟充电数据集，用于方法验证 |
| 真实数据 | City of Boulder 官方开放充电交易数据（外部实验） |
| 真实数据 + 模拟补齐 | 北京主大屏：Figshare 真实北京交易 + 模拟数据补齐缺失字段（用户 ID、故障遥测等） |

北京真实数据来自 Figshare 发布的 *Beijing public charging transactions v2*（CC BY 4.0），仅覆盖 2025 年 1 月、7 月两个离散月份；站点位置仅有约 1 km 匿名网格编码，无精确经纬度。Boulder 数据为美国科罗拉多州真实充电交易，与北京数据分库、分报告、分展示，不混用。

## 仓库目录

```text
.
├── README.md                       # 项目入口
├── 05.北京模拟充电数据集.zip        # 原始模拟数据
├── code/
│   ├── ChargingPlatform.pro        # qmake 双程序工程
│   ├── client/                     # 用户界面、客户端会话与网络
│   ├── server/                     # 管理后台、DAO、TCP/HTTP 服务、充电引擎、预测器
│   ├── common/                     # 协议、数据类型、主题、图表及动效组件
│   ├── resources/                  # 图标、样式和资源清单
│   ├── web/                        # Web 大屏（HTML/CSS/JS + 数据 JSON）
│   │   ├── index.html / analytics.js / analytics.css
│   │   ├── map.html                # 导航地图
│   │   ├── start.sh                # 独立大屏启动脚本
│   │   └── data/                   # 大数据流水线输出的 ADS JSON
│   └── tests/                      # 客户端、局域网连接、服务端回归
├── bigdata/                        # Spark / Hadoop 大数据模块
│   ├── README.md                   # 大数据模块详细说明
│   ├── DEMO.md                     # 部署演示与血缘追溯
│   ├── ACCEPTANCE.md               # 验收记录
│   ├── conf/                       # 配置文件（dev / cluster / sample）
│   ├── jobs/                       # PySpark 作业
│   ├── serving/                    # ADS JSON 导出
│   ├── experiments/                # CNN 深度学习实验
│   ├── scripts/                    # 流水线脚本
│   └── reports/                    # 实验报告
├── database/                       # 演示数据库和升级脚本
├── scripts/                        # 本地启动脚本（server / client / hadoop / dashboard）
└── docs/                           # 设计、操作说明、测试清单与界面预览
```

## 测试与文档

自动化测试覆盖客户端交互、连接配置、服务端真实 TCP 登录、充电结算、卡片布局、环状图比例及动画清理。大数据模块另有 PyTest 数据契约与指标校验。测试使用模拟服务端或临时数据库，不代表已验证真实设备、支付或外部地图服务。

- [开发与运行说明（Qt）](code/README.md)
- [大数据模块说明](bigdata/README.md)
- [部署与演示指南](bigdata/DEMO.md)
- [自动化测试说明](code/tests/README.md)
- [局域网连接说明](docs/局域网连接说明.md)
- [充电系统完整设计方案](docs/充电系统完整设计方案.md)
- [开发计划书](docs/开发计划书.md)
- [手工测试清单](docs/测试清单.md)
- [用户首页重构说明](docs/用户首页重构说明.md)
- [Web 大屏智能化升级实施方案](docs/Web大屏智能化升级实施方案.md)

设计方案和开发计划保留了阶段性规划；当前功能、配置和行为以源码及实际运行结果为准。
