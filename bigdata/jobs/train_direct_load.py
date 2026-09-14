"""Train shared, device-normalized direct GBT models for H=1/6/24 hours."""
from common import *
from build_load_features import FEATURES
from modeling import metrics, serialize, score
from pyspark.ml.feature import VectorAssembler
from pyspark.ml.regression import GBTRegressor
from pyspark.sql import Window

HORIZONS = (1, 6, 24)


def direct_frame(data, horizon, c):
    """Features at t use observations through t-1; label covers [t, t+H)."""
    future = Window.partitionBy('station_id').orderBy('event_hour').rowsBetween(0, horizon - 1)
    frame = (data
        .withColumn('target_kwh', F.sum('load_kwh').over(future))
        .withColumn('baseline_kwh', F.sum('baseline_8week').over(future))
        .withColumn('target_count', F.count('load_kwh').over(future))
        .withColumn('target_end', F.max('event_hour').over(future))
        .filter(F.col('target_count') == horizon)
        .withColumn('label_per_device', F.col('target_kwh') / F.col('device_count')))
    return frame.withColumn(
        'direct_split',
        F.when(F.col('target_end') < F.lit(c['train_end']), 'train')
         .when((F.col('event_hour') >= F.lit(c['train_end'])) & (F.col('target_end') < F.lit(c['validation_end'])), 'validation')
         .when((F.col('event_hour') >= F.lit(c['validation_end'])) & (F.col('target_end') < F.lit(c['test_end'])), 'test')
         .otherwise('excluded'))


def metric_map(frame, prediction):
    rows = (frame
        .withColumn('_prediction', F.greatest(F.lit(0.), F.col(prediction)))
        .withColumn('_error', F.col('_prediction') - F.col('target_kwh'))
        .groupBy('station_id')
        .agg(F.count('*').alias('n'), F.avg(F.abs('_error')).alias('mae'),
             F.sqrt(F.avg(F.col('_error') * F.col('_error'))).alias('rmse'),
             (F.sum(F.abs('_error')) / F.sum(F.abs('target_kwh'))).alias('wmape'),
             F.avg(2 * F.abs('_error') / F.greatest(F.abs('target_kwh') + F.abs('_prediction'), F.lit(1e-9))).alias('smape'),
             (F.lit(1.) - F.sum(F.col('_error') * F.col('_error')) /
              F.greatest(F.var_pop('target_kwh') * F.count('*'), F.lit(1e-9))).alias('r2'))
        .collect())
    return {str(r.station_id): {k: (int(v) if k == 'n' else float(v) if v is not None else None)
                                for k, v in r.asDict().items() if k != 'station_id'} for r in rows}


def aggregate_metrics(values):
    values = [v for v in values if v and v.get('n')]
    if not values:
        return metrics([], [])
    n = sum(v['n'] for v in values)
    return dict(n=n,
                mae=sum(v['mae'] * v['n'] for v in values) / n,
                rmse=(sum(v['rmse'] ** 2 * v['n'] for v in values) / n) ** .5,
                wmape=sum(v['wmape'] * v['n'] for v in values) / n,
                smape=sum(v['smape'] * v['n'] for v in values) / n,
                r2=sum(v['r2'] * v['n'] for v in values) / n)


def train(s, c, a):
    data = read(s, c, 'features/load/version=' + read_json(s, c, 'features/load/current.json')['version']).persist()
    assembler = VectorAssembler(inputCols=FEATURES, outputCol='features', handleInvalid='error')
    report = dict(version=c['run_id'], feature_version=read_json(s, c, 'features/load/current.json')['version'],
                  target='未来 H 小时累计结算电量 / 设备数', scope='47 站共享模型', horizons={})
    for horizon in HORIZONS:
        frame = direct_frame(data, horizon, c).persist()
        prepared = assembler.transform(frame.filter(F.col('direct_split') == 'train')).select(
            'features', F.col('label_per_device').cast('double').alias('label')).persist()
        estimator = GBTRegressor(maxIter=c['gbt_iterations'], maxDepth=c['gbt_depth'], seed=20260912,
                                 stepSize=.1, lossType='squared', maxBins=64)
        model = estimator.fit(prepared)
        prepared.unpersist()
        base = f'models/load/direct_horizon={horizon}/version={c["run_id"]}'
        model.write().overwrite().save(path(c, base + '/model'))
        portable = serialize(model)
        write_json(s, c, base + '/portable.json', portable)
        probe = assembler.transform(frame.filter(F.col('direct_split') == 'train').limit(30))
        checked = model.transform(probe).select('features', 'prediction').collect()
        if any(abs(score(portable, list(r.features)) - max(0., r.prediction)) > 1e-7 for r in checked):
            raise ValueError('Direct portable tree prediction mismatch')
        scored = (model.transform(assembler.transform(frame))
            .withColumn('shared_prediction_kwh', F.greatest(F.lit(0.), F.col('prediction')) * F.col('device_count')))
        validation = scored.filter(F.col('direct_split') == 'validation').persist()
        test = scored.filter(F.col('direct_split') == 'test').persist()
        vv, vb = metric_map(validation, 'shared_prediction_kwh'), metric_map(validation, 'baseline_kwh')
        tv, tb = metric_map(test, 'shared_prediction_kwh'), metric_map(test, 'baseline_kwh')
        stations = {}
        for sid in sorted(set(vv) | set(vb)):
            winner = 'shared_gbt' if vv[sid]['mae'] < vb[sid]['mae'] else '8week'
            stations[sid] = dict(winner=winner, validation={'shared_gbt': vv[sid], '8week': vb[sid]},
                                 test={'shared_gbt': tv[sid], '8week': tb[sid]})
        report['horizons'][str(horizon)] = dict(
            model_path=path(c, base + '/model'), stations=stations,
            validation={'shared_gbt': aggregate_metrics(vv.values()), '8week': aggregate_metrics(vb.values())},
            test={'shared_gbt': aggregate_metrics(tv.values()), '8week': aggregate_metrics(tb.values())})
        write_json(s, c, base + '/manifest.json', dict(version=c['run_id'], horizon=horizon,
                   feature_version=report['feature_version'], features=FEATURES,
                   target=report['target'], scope=report['scope'], application_id=s.sparkContext.applicationId,
                   feature_importance=dict(zip(FEATURES, [float(v) for v in model.featureImportances])), trained_at=now()))
        validation.unpersist(); test.unpersist(); frame.unpersist()
    write_json(s, c, 'models/load/direct/production.json', report)
    data.unpersist()
    return {h: len(report['horizons'][str(h)]['stations']) for h in HORIZONS}


if __name__ == '__main__':
    run('train_direct', train)
