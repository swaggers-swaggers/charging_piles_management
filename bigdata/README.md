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

版本锁定在 `conf/versions.env` 和 `requirements.txt`。实测环境为 Python 3.13、Java 21、Spark 4.0.1、Hadoop 3.4.3。Spark 4 支持 Java 17/21；Hadoop 守护进程的 JDK 兼容性必须在部署机器独立确认，不据此推断其他 Hadoop 版本也支持 Java 21。

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
| DWS | 15 分钟 / 小时负荷、日运营、用户时段矩阵 | `dws/` |
| 特征 | 因果滞后、滚动统计、固定时间切分 | `features/` |
| 模型 | MLlib 模型、参数、特征、评估、生产指针 | `models/` |
| ADS | 运营、用户、质量、负荷预测 | `ads/` |
| 审计 | 每阶段起止、状态、Application ID、源清单 | `audit/<run_id>/` |

字段字典见 `conf/schema_contracts.yaml`。`jobs/common.py` 中的 `SPECS` 是执行时合同；自动化测试核对 CSV 原始表头。每日分区最多一个数据文件（按日期散列 repartition），没有站点二次分区。原始 ODS 同批次只允许相同字节重复导入；不同字节报错。DWD/DWS 为全量确定性覆盖，因此本版本日常运行也可全量重算，不会重复追加。增量 MERGE 与分区回放尚未作为默认运行模式。

会话采用半开时间区间 `[created, ended)`，按与窗口重叠秒数分摊电量及费用。分摊前后逐会话校验；最后一个数据日之后的跨日尾段保留在 DWS，但缺少快照的尾段不作为训练/评估样本。小时订单数和用户数按会话开始统计，电量和费用按时间分摊。

快照保留 `in_use_raw/idle_raw/fault_raw/fault_alarm_raw`，状态计数按设备容量修正。遥测估计电量与结算电量分别统计。BMS 负电流是业务约定，保留并派生绝对值。错误行全部隔离，重复键全部隔离，避免任意挑选一条丢失证据。

## 负荷模型与真实评估

- 时间切分：训练 `<2026-06-01`，验证 `<2026-08-01`，测试 `<2026-09-11`，末日作为演示起点；前 168 小时用于特征预热。
- 4 个基线：上一小时、昨日同小时、上周同小时、最多 8 周同期均值。
- Spark MLlib 全局 GBT + 每站 GBT，平方误差训练，20 棵树、深度 5，固定随机种子。
- 每天 00:00 作为回测起点，模型递归产生 24 小时，不能将未来实际负荷作为后续递归输入。
- 按验证集递归 24 小时 MAE 在分站、全局、四基线之间选取冠军。测试结果只用于报告与退化标记，不能再次据测试集挑选模型。
- 指标：MAE、RMSE、WMAPE、sMAPE、R²；测试期额外按时间顺序拆成三个回测块，检查跨时段稳定性。这是固定模型滚动起点评估，不宣称完成三个扩展训练窗的重新拟合。
- 预测区间采用验证期每个步长残差 P10/P90，为经验区间，未经覆盖率校准，不能当作严格概率保证。
- 一步 MLlib 模型同时导出无依赖树结构，递归阶段用同一组特征计算；自动核对 30 条 Spark 与导出树的预测一致性。
- 当前特征：站点、类型、设备数、小时/星期/月与周期编码、lag 1/2/3/6/12/24/48/168、前 6/24/168 小时均值/总体标准差/最大值/首尾斜率。另包含上一已关闭小时的温度、降水、天气/节假日、利用率、故障率、排队、订单与用户数。递归 24 小时时冻结这些起点前观测，明确标为持续性情景，不能读取未来实际天气或快照。正式天气预报接口仍可后续替换。

预计空闲/排队基于历史正功率除以在用桩数的中位数换算；缺少有效功率时结果为 null，不能随意填一个功率常数。所有 47 站均保留失败、基线回退与测试退化信息。

## 用户需求

168 个星期小时槽。每用户最后一次会话测试、倒数第二次验证、此前训练。稳定整数 ID 与映射留在 HDFS，不导出个人记录。

分别训练隐式 ALS 时段偏好和显式 ALS 电量；验证集决定采用 ALS 还是基线。时段比较全局热门/个人历史偏好，电量比较用户均值/时段均值，报告真实测试集指标。无法评分时按用户均值、时段均值、全局均值依次回退。新用户仅提供群体结果。

下一时段是从预测起点之后 168 小时中选择偏好最高的候选，不代表具有校准概率的精确出行承诺。Web 只呈现群体热力图、电量直方图与评估。

## 大屏契约和失败保护

七类数据：overview、stations、station-ranking、load-forecast、user-demand、data-quality、pipeline-health。统一包含 `schemaVersion/dataNature/dataTime/generatedAt/runId/data`。

导出先校验所有 JSON，再写入不可变 `data/runs/<generation>/`，最后原子替换 `data/current.json`。页面以同一 generation 读取七个文件，避免不同运行混读。固定文件名保留给外部使用者。导出失败不会移动 current 指针；运行脚本失败写 `latest-attempt.json`。刷新失败保留页面中已有结果并明确标识降级。超过一天未更新会提示延迟；历史数据日期始终展示。

## 验证和复现

```bash
.venv-bigdata/bin/python -m pytest bigdata/tests -q
# 检查模型指标 / 47×24覆盖，生成正式报告
RUN_ID=your_run bash bigdata/scripts/submit.sh evaluate_models --config bigdata/conf/cluster.yaml
RUN_ID=your_run bash bigdata/scripts/submit.sh predict_station_load --config bigdata/conf/cluster.yaml
RUN_ID=your_run bash bigdata/scripts/export_dashboard_json.sh --config bigdata/conf/cluster.yaml
```

验收记录见 `ACCEPTANCE.md`；部署与演示见 `DEMO.md`。依赖文档：
- [Spark 4.0.1](https://spark.apache.org/docs/4.0.1/)
- [Spark on YARN](https://spark.apache.org/docs/4.0.1/running-on-yarn.html)
- [Spark ALS](https://spark.apache.org/docs/4.0.1/ml-collaborative-filtering.html)

不会把模拟数据说成真实运营数据，也不保证机器学习必然优于基线。P2 的 15 分钟模型、What-if 和 BMS 故障诊断不在当前实现内。
