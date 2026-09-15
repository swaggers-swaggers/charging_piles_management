import json
from pathlib import Path


REPO = Path(__file__).resolve().parents[2]


def test_real_forecast_has_complete_station_horizon_grid():
    payload = json.loads((REPO / "code/web/data/real-load-forecast.json").read_text())
    rows = payload["data"]
    stations = {row["station_id"] for row in rows}
    assert "Beijing public charging transactions" in payload["source"]
    assert len(stations) == 20
    assert len(rows) == len(stations) * 3
    assert {(row["station_id"], row["horizon_hours"]) for row in rows} == {
        (station, horizon) for station in stations for horizon in (1, 6, 24)
    }
    assert all(row["prediction_kwh"] >= 0 for row in rows)
    assert {row["model"] for row in rows} <= {"residual_cnn", "spark_gbt", "8week"}


def test_real_training_reports_reconcile_counts():
    spark = json.loads((REPO / "bigdata/reports/real_beijing_spark_run.json").read_text())
    cnn = json.loads((REPO / "bigdata/reports/real_beijing_cnn_experiment.json").read_text())
    assert spark["raw_records"] == spark["deduplicated_records"] + spark["duplicate_records"]
    assert spark["deduplicated_records"] == spark["clean_records"] + spark["rejected_records"]
    assert spark["spark_master"] == "yarn"
    assert spark["source_station_count"] == 8553
    assert sum(cnn["sample_counts"].values()) > 0
    assert cnn["spark_application_id"] == spark["application_id"]


def test_active_dashboard_is_the_coherent_real_generation():
    data_dir = REPO / "code/web/data"
    current = json.loads((data_dir / "current.json").read_text())
    assert current["runId"].startswith("beijing_")
    payloads = {
        name: json.loads((data_dir / relative).read_text())
        for name, relative in current["files"].items()
    }
    assert len(payloads) == 7
    assert {value["runId"] for value in payloads.values()} == {current["runId"]}
    assert {value["dataNature"] for value in payloads.values()} == {"真实数据+模拟补齐"}
    operation = payloads["overview"]["data"]["operation_overview"][0]
    for field in ("energy_kwh", "revenue", "orders", "utilization", "fault_rate",
                  "total_users", "active_users", "device_count"):
        assert operation[field] is not None
    assert operation["field_sources"]["energy_kwh"] == "北京真实交易"
    assert operation["field_sources"]["fault_rate"].startswith("模拟补齐:")
    assert payloads["user-demand"]["data"]["user_count"] > 0
    assert payloads["user-demand"]["data"]["data_source"] == "北京模拟数据补齐"
