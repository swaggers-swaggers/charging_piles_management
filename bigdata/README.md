# 北京模拟充电数据 · Spark / Hadoop 预测工程

独立于 Qt、TCP 和 SQLite 的数据处理与展示模块。输入为仓库 ZIP；输出为可追溯的 Parquet、Spark MLlib 模型、评估报告和 Web ADS JSON。现有 C++ 业务代码不参与离线计算。

## 快速启动

```bash
cd /path/to/charging
python3 -m venv .venv-bigdata
.venv-bigdata/bin/pip install -r bigdata/requirements.txt
bash bigdata/scripts/check_environment.sh
# 可迁移目录：dev.yaml 默认目录相对仓库根路径
export BIGDATA_ROOT="file://$PWD/.bigdata/lake"
export RUN_ID="$(date +%Y%m%d_%H%M%S)"
bash bigdata/scripts/run_full_pipeline.sh bigdata/conf/dev.yaml
python3 -m http.server 8090 --bind 127.0.0.1 --directory code/web
```

浏览器访问 `http://127.0.0.1:8090`。首次无 ADS 时页面明确显示暂无结果，不展示随机数。Web 页面无需 C++ 服务；旧版已经编译进 Qt 可执行文件的首页需重新编译资源才会更新，推荐使用独立 HTTP 入口预览本模块。

版本锁定在 `conf/versions.env` 和 `requirements.txt`。本仓库当前已在 Python 3.11、Java 8、Spark / PySpark 3.4.1、Hadoop 3.2.1 的本机伪分布式 HDFS + YARN 环境完成全量训练验证。迁移到其他版本前仍需重新执行环境检查和全套测试。

## Hadoop / YARN

本机已有 Hadoop 安装的情况下：

```bash
bash scripts/hadoop-local.sh start
bash bigdata/scripts/check_environment.sh cluster
unset BIGDATA_ROOT
export HADOOP_CONF_DIR="$HOME/hadoop/etc/hadoop"
export RUN_ID="$(date +%Y%m%d_%H%M%S)"
bash bigdata/scripts/init_hdfs.sh bigdata/conf/cluster.yaml --run-id "$RUN_ID"
# 全流水线：master 显式传给 spark-submit
export SPARK_MASTER=yarn
bash bigdata/scripts/run_full_pipeline.sh bigdata/conf/cluster.yaml
```

脚本不会格式化已有 NameNode。新集群请先安装 Hadoop、配置 core-site/hdfs-site/yarn-site，再按 Hadoop 官方部署文档初始化。将 `cluster.yaml` 的 `root` 改成集群 NameNode 地址。3 节点时副本建议 2，伪分布式为 1；按实际可用内存分配 YARN 资源。

每个工作节点需有相同的 Python 环境路径（`PYSPARK_PYTHON`）和 Java。默认使用 YARN client 模式，驱动器在提交机。仅单机设置 `SPARK_LOCAL_IP=127.0.0.1`；多节点必须设置为执行节点可访问的提交机网卡地址，HDFS 地址也不能是 localhost。

分站训练可使用 6 个独立 YARN Application：

```bash
# 先完成全局模型，再批量训练
RUN_ID=your_run SPARK_MASTER=yarn bash bigdata/scripts/submit.sh train_global_load --config bigdata/conf/cluster.yaml
RUN_ID=your_run MAX_CONCURRENT=1 bash bigdata/scripts/submit_load_batches.sh bigdata/conf/cluster.yaml
```

每批 7–8 站，站点 ID 从真实 ZIP 读取；单批依次训练，默认一次仅提交一批，避免挤满 8 GB 伪分布式节点。多节点实测资源足够后可提高 `MAX_CONCURRENT`。每个 executor 2 核、2 GB，两个 executor；提交日志含 Application ID。

## 数据口径与层次

