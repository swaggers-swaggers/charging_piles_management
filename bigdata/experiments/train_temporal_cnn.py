"""Train and evaluate a shared temporal CNN without changing production selection."""
from __future__ import annotations

import argparse
import copy
import datetime as dt
import json
import math
import os
import random
import subprocess
from pathlib import Path

import numpy as np
import torch
from torch import nn


REPO = Path(__file__).resolve().parents[2]
HORIZONS = (1, 6, 24)
LOOKBACK = 168
SPLITS = {'train': 0, 'validation': 1, 'test': 2}


class ArrayBundle(dict):
    @property
    def files(self):
        return list(self.keys())


def load_array_bundle(path):
    path = Path(path).resolve()
    if path.suffix == '.npz':
        return np.load(path)
    rows = [json.loads(line) for line in path.read_text(encoding='utf-8').splitlines() if line]
    if not rows:
        raise ValueError(f'Empty feature bundle: {path}')
    integer = {'station_id', 'event_time', 'local_hour', 'local_dow',
               'active_sessions', 'split'}
    return ArrayBundle({
        key: np.asarray([row[key] for row in rows],
                        dtype=np.int64 if key in integer else
                        (None if key == 'station_code' else np.float32))
        for key in rows[0]
    })


class TemporalCNN(nn.Module):
    def __init__(self, station_count: int, residual: bool = False):
        super().__init__()
        self.residual = residual
        self.encoder = nn.Sequential(
            nn.Conv1d(1, 16, kernel_size=5, padding=2), nn.ReLU(),
            nn.MaxPool1d(4),
            nn.Conv1d(16, 32, kernel_size=3, padding=1), nn.ReLU(),
            nn.MaxPool1d(3),
            nn.Conv1d(32, 32, kernel_size=3, padding=1), nn.ReLU(),
            nn.AdaptiveAvgPool1d(1),
        )
        self.station = nn.Embedding(station_count, 8)
        self.head = nn.Sequential(
            nn.Linear(32 + 8 + 4 + (len(HORIZONS) if residual else 0), 32),
            nn.ReLU(), nn.Dropout(.1),
            nn.Linear(32, len(HORIZONS)),
        )

    def forward(self, sequence, station, calendar, baseline=None):
        encoded = self.encoder(sequence[:, None, :]).squeeze(-1)
        parts = [encoded, self.station(station), calendar]
        if self.residual:
            parts.append(baseline)
        return self.head(torch.cat(parts, dim=1))


