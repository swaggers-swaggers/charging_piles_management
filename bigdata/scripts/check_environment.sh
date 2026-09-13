#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../.." && pwd)"
source "$repo/bigdata/conf/versions.env"
"$repo/.venv-bigdata/bin/python" - "$SPARK_VERSION" <<'PY'
import sys,pyspark,yaml,jsonschema,numpy
assert pyspark.__version__==sys.argv[1],pyspark.__version__
print('Python',sys.version.split()[0]);print('Spark',pyspark.__version__)
PY
java -version
if [[ "${1:-local}" == cluster ]]; then
 "${HADOOP_HOME:-$HOME/hadoop}/bin/hdfs" dfsadmin -report
 "${HADOOP_HOME:-$HOME/hadoop}/bin/yarn" node -list
fi