| 层 | 内容 | 路径 |
|---|---|---|
| ODS | 原始 CSV，SHA256、HDFS 校验和、行数 | `ods/beijing/<table>/ingest_date=YYYYMMDD/` |
| DWD | 明细、原始 JSON 值、异常标记、隔离区 | `dwd/<table>/dt=.../` |
| DWS | 15 分钟 / 小时负荷、行政区小时负荷、站点日运营、用户时段矩阵 | `dws/` |
| 特征 | 因果滞后、滚动统计、固定时间切分 | `features/` |
| 模型 | MLlib 模型、参数、特征、评估、生产指针 | `models/` |
| ADS | 运营、用户、质量、负荷预测 | `ads/` |
| 审计 | 每阶段起止、状态、Application ID、源清单 | `audit/<run_id>/` |

字段字典见 `conf/schema_contracts.yaml`。`jobs/common.py` 中的 `SPECS` 是执行时合同；自动化测试核对 CSV 原始表头。每日分区最多一个数据文件（按日期散列 repartition），没有站点二次分区。原始 ODS 同批次只允许相同字节重复导入；不同字节报错。DWD/DWS 为全量确定性覆盖，因此本版本日常运行也可全量重算，不会重复追加。增量 MERGE 与分区回放尚未作为默认运行模式。全流水线会自动完成字段质量画像、数值分布画像和 Hive 外部表注册，无需再手工补跑。

会话采用半开时间区间 `[created, ended)`，按与窗口重叠秒数分摊电量及费用。分摊前后逐会话校验；最后一个数据日之后的跨日尾段保留在 DWS，但缺少快照的尾段不作为训练/评估样本。小时订单数和用户数按会话开始统计，电量和费用按时间分摊。

快照保留 `in_use_raw/idle_raw/fault_raw/fault_alarm_raw`，状态计数按设备容量修正。遥测估计电量与结算电量分别统计。BMS 负电流是业务约定，保留并派生绝对值。错误行全部隔离，重复键全部隔离，避免任意挑选一条丢失证据。

## 负荷模型与真实评估

- 时间切分：训练 `<2026-06-01`，验证 `<2026-08-01`，测试 `<2026-09-11`，末日作为演示起点；前 168 小时用于特征预热。
- 4 个基线：上一小时、昨日同小时、上周同小时、最多 8 周同期均值。
- Spark MLlib 全局 GBT + 每站 GBT，平方误差训练，20 棵树、深度 5，固定随机种子。
- 另训练共享的 H1/H6/H24 直接 GBT：预测未来 1、6、24 小时累计站点结算电量，目标窗口严格落在同一时间分区内，避免跨越训练/验证/测试边界。
- 每个站点、每个预测跨度均在验证集上比较共享 GBT 与最多 8 周同期累计基线；验证冠军用于生产，测试集只用于最终报告，不据测试结果重新选模。
- 每天 00:00 作为回测起点，模型递归产生 24 小时，不能将未来实际负荷作为后续递归输入。
- 按验证集递归 24 小时 MAE 在分站、全局、四基线之间选取冠军。测试结果只用于报告与退化标记，不能再次据测试集挑选模型。
- 指标：MAE、RMSE、WMAPE、sMAPE、R²；测试期额外按时间顺序拆成三个回测块，检查跨时段稳定性。这是固定模型滚动起点评估，不宣称完成三个扩展训练窗的重新拟合。
- 预测区间采用验证期每个步长残差 P10/P90，为经验区间，未经覆盖率校准，不能当作严格概率保证。
- 一步 MLlib 模型同时导出无依赖树结构，递归阶段用同一组特征计算；自动核对 30 条 Spark 与导出树的预测一致性。
- 当前特征：站点、类型、设备数、小时/星期/月与周期编码、lag 1/2/3/6/12/24/48/168、前 6/24/168 小时均值/总体标准差/最大值/首尾斜率。另包含上一已关闭小时的温度、降水、天气/节假日、利用率、故障率、排队、订单与用户数。递归 24 小时时冻结这些起点前观测，明确标为持续性情景，不能读取未来实际天气或快照。正式天气预报接口仍可后续替换。

预计空闲/排队基于历史正功率除以在用桩数的中位数换算；缺少有效功率时结果为 null，不能随意填一个功率常数。所有 47 站均保留失败、基线回退与测试退化信息。

## 用户需求

