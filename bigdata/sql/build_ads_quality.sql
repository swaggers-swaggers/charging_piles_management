SELECT table_name,severity,sum(failed_count) rule_hits,max(failed_rate) worst_rule_rate
FROM charging.ads_data_quality_report GROUP BY table_name,severity;
