# 验收记录

本文件记录一次完整全量运行的验收结果，供复现对照。口径与方法定义见 [README.md](README.md)，现场演示见 [DEMO.md](DEMO.md)。所有数字来自模拟数据，不代表真实运营或商业预测能力。

## 运行标识

| 项 | 值 |
|---|---|
| run_id | `20260913_full` |
| 大屏 generation | `20260913_full_0de41f5c` |
| 存储根 | `hdfs://localhost:9000/charging` |
| 调度 | Spark on YARN（伪分布式，1 个 RUNNING NodeManager） |
| 数据截止 | 2026-09-11 23:00（演示起点） |
| 导出时间 | 2026-09-13 11:37:38 +08:00 |
| 链路状态 | `SUCCESS`（18 个审计阶段全部成功） |

环境版本锁定见 `conf/versions.env`：Python 3.13、Java 21、Spark 4.0.1、Hadoop 3.4.3、PySpark 4.0.1。

## 数据规模

| 源表 | 行数 |
|---|---|
| station | 47 |
| weather | 619 |
| session | 572,539 |
| snapshot | 2,792,928 |
| bms | 2,290,411 |

ODS 摄取含 SHA256、HDFS 校验和与行数；`audit/20260913_full/source_manifest.json` 可追溯。各阶段审计 JSON 与 Application ID 见 `hdfs:///charging/audit/20260913_full/`。

## 分层产物

| 层 | 路径 | 状态 |
|---|---|---|
| ODS | `/charging/ods` | ✅ |
| DWD | `/charging/dwd` | ✅ |
| DWS | `/charging/dws`（15 分钟 / 小时 / 日运营 / 用户时段矩阵） | ✅ |
| 特征 | `/charging/features`（load、user_demand） | ✅ |
| 模型 | `/charging/models`（全局 + 47 分站 + user_mapping） | ✅ |
| ADS | `/charging/ads` | ✅ |

## 数据质量

29 条规则，ERROR 命中 0 行。异常按设计保留原值或隔离，不静默丢弃：

- `session.cross_midnight` INFO 42,112 行（跨日尾段保留）
- `snapshot.capacity_conflict` WARN 3,319 行（按设备容量修正）
- `snapshot.zero_cumulative` WARN 2,792,928 行（恒零累计量告警）
- `bms.negative_current` INFO 2,290,411 行（BMS 负电流为业务约定，保留并派生绝对值）

## 负荷预测

时间切分：训练 `<2026-06-01`、验证 `<2026-08-01`、测试 `<2026-09-11`。测试期 47 站 × 46248 小时。验证集每日递归 24 小时 MAE 选冠军，测试集仅用于报告，不参与选型。

| 候选 | 测试 MAE (kWh) | RMSE |
|---|---|---|
| 上一小时基线 | 23.78 | 34.31 |
| 昨日同小时基线 | 17.16 | 27.58 |
| 上周同小时基线 | 16.17 | 26.10 |
| **8 周同期均值（发布）** | **12.46** | **19.59** |
| 全局 GBT | 17.35 | 28.48 |
| 分站 GBT | 16.04 | 26.75 |

**结论（如实记录）：47 个站点全部回退 `8week` 基线，`needs_optimization=47`、`test_degraded=0`、`improvement=0.0`。** 分站 GBT 优于全局 GBT 与其他三条朴素基线，但未超过 8 周同期均值。按设计发布验证集冠军，不因测试集表现重新挑模型，也不强行宣称 ML 胜出。此结果说明当前特征/切分下 GBT 尚未体现相对季节朴素基线的增益，属于待优化项而非运行故障。

每站导出未来 24 点，含 85% 容量阈值预警与验证期残差 P10/P90 经验区间（未经覆盖率校准）。预测的空闲/排队为历史正功率换算估计，缺有效功率时为 null。

## 用户需求

168 个星期小时槽；每用户最后一次会话测试、倒数第二次验证、此前训练；导出只含群体聚合，无用户明细。2000 用户全覆盖，新用户只展示群体分布。

| 任务 | 发布模型 | 测试指标 |
|---|---|---|
| 下一时段 | 全局热门时段 | HitRate@3 0.0515 |
| 下一次电量 | 用户历史均值 | MAE 8.27 kWh |

测试集上显式 ALS 电量 MAE 8.60、隐式 ALS 时段 HitRate@3 0.0655，按验证集规则未选为发布模型。回退链：用户均值 → 同时段均值 → 全局均值。

## 大屏契约与导出

七类 JSON（overview、stations、station-ranking、load-forecast、user-demand、data-quality、pipeline-health）统一 `schemaVersion=1.0 / dataNature=模拟数据 / runId=20260913_full`，校验全部通过；`load-forecast` 覆盖 47 站 × 24 点。

导出先校验全部 schema，再写不可变 `code/web/data/runs/20260913_full_0de41f5c/`，最后原子替换 `current.json`。页面按同一 generation 读取，避免混读。

预览：`python3 -m http.server 8090 --bind 127.0.0.1 --directory code/web`，访问 `http://127.0.0.1:8090`。

## 自动化测试

```
.venv-bigdata/bin/python -m pytest bigdata/tests -q
# 15 passed
```

覆盖能量守恒、时间泄漏、模型指标、schema 契约、导出校验、用户回退等。

## 复现命令

```bash
export RUN_ID=20260913_full
export SPARK_MASTER=yarn
bash bigdata/scripts/submit.sh evaluate_models      --config bigdata/conf/cluster.yaml --run-id "$RUN_ID"
bash bigdata/scripts/submit.sh predict_station_load --config bigdata/conf/cluster.yaml --run-id "$RUN_ID"
bash bigdata/scripts/export_dashboard_json.sh       --config bigdata/conf/cluster.yaml --run-id "$RUN_ID"
```

## 已知边界

- 模拟数据 47 站 619 天，不能作为真实场景预测能力背书。
- HDFS/YARN 为单节点接口验证；三节点网络、权限、容量需在真实集群另验。
- 47 站均回退 8 周基线，负荷 GBT 增益待优化；P2 的 15 分钟模型、What-if、BMS 故障诊断未实现。
- 预测区间为经验分位数，无严格覆盖率保证；递归阶段外生变量冻结在起点前，属持续性情景。
- 三个时间回测块复用训练期模型，不是三次扩展训练集重训。