168 个星期小时槽。每用户最后一次会话测试、倒数第二次验证、此前训练。稳定整数 ID 与映射留在 HDFS，不导出个人记录。

分别训练隐式 ALS 时段偏好和显式 ALS 电量；用户时段行为加入 90 天指数衰减与最近 30 天活跃度特征。验证集在 ALS、历史/全局基线和混合模型之间选择冠军，测试集只报告泛化结果。无法评分时按用户均值、时段均值、全局均值依次回退。新用户仅提供群体结果。

下一时段是从预测起点之后 168 小时中选择偏好最高的候选，不代表具有校准概率的精确出行承诺。Web 只呈现群体热力图、电量直方图与评估。

## 深度学习对照实验

`experiments/train_temporal_cnn.py` 提供不影响生产模型的 Temporal CNN 实验：输入每站过去 168 小时单桩负荷，通过三层一维卷积、站点嵌入和日历周期特征，同时预测 H1/H6/H24 累计负荷。数据仍由 Spark on YARN 从 HDFS 特征层导出，训练使用 PyTorch CPU；Hadoop 负责分布式存储、清洗和特征准备，神经网络优化由 PyTorch 完成。

```bash
RUN_ID=dl_export SPARK_MASTER=yarn \
  bash bigdata/scripts/submit.sh export_deep_learning_data --config bigdata/conf/cluster.yaml
python3 bigdata/experiments/train_temporal_cnn.py \
  --input .bigdata/experiments/dl_load_20260914_hadoop2.npz \
  --run-id temporal_cnn_trial
# 让 CNN 学习 8 周同期基线的修正量
python3 bigdata/experiments/train_temporal_cnn.py \
  --input .bigdata/experiments/dl_load_20260914_hadoop2.npz \
  --run-id residual_cnn_trial --residual
```

实验严格沿用训练/验证/测试时间边界，验证集用于比较，测试集只报告。模型和中间数组保存在忽略提交的 `.bigdata/experiments/`，可审计指标保存在 `reports/deep_learning_experiment.json` 和 `reports/deep_learning_residual_experiment.json`。只有验证集胜出且测试表现可接受时，才应进入下一轮按站点候选发布，不直接覆盖大屏冠军。

## 外部真实数据实验

新增 City of Boulder 官方开放充电交易数据通道，与北京模拟数据分库、分报告、分展示文件，不会把美国站点冒充北京站点。下载器对 ArcGIS 的 1,000 条分页限制做了断点续传、页行数校验和整体 SHA-256 清单。

```bash
# 1. 下载 148,136 条官方真实交易（本地原始数据默认被 Git 忽略）
python3 bigdata/scripts/download_real_boulder.py

# 2. PySpark 质量检查 + SparkSQL 小时聚合 + YARN 上的 H1/H6/H24 GBT
SPARK_MASTER=yarn PYSPARK_PYTHON=/private/tmp/charging-bigdata-venv/bin/python \
  bash bigdata/scripts/submit.sh prepare_real_boulder \
  --run-id real_20260915_v1 --max-stations 10

# 3. 共享 CNN 学习 8 周同期基线残差，模型与评估存入 HDFS
python3 bigdata/experiments/train_real_residual_cnn.py \
  --run-id real_cnn_20260915_v1 --epochs 10 --train-stride 3
```

清洗后的会话、隔离行、小时特征、Spark GBT 和 CNN 模型保存在 HDFS `/charging_real/boulder/`。页面候选输出为 `code/web/data/real-load-forecast.json`；它是外部实验数据，不自动覆盖北京模拟运营大屏。实验结果见 `reports/real_boulder_experiment.md`。

## 北京真实数据与混合大屏

北京主大屏使用 Figshare 发布的 **Beijing public charging transactions v2**（DOI `10.6084/m9.figshare.31952289.v2`，CC BY 4.0）。发布包包含 2025 年 1 月、7 月共约 854 万笔脱敏交易和 8,553 个站点元数据；下载脚本按发布方文件大小与 MD5 校验，原始大文件保存在 Git 忽略的 `.bigdata/source/real/beijing_figshare/`，并复制到 HDFS `/charging_real/beijing/ods/`。

