#!/usr/bin/env bash
# Start the existing local installation without SSH or formatting HDFS.
set -euo pipefail
export HADOOP_HOME="${HADOOP_HOME:-$HOME/hadoop}"
export HADOOP_CONF_DIR="${HADOOP_CONF_DIR:-$HADOOP_HOME/etc/hadoop}"
export JAVA_HOME="${JAVA_HOME:-$(dirname "$(dirname "$(readlink -f "$(command -v java)")")")}"
export HADOOP_MAPRED_HOME="$HADOOP_HOME"
start_daemon() {
    if ! "$HADOOP_HOME/bin/$1" --daemon status "$2" >/dev/null 2>&1; then
        "$HADOOP_HOME/bin/$1" --daemon start "$2"
    fi
}
case "${1:-start}" in
    start)
        start_daemon hdfs namenode
        start_daemon hdfs datanode
        start_daemon hdfs secondarynamenode
        start_daemon yarn resourcemanager
        start_daemon yarn nodemanager
        "$HADOOP_HOME/bin/hdfs" dfsadmin -safemode wait
        ;;
    status)
        "$HADOOP_HOME/bin/hdfs" dfsadmin -report
        "$HADOOP_HOME/bin/yarn" node -list
        ;;
    stop)
        "$HADOOP_HOME/bin/yarn" --daemon stop nodemanager
        "$HADOOP_HOME/bin/yarn" --daemon stop resourcemanager
        "$HADOOP_HOME/bin/hdfs" --daemon stop secondarynamenode
        "$HADOOP_HOME/bin/hdfs" --daemon stop datanode
        "$HADOOP_HOME/bin/hdfs" --daemon stop namenode
        ;;
    *) echo "Usage: $0 {start|status|stop}" >&2; exit 2 ;;
esac
