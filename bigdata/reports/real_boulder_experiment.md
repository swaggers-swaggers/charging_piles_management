# 真实充电站负荷预测实验

## 数据与口径

- 数据源：[City of Boulder 官方 Electric Vehicle Charging Station Data](https://www.arcgis.com/sharing/rest/content/items/95992b3938be4622b07f0b05eba95d4c/info/metadata/metadata.xml?format=default&output=html)，数据由[ ArcGIS FeatureServer](https://services.arcgis.com/ePKBjXrBZ2vEEgWd/arcgis/rest/services/Electric_Vehicle_Charging_Station_Data/FeatureServer/0) 获取。
- 授权：官方元数据标注 CC0 1.0 Public Domain。
- 原始记录：148,136 条真实充电交易，本次下载清单 SHA-256 为 `9a5318f87972ad916c71efc9aeffe2089396c2983f77eca4e9011923e00c8675`。
- 数据分类：外部真实数据，不代表北京运营情况。
- 负荷口径：原始数据只有会话总电量与充电时长，因此将 kWh 从开始时间起在报告充电时长内均匀分摊到小时。这是可复现估计，不是桩级秒采样功率曲线。

## PySpark 质量结果

YARN Application `application_1789135312450_0041`，运行编号 `real_20260915_v1`。

| 项目 | 数量 |
|---|---:|
| 原始记录 | 148,136 |
| 去重后记录 | 148,136 |
| 清洗保留 | 124,640 |
| 隔离异常 | 23,496 |
| 选中站点 | 10 |
| 连续小时点 | 436,931 |

异常明细：无效电量 13,426，无效时间戳 7,820，无效总时长 2,248，无效充电时长 2。异常行写入 HDFS 隔离区，没有静默丢弃。

## 模型与结果

- Spark GBT：滞后 1/2/24/48/168 小时、24/168 小时滚动均值、小时/星期周期特征；在 YARN 上分布式训练。
- 残差 CNN：输入过去 168 小时，Conv1D + 站点嵌入 + 日历特征，学习 8 周同期基线的修正量。
- 切分：各站最后 60 天为测试，再向前 60 天为验证，其余为训练。验证集选模，测试集只报告。
- CNN 样本：训练 135,406，验证 14,170，测试 14,184；完成 10 轮。

| 预测窗口 | 测试集整体表现最优（仅报告） | MAE (kWh) | RMSE (kWh) | WMAPE | CNN 按验证集逐站胜出 |
|---|---|---:|---:|---:|---:|
| H1 | Spark GBT | 1.337 | 2.502 | 64.64% | 5 / 10 |
| H6 | 残差 CNN | 7.633 | 11.591 | 61.51% | 9 / 10 |
| H24 | 残差 CNN | 21.274 | 27.710 | 42.92% | 6 / 10 |

H1/H6/H24 表示从同一预测起点开始，未来 1/6/24 小时累计电量。由于小时级数据大量为零，WMAPE 会明显高于 MAE 给人的直观感受；本报告不将 `1 - WMAPE` 包装为“准确率”。

## 产物

- Spark 报告：`bigdata/reports/real_boulder_spark_run.json`
- CNN 报告：`bigdata/reports/real_boulder_cnn_experiment.json`
- HDFS 小时特征：`/charging_real/boulder/features/hourly/version=real_20260915_v1`
- HDFS CNN：`/charging_real/boulder/models/residual_cnn/version=real_cnn_20260915_v1`
- Web 候选预测：`code/web/data/real-load-forecast.json`
