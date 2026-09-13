from modeling import metrics
import math

def test_zero_load_metrics_are_finite_or_explicitly_undefined():
 m=metrics([0,0],[0,1])
 assert m['mae']==.5 and m['wmape'] is None and m['r2'] is None
 assert math.isfinite(m['smape'])

def test_bad_model_is_not_hidden_by_accuracy_label():
 m=metrics([1,2,3],[9,9,9])
 assert m['r2']<0 and m['mae']==7
