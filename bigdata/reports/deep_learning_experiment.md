# Temporal CNN 负荷预测对照实验

- 实验批次：`20260914_tcnn1`
- 特征来源：HDFS `features/load/version=20260914_hadoop2`
- Spark/YARN 导出任务：`application_1789135312450_0035`
- 算法：共享 Temporal CNN（3 层 Conv1D + 站点嵌入 + 日历周期特征）
- 输入：过去 168 小时单桩负荷
- 输出：未来 H1/H6/H24 累计结算电量
- 参数量：6,683
- 样本：训练 94,235，验证 67,727，测试 45,167
- 训练：12 epoch、固定随机种子、SmoothL1、AdamW；验证集选最佳权重

## 测试集结果

| 预测跨度 | Temporal CNN MAE | 共享 GBT MAE | 8 周基线 MAE | 当前结论 |
|---|---:|---:|---:|---|
| H1 | 12.20 kWh | **8.74 kWh** | 12.46 kWh | 保留 GBT |
| H6 | 52.40 kWh | 51.19 kWh | **49.47 kWh** | 保留验证选模/基线 |
| H24 | **125.17 kWh** | 127.54 kWh | 126.03 kWh | 可作为下一轮候选 |

H24 的 CNN 测试 MAE 比单一共享 GBT 低约 1.9%，比单一 8 周基线低约 0.7%。但当前生产策略会为每个站点在验证集上分别选择 GBT 或基线，其测试集平均 MAE 约为 123.10 kWh，仍优于本次共享 CNN 约 1.7%。因此本实验不直接替换大屏模型。

## 结论

深度学习并非天然优于树模型。本数据的小时级规律和周期性较强，H1 的滞后统计特征非常适合 GBT；H6 的 8 周同期基线仍更稳定。后续已经完成 CNN 残差学习实验，结果见 `deep_learning_residual_experiment.md`。

完整机器可读报告见 `deep_learning_experiment.json`。测试集没有参与模型选择。
