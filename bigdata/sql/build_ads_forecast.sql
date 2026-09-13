-- Published predictions are produced by predict_station_load.py after validation selection.
SELECT station_id,model,model_version,count(*) steps,min(load_kwh) min_prediction,max(load_kwh) peak_kwh
FROM charging.ads_load_forecast GROUP BY station_id,model,model_version;