def calendar_features(epoch_seconds, local_hours=None, local_days=None):
    # Beijing exports use Asia/Shanghai. External real-data bundles may carry
    # source-local hour/day arrays so DST and timezone are not guessed here.
    if local_hours is None:
        local_hours = ((epoch_seconds // 3600) + 8) % 24
    if local_days is None:
        local_days = ((epoch_seconds // 86400) + 3) % 7  # 1970-01-01 was Thursday.
    return np.column_stack((
        np.sin(local_hours * 2 * np.pi / 24), np.cos(local_hours * 2 * np.pi / 24),
        np.sin(local_days * 2 * np.pi / 7), np.cos(local_days * 2 * np.pi / 7),
    )).astype(np.float32)


def baseline_at(values, end, times=None):
    """Return a weekly seasonal baseline without bridging missing calendar spans.

    Some public data releases contain separate observation months.  Index-only
    lagging would silently treat the end of one month as adjacent to the start
    of another, so timestamp continuity is verified whenever epochs are given.
    """
    output = []
    for horizon in HORIZONS:
        total = 0.
        for offset in range(horizon):
            history = []
            target_epoch = None
            if times is not None:
                if end + offset < len(times):
                    target_epoch = int(times[end + offset])
                else:
                    target_epoch = int(times[-1]) + (end + offset - len(times) + 1) * 3600
            for week in range(1, 9):
                index = end + offset - 168 * week
                if index < 0:
                    continue
                if target_epoch is not None and target_epoch - int(times[index]) != 168 * week * 3600:
                    continue
                history.append(values[index])
            if not history:
                index = end + offset - 24
                if index >= 0 and (target_epoch is None or
                                   target_epoch - int(times[index]) == 24 * 3600):
                    history.append(values[index])
            total += float(np.mean(history)) if history else 0.0
        output.append(total)
    return output


def build_samples(raw, split_name, station_lookup, stride):
    code = SPLITS[split_name]
    sequences, stations, calendars, targets, baselines, gbts, devices = [], [], [], [], [], [], []
    for station_id in sorted(station_lookup):
        mask = raw['station_id'] == station_id
        times = raw['event_time'][mask]
        station_calendars = calendar_features(
            times,
            raw['local_hour'][mask] if 'local_hour' in raw.files else None,
            raw['local_dow'][mask] if 'local_dow' in raw.files else None)
        device = raw['device_count'][mask]
        values = raw['load_kwh'][mask] / np.maximum(device, 1.)
        splits = raw['split'][mask]
        gbt_values = np.column_stack([raw[f'gbt_h{horizon}'][mask]
                                      for horizon in HORIZONS])
        for end in range(LOOKBACK, len(values) - max(HORIZONS) + 1, stride):
            target_end = end + max(HORIZONS) - 1
            if splits[end] != code or splits[target_end] != code:
                continue
            if times[target_end] - times[end - LOOKBACK] != (LOOKBACK + max(HORIZONS) - 1) * 3600:
                continue
            sequence = values[end - LOOKBACK:end]
            if not np.isfinite(sequence).all():
                continue
            target = [values[end:end + horizon].sum() for horizon in HORIZONS]
            baseline = baseline_at(values, end, times)
            sequences.append(sequence)
            stations.append(station_lookup[station_id])
            calendars.append(station_calendars[end])
            targets.append(target)
            baselines.append(baseline)
            gbts.append(gbt_values[end])
            devices.append(device[end])
    return {
        'x': np.asarray(sequences, dtype=np.float32),
        'station': np.asarray(stations, dtype=np.int64),
        'calendar': np.asarray(calendars, dtype=np.float32),
        'target': np.asarray(targets, dtype=np.float32),
        'baseline': np.asarray(baselines, dtype=np.float32),
        'gbt': np.asarray(gbts, dtype=np.float32),
        'device': np.asarray(devices, dtype=np.float32),
    }


def build_origins(raw, station_lookup):
    sequences, stations, calendars, baselines, devices = [], [], [], [], []
    for station_id in sorted(station_lookup):
        mask = raw['station_id'] == station_id
        times = raw['event_time'][mask]
        if 'local_hour' in raw.files:
            next_hour = (int(raw['local_hour'][mask][-1]) + 1) % 24
            next_day = (int(raw['local_dow'][mask][-1]) +
                        (1 if int(raw['local_hour'][mask][-1]) == 23 else 0)) % 7
            next_calendar = calendar_features(
                np.asarray([times[-1] + 3600]), np.asarray([next_hour]),
                np.asarray([next_day]))[0]
        else:
            next_calendar = calendar_features(np.asarray([times[-1] + 3600]))[0]
        device = raw['device_count'][mask]
        values = raw['load_kwh'][mask] / np.maximum(device, 1.)
        end = len(values)
        if end < LOOKBACK:
            raise ValueError(f'Insufficient origin history for station {station_id}')
        sequences.append(values[-LOOKBACK:])
        stations.append(station_lookup[station_id])
        calendars.append(next_calendar)
        baselines.append(baseline_at(values, end, times))
        devices.append(device[-1])
    size = len(stations)
    return {'x': np.asarray(sequences, dtype=np.float32),
            'station': np.asarray(stations, dtype=np.int64),
            'calendar': np.asarray(calendars, dtype=np.float32),
            'target': np.zeros((size, len(HORIZONS)), dtype=np.float32),
            'baseline': np.asarray(baselines, dtype=np.float32),
            'gbt': np.zeros((size, len(HORIZONS)), dtype=np.float32),
            'device': np.asarray(devices, dtype=np.float32)}


def batches(data, batch_size, shuffle, seed):
    indexes = np.arange(len(data['x']))
    if shuffle:
        np.random.default_rng(seed).shuffle(indexes)
    for start in range(0, len(indexes), batch_size):
        yield indexes[start:start + batch_size]


def tensors(data, indexes, x_mean, x_std, y_mean, y_std,
            baseline_mean, baseline_std, residual):
    target = data['target'][indexes]
    if residual:
        target = target - data['baseline'][indexes]
    return (
        torch.from_numpy((data['x'][indexes] - x_mean) / x_std),
        torch.from_numpy(data['station'][indexes]),
        torch.from_numpy(data['calendar'][indexes]),
        torch.from_numpy((data['baseline'][indexes] - baseline_mean) / baseline_std),
        torch.from_numpy((target - y_mean) / y_std),
    )


def predict(model, data, x_mean, x_std, y_mean, y_std,
            baseline_mean, baseline_std, residual, batch_size):
    model.eval()
    output = []
    with torch.no_grad():
        for indexes in batches(data, batch_size, False, 0):
            x, station, calendar, baseline, _ = tensors(
                data, indexes, x_mean, x_std, y_mean, y_std,
                baseline_mean, baseline_std, residual)
            value = model(x, station, calendar, baseline if residual else None).numpy()
            value = value * y_std + y_mean
            if residual:
                value += data['baseline'][indexes]
            output.append(value)
    return np.maximum(0., np.concatenate(output))


def metric(actual_per_device, predicted_per_device, devices):
    actual = actual_per_device * devices
    predicted = predicted_per_device * devices
    error = predicted - actual
    denominator = np.abs(actual).sum()
    variance = ((actual - actual.mean()) ** 2).sum()
    return {
        'n': int(len(actual)),
        'mae': float(np.abs(error).mean()),
        'rmse': float(np.sqrt(np.mean(error ** 2))),
        'wmape': float(np.abs(error).sum() / denominator) if denominator else None,
        'smape': float(np.mean(2 * np.abs(error) /
                              np.maximum(np.abs(actual) + np.abs(predicted), 1e-9))),
        'r2': float(1 - (error ** 2).sum() / variance) if variance else None,
    }


def per_station_metrics(data, prediction, station_ids, column):
    output = {}
    for index, station_id in enumerate(station_ids):
        mask = data['station'] == index
        output[str(station_id)] = metric(data['target'][mask, column],
                                         prediction[mask, column], data['device'][mask])
    return output


def load_gbt_report():
    content = subprocess.check_output(
        ['hdfs', 'dfs', '-cat', '/charging/models/load/direct/production.json'], text=True)
    return json.loads(content)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--input', required=True)
    parser.add_argument('--run-id', default='dl_' + dt.datetime.now().strftime('%Y%m%d_%H%M%S'))
    parser.add_argument('--epochs', type=int, default=12)
    parser.add_argument('--batch-size', type=int, default=1024)
    parser.add_argument('--train-stride', type=int, default=6)
    parser.add_argument('--residual', action='store_true',
                        help='Predict corrections to the 8-week baseline instead of absolute load.')
    args = parser.parse_args()

    seed = 20260914
    random.seed(seed); np.random.seed(seed); torch.manual_seed(seed)
    torch.set_num_threads(min(8, os.cpu_count() or 4))
    raw_file = Path(args.input).resolve()
    raw = np.load(raw_file)
    station_ids = sorted(int(v) for v in np.unique(raw['station_id']))
    station_lookup = {station_id: index for index, station_id in enumerate(station_ids)}
    train = build_samples(raw, 'train', station_lookup, args.train_stride)
    validation = build_samples(raw, 'validation', station_lookup, 1)
    test = build_samples(raw, 'test', station_lookup, 1)
    origins = build_origins(raw, station_lookup)
    x_mean, x_std = float(train['x'].mean()), float(train['x'].std() + 1e-6)
    training_target = train['target'] - train['baseline'] if args.residual else train['target']
    y_mean = training_target.mean(axis=0).astype(np.float32)
    y_std = (training_target.std(axis=0) + 1e-6).astype(np.float32)
    baseline_mean = train['baseline'].mean(axis=0).astype(np.float32)
    baseline_std = (train['baseline'].std(axis=0) + 1e-6).astype(np.float32)

    model = TemporalCNN(len(station_ids), residual=args.residual)
    optimizer = torch.optim.AdamW(model.parameters(), lr=2e-3, weight_decay=1e-4)
    loss_fn = nn.SmoothL1Loss()
    history, best_state, best_score, patience = [], None, math.inf, 0
    for epoch in range(1, args.epochs + 1):
        model.train(); losses = []
        for indexes in batches(train, args.batch_size, True, seed + epoch):
            x, station, calendar, baseline, target = tensors(
                train, indexes, x_mean, x_std, y_mean, y_std,
                baseline_mean, baseline_std, args.residual)
            optimizer.zero_grad(set_to_none=True)
            loss = loss_fn(model(x, station, calendar,
                                 baseline if args.residual else None), target)
            loss.backward()
            nn.utils.clip_grad_norm_(model.parameters(), 2.)
            optimizer.step(); losses.append(float(loss.detach()))
        val_prediction = predict(
            model, validation, x_mean, x_std, y_mean, y_std,
            baseline_mean, baseline_std, args.residual, args.batch_size)
        normalized_mae = float(np.mean(np.abs(val_prediction - validation['target']) / y_std))
        history.append({'epoch': epoch, 'train_loss': float(np.mean(losses)),
                        'validation_normalized_mae': normalized_mae})
        print(json.dumps(history[-1]), flush=True)
        if normalized_mae < best_score - 1e-4:
            best_score, best_state, patience = normalized_mae, copy.deepcopy(model.state_dict()), 0
        else:
            patience += 1
            if patience >= 3:
                break
    model.load_state_dict(best_state)

    predictions = {
        name: predict(model, data, x_mean, x_std, y_mean, y_std,
                      baseline_mean, baseline_std, args.residual, args.batch_size)
        for name, data in [('validation', validation), ('test', test)]
    }
    origin_prediction = predict(model, origins, x_mean, x_std, y_mean, y_std,
                                baseline_mean, baseline_std, args.residual, args.batch_size)
    existing = load_gbt_report()
    evaluation = {}
    for split_name, data in [('validation', validation), ('test', test)]:
        evaluation[split_name] = {}
        for column, horizon in enumerate(HORIZONS):
            cnn = metric(data['target'][:, column], predictions[split_name][:, column], data['device'])
            baseline = metric(data['target'][:, column], data['baseline'][:, column], data['device'])
            gbt = metric(data['target'][:, column], data['gbt'][:, column], data['device'])
            candidate_name = 'residual_cnn' if args.residual else 'temporal_cnn'
            choices = {candidate_name: cnn['mae'], 'shared_gbt': gbt['mae'], '8week': baseline['mae']}
            evaluation[split_name][str(horizon)] = {
                candidate_name: cnn, 'shared_gbt': gbt, '8week': baseline,
                'best': min(choices, key=choices.get),
            }


    station_evaluation = {}
    dashboard = {str(row['station_id']): row for row in
                 json.loads((REPO / 'code/web/data/load-forecast.json').read_text())['data']}
    for column, horizon in enumerate(HORIZONS):
        candidate_name = 'residual_cnn' if args.residual else 'temporal_cnn'
        per_model = {}
        for split_name, data in [('validation', validation), ('test', test)]:
            per_model[split_name] = {
                candidate_name: per_station_metrics(data, predictions[split_name], station_ids, column),
                'shared_gbt': per_station_metrics(data, data['gbt'], station_ids, column),
                '8week': per_station_metrics(data, data['baseline'], station_ids, column),
            }
        stations = {}
        selected_test_prediction = np.zeros(len(test['target']), dtype=np.float32)
        for index, station_id in enumerate(station_ids):
            sid = str(station_id)
            allowed = ['shared_gbt', '8week'] if horizon == 1 else [candidate_name, 'shared_gbt', '8week']
            winner = min(allowed, key=lambda name: per_model['validation'][name][sid]['mae'])
            row = dashboard[sid]['direct_horizons'][str(horizon)]
            origin_values = {
                candidate_name: float(origin_prediction[index, column] * origins['device'][index]),
                'shared_gbt': float(row['shared_gbt_prediction_kwh']),
                '8week': float(row['baseline_prediction_kwh']),
            }
            mask = test['station'] == index
            candidate_arrays = {candidate_name: predictions['test'][:, column],
                                'shared_gbt': test['gbt'][:, column],
                                '8week': test['baseline'][:, column]}
            selected_test_prediction[mask] = candidate_arrays[winner][mask]
            stations[sid] = {
                'winner': winner, 'prediction_kwh': origin_values[winner],
                'candidate_predictions_kwh': origin_values,
                'validation': {name: per_model['validation'][name][sid] for name in allowed},
                'test': {name: per_model['test'][name][sid] for name in allowed},
            }
        station_evaluation[str(horizon)] = {
            'stations': stations,
            'winner_counts': {name: sum(row['winner'] == name for row in stations.values())
                              for name in (candidate_name, 'shared_gbt', '8week')},
            'selected_test': metric(test['target'][:, column], selected_test_prediction, test['device']),
        }

    report = {
        'run_id': args.run_id,
        'created_at': dt.datetime.now(dt.timezone(dt.timedelta(hours=8))).isoformat(),
        'status': 'EXPERIMENT_ONLY',
        'algorithm': ('8周同期基线 + 共享残差 Temporal CNN (Conv1D + 站点嵌入)'
                      if args.residual else
                      '共享时序卷积网络 Temporal CNN (Conv1D + 站点嵌入)'),
        'target_mode': 'baseline_residual' if args.residual else 'absolute_load',
        'feature_source': str(raw_file),
        'lookback_hours': LOOKBACK,
        'horizons': list(HORIZONS),
        'training': {'seed': seed, 'train_stride': args.train_stride,
                     'batch_size': args.batch_size, 'epochs_completed': len(history),
                     'parameter_count': sum(p.numel() for p in model.parameters()),
                     'sample_counts': {k: len(v['x']) for k, v in
                                       [('train', train), ('validation', validation), ('test', test)]},
                     'history': history},
        'evaluation': evaluation,
        'station_selection': station_evaluation,
        'selection_rule': 'Only validation may select a model; test is report-only.',
    }
    report['production_recommendation'] = {
        str(h): ('candidate' if evaluation['validation'][str(h)]['best'] ==
                 ('residual_cnn' if args.residual else 'temporal_cnn')
                 else 'keep_existing') for h in HORIZONS
    }
    report['feature_version'] = existing['feature_version']
    report['origin'] = next(iter(dashboard.values()))['origin']
    output_dir = REPO / '.bigdata' / 'experiments'
    output_dir.mkdir(parents=True, exist_ok=True)
    torch.save({'state_dict': model.state_dict(), 'station_ids': station_ids,
                'x_mean': x_mean, 'x_std': x_std, 'y_mean': y_mean, 'y_std': y_std,
                'baseline_mean': baseline_mean, 'baseline_std': baseline_std,
                'residual': args.residual, 'run_id': args.run_id},
               output_dir / f'{args.run_id}.pt')
    artifact_dir = REPO / 'bigdata' / 'reports'
    artifact_dir.mkdir(parents=True, exist_ok=True)
    report_file = artifact_dir / ('deep_learning_residual_experiment.json'
                                  if args.residual else 'deep_learning_experiment.json')
    report_file.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf-8')
    print(json.dumps({'report': str(report_file),
                      'recommendation': report['production_recommendation']}, ensure_ascii=False))


if __name__ == '__main__':
    main()
