CREATE OR REPLACE TEMP VIEW hourly_energy AS
SELECT station_id,date_trunc('hour',record_time) event_hour,
 sum(load_kwh) load_kwh,sum(revenue) revenue,sum(telemetry_kwh) telemetry_kwh,
 avg(in_use) in_use,avg(idle) idle,avg(fault) fault,avg(fault_alarm_raw) fault_alarm_raw,
 avg(queue_count) queue_count,avg(charging_power_kw) charging_power_kw,
 sum(CASE WHEN snapshot_missing THEN 1 ELSE 0 END) missing_snapshots
FROM quarter_load GROUP BY station_id,date_trunc('hour',record_time);
CREATE OR REPLACE TEMP VIEW hourly_orders AS
SELECT station_id,date_trunc('hour',created) event_hour,count(*) orders,count(DISTINCT user_id) users
FROM sessions GROUP BY station_id,date_trunc('hour',created);
CREATE OR REPLACE TEMP VIEW station_hourly AS
SELECT e.*,coalesce(o.orders,0) orders,coalesce(o.users,0) users,
 s.station_name,s.district,s.facility_type,s.device_count,
 e.in_use/s.device_count utilization,e.fault/s.device_count fault_rate,
 w.temp_high,w.temp_low,w.precipitation,w.holiday,w.condition
FROM hourly_energy e LEFT JOIN hourly_orders o USING(station_id,event_hour)
JOIN stations s ON e.station_id=s.station_id
LEFT JOIN weather w ON to_date(e.event_hour)=w.dt;
