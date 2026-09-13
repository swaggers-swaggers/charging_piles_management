-- Run jobs/create_tables.py after ETL to infer exact schema and register all existing layers.
-- Replace ${ROOT} with file:///.../.bigdata/lake or hdfs://namenode:9000/charging.
CREATE DATABASE IF NOT EXISTS charging;
CREATE TABLE IF NOT EXISTS charging.dwd_station USING PARQUET LOCATION '${ROOT}/dwd/station';
CREATE TABLE IF NOT EXISTS charging.dwd_session USING PARQUET LOCATION '${ROOT}/dwd/session';
CREATE TABLE IF NOT EXISTS charging.dwd_snapshot USING PARQUET LOCATION '${ROOT}/dwd/snapshot';
CREATE TABLE IF NOT EXISTS charging.dwd_weather USING PARQUET LOCATION '${ROOT}/dwd/weather';
CREATE TABLE IF NOT EXISTS charging.dwd_bms USING PARQUET LOCATION '${ROOT}/dwd/bms';
CREATE TABLE IF NOT EXISTS charging.dws_station_hourly_load USING PARQUET LOCATION '${ROOT}/dws/station_hourly_load';
CREATE TABLE IF NOT EXISTS charging.ads_load_forecast USING PARQUET LOCATION '${ROOT}/ads/load_forecast';
