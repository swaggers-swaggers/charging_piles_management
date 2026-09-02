#!/bin/bash
# 大屏启动脚本: 在 web/ 目录启动本地 HTTP 服务并打开浏览器
# (浏览器禁止 file:// 页面 fetch 本地 JSON, 必须以 http 方式访问)
cd "$(dirname "$0")"
echo "大数据可视化大屏: http://127.0.0.1:8080"
(xdg-open http://127.0.0.1:8080 >/dev/null 2>&1 || open http://127.0.0.1:8080 >/dev/null 2>&1 &)
python3 -m http.server 8080
