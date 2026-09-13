CREATE OR REPLACE TEMP VIEW operation_overview AS
SELECT sum(load_kwh) energy_kwh,sum(revenue) revenue,sum(orders) orders,
 avg(utilization) utilization,avg(fault_rate) fault_rate,
 (SELECT count(DISTINCT user_id) FROM sessions) total_users,
 (SELECT count(DISTINCT user_id) FROM sessions WHERE dt=(SELECT dt FROM data_day)) active_users,
 (SELECT sum(device_count) FROM stations) device_count
FROM hourly WHERE to_date(event_hour)=(SELECT dt FROM data_day);
CREATE OR REPLACE TEMP VIEW station_health_rank AS
SELECT h.station_id,first(station_name) station_name,first(district) district,
 first(device_count) device_count,sum(load_kwh) energy_kwh,sum(revenue) revenue,
 sum(orders) orders,avg(utilization) utilization,avg(fault_rate) fault_rate,
 max(queue_count) peak_queue,
 greatest(0,100-avg(fault_rate)*60-least(20,avg(queue_count)*2)-greatest(0,avg(utilization)-0.85)*100) health
FROM hourly h WHERE to_date(event_hour)=(SELECT dt FROM data_day)
GROUP BY h.station_id ORDER BY health DESC;
CREATE OR REPLACE TEMP VIEW district_map AS
SELECT district,sum(energy_kwh) energy_kwh,sum(device_count) capacity,
 sum(peak_queue) peak_queue,avg(utilization) utilization,avg(fault_rate) fault_rate
FROM station_health_rank GROUP BY district;
CREATE OR REPLACE TEMP VIEW operation_trend AS
SELECT to_date(event_hour) dt,sum(load_kwh) energy_kwh,sum(revenue) revenue,sum(orders) orders
FROM hourly WHERE to_date(event_hour)<=(SELECT dt FROM data_day)
GROUP BY to_date(event_hour) ORDER BY dt;
CREATE OR REPLACE TEMP VIEW load_factors AS
SELECT facility_type,holiday,condition,CASE WHEN dayofweek(event_hour) IN (1,7) THEN 'weekend' ELSE 'weekday' END day_type,
 count(*) hours,avg(load_kwh) avg_kwh FROM hourly WHERE missing_snapshots=0
GROUP BY facility_type,holiday,condition,CASE WHEN dayofweek(event_hour) IN (1,7) THEN 'weekend' ELSE 'weekday' END;
CREATE OR REPLACE TEMP VIEW tariff_contribution AS
SELECT CASE WHEN hour(event_hour) BETWEEN 0 AND 6 THEN '谷(0—6时)' WHEN hour(event_hour) BETWEEN 17 AND 21 THEN '峰(17—21时)' ELSE '平' END band,
 sum(load_kwh) energy_kwh,sum(revenue) revenue FROM hourly
GROUP BY CASE WHEN hour(event_hour) BETWEEN 0 AND 6 THEN '谷(0—6时)' WHEN hour(event_hour) BETWEEN 17 AND 21 THEN '峰(17—21时)' ELSE '平' END;
CREATE OR REPLACE TEMP VIEW realtime_alert AS
SELECT station_id,station_name,'数据日排队/故障' reason,peak_queue,fault_rate
FROM station_health_rank WHERE peak_queue>0 OR fault_rate>0 ORDER BY peak_queue DESC;
CREATE OR REPLACE TEMP VIEW user_segments AS
SELECT CASE WHEN visits>=300 THEN '高活跃' WHEN visits>=100 THEN '常规' ELSE '低频' END segment,
 count(*) users,avg(avg_kwh) avg_kwh,avg(revisit_days) revisit_days
FROM (SELECT user_id,count(*) visits,avg(kwh_total) avg_kwh,
 datediff(max(created),min(created))/greatest(count(*)-1,1) revisit_days FROM sessions GROUP BY user_id) p
GROUP BY CASE WHEN visits>=300 THEN '高活跃' WHEN visits>=100 THEN '常规' ELSE '低频' END;
CREATE OR REPLACE TEMP VIEW load_discrepancy AS
SELECT sum(load_kwh) settlement_kwh,sum(telemetry_kwh) telemetry_kwh,
 (sum(telemetry_kwh)-sum(load_kwh))/sum(load_kwh) relative_difference
FROM hourly WHERE missing_snapshots=0;

CREATE OR REPLACE TEMP VIEW device_status AS
SELECT sum(in_use) busy,sum(idle) idle,sum(fault) fault,sum(fault_alarm_raw) raw_alarms
FROM snapshots WHERE record_time=(SELECT max(record_time) FROM snapshots);