数据只覆盖两个离散月份，因此建模时保留为两个独立连续时间段，绝不把 2–6 月填成零，也不宣称是全年连续数据。站点位置仅有约 1 km 网格编码，没有精确经纬度或公开站名；页面继续使用程序自带北京导航地图，用匿名站点与行政区统计展示，不伪造精确落点。公开数据也没有用户 ID、故障遥测与 BMS 状态，这些模块使用原北京模拟数据补齐，并在每张卡片和 JSON `field_sources` 中标明“模拟补齐”。

完整流程如下：

```bash
# 同一个 Python 环境需安装基础依赖与可选的 Torch 实验依赖
.venv-bigdata/bin/pip install -r bigdata/requirements.txt -r bigdata/requirements-deep-learning.txt

# 需要 HDFS、YARN 已启动，hdfs 与 spark-submit 可执行
PYTHON_CMD=.venv-bigdata/bin/python \
RUN_ID=beijing_demo bash bigdata/scripts/run_real_beijing_pipeline.sh

# 安装位置不在 PATH 时可显式指定，不需要修改脚本
PYTHON_CMD=.venv-bigdata/bin/python \
HDFS_CMD=/path/to/hadoop/bin/hdfs \
SPARK_SUBMIT=/path/to/spark/bin/spark-submit \
RUN_ID=beijing_demo bash bigdata/scripts/run_real_beijing_pipeline.sh
```

该脚本依次执行：官方文件下载与校验 → HDFS ODS → PySpark 质量检查/去重/隔离 → SparkSQL 运营聚合 → 20 个高活跃站点小时序列 → YARN 上 H1/H6/H24 Spark GBT → PyTorch 残差 Temporal CNN → 七份同 generation 的大屏 JSON。Hadoop 负责分布式存储、清洗、特征和 GBT 训练；CNN 的梯度优化由 PyTorch CPU 完成，模型与评估结果再归档到 HDFS。这个边界会在训练报告中保留，不把本地 PyTorch 描述成 Hadoop 原生深度学习。

本次真实运行的质量统计、全局测试指标和分站点冠军数量见 `reports/real_beijing_experiment.md`。

## 大屏契约和失败保护

七类数据：overview、stations、station-ranking、load-forecast、user-demand、data-quality、pipeline-health。统一包含 `schemaVersion/dataNature/dataTime/generatedAt/runId/data`。

导出先校验所有 JSON，再写入不可变 `data/runs/<generation>/`，最后原子替换 `data/current.json`。页面以同一 generation 读取七个文件，避免不同运行混读。固定文件名保留给外部使用者。导出失败不会移动 current 指针；运行脚本失败写 `latest-attempt.json`。刷新失败保留页面中已有结果并明确标识降级。超过一天未更新会提示延迟；历史数据日期始终展示。

## 验证和复现

```bash
.venv-bigdata/bin/python -m pytest bigdata/tests -q
# 检查模型指标 / 47×24覆盖，生成正式报告
RUN_ID=your_run bash bigdata/scripts/submit.sh evaluate_models --config bigdata/conf/cluster.yaml
RUN_ID=your_run bash bigdata/scripts/submit.sh train_direct_load --config bigdata/conf/cluster.yaml
RUN_ID=your_run bash bigdata/scripts/submit.sh predict_station_load --config bigdata/conf/cluster.yaml
RUN_ID=your_run bash bigdata/scripts/export_dashboard_json.sh --config bigdata/conf/cluster.yaml
```

验收记录见 `ACCEPTANCE.md`；部署与演示见 `DEMO.md`。依赖文档：
- [Spark 3.4.1](https://spark.apache.org/docs/3.4.1/)
- [Spark on YARN](https://spark.apache.org/docs/3.4.1/running-on-yarn.html)
- [Spark ALS](https://spark.apache.org/docs/3.4.1/ml-collaborative-filtering.html)

不会把模拟数据说成真实运营数据，也不保证机器学习必然优于基线。P2 的 15 分钟模型、What-if 和 BMS 故障诊断不在当前实现内。
