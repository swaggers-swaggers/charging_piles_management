"""Export the current HDFS load feature series for a reproducible PyTorch experiment."""
from common import *
from build_load_features import FEATURES
import numpy as np
from pyspark.ml.feature import VectorAssembler
from pyspark.ml.regression import GBTRegressionModel


SPLIT_CODE = {'train': 0, 'validation': 1, 'test': 2, 'demo': 3}


def export(s, c, a):
    feature_meta = read_json(s, c, 'features/load/current.json')
    version = feature_meta['version']
    data = read(s, c, 'features/load/version=' + version)
    direct = read_json(s, c, 'models/load/direct/production.json')
    vectorized = VectorAssembler(inputCols=FEATURES, outputCol='features',
                                 handleInvalid='error').transform(data).persist()
    scored = data.select('station_id', 'event_hour', 'load_kwh', 'device_count',
                         'facility_type', 'split')
    for horizon in (1, 6, 24):
        model_path = path(c, f'models/load/direct_horizon={horizon}/version=' +
                          direct['version'] + '/model')
        model = GBTRegressionModel.load(model_path)
        prediction = (model.transform(vectorized)
                      .select('station_id', 'event_hour',
                              F.greatest(F.lit(0.), F.col('prediction'))
                              .alias(f'gbt_h{horizon}')))
        scored = scored.join(prediction, ['station_id', 'event_hour'], 'inner')
    vectorized.unpersist()
    rows = (scored
            .select('station_id', 'event_hour', 'load_kwh', 'device_count',
                    'facility_type', 'split', 'gbt_h1', 'gbt_h6', 'gbt_h24')
            .orderBy('station_id', 'event_hour')
            .collect())
    output_dir = REPO / '.bigdata' / 'experiments'
    output_dir.mkdir(parents=True, exist_ok=True)
    output = output_dir / f'dl_load_{version}.npz'
    np.savez_compressed(
        output,
        station_id=np.asarray([r.station_id for r in rows], dtype=np.int32),
        event_time=np.asarray([int(r.event_hour.timestamp()) for r in rows], dtype=np.int64),
        load_kwh=np.asarray([float(r.load_kwh) for r in rows], dtype=np.float32),
        device_count=np.asarray([float(r.device_count) for r in rows], dtype=np.float32),
        facility_type=np.asarray([int(r.facility_type) for r in rows], dtype=np.int16),
        split=np.asarray([SPLIT_CODE[r.split] for r in rows], dtype=np.int8),
        gbt_h1=np.asarray([float(r.gbt_h1) for r in rows], dtype=np.float32),
        gbt_h6=np.asarray([float(r.gbt_h6) for r in rows], dtype=np.float32),
        gbt_h24=np.asarray([float(r.gbt_h24) for r in rows], dtype=np.float32),
    )
    metadata = {
        'feature_version': version,
        'source': path(c, 'features/load/version=' + version),
        'output': str(output),
        'rows': len(rows),
        'stations': len({r.station_id for r in rows}),
        'split_codes': SPLIT_CODE,
        'exported_at': now(),
        'application_id': s.sparkContext.applicationId,
        'direct_model_version': direct['version'],
    }
    (output_dir / f'dl_load_{version}.json').write_text(
        json.dumps(metadata, ensure_ascii=False, indent=2), encoding='utf-8')
    return metadata


if __name__ == '__main__':
    run('export_dl_data', export)
