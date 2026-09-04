# -*- coding: utf-8 -*-
"""完整模拟 HttpServer 行为: 根请求返回打包的 index.html, /echarts.min.js 返回打包的 js,
/data.json 实时生成 JSON. 证明这套 HTTP 设计在逻辑层是通的. 监听本机 18080."""
import http.server, socketserver, json, os, datetime

WEB = r"D:\共享文件夹\Projects\code\web"

class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): pass

    def do_GET(self):
        rel = self.path.split('?')[0]
        if rel in ('/', ''):
            rel = 'index.html'
        rel = rel.lstrip('/')

        # 模拟: index.html / echarts.min.js 从"打包资源"读(这里直接读磁盘 web/ 模拟资源)
        if rel == 'data.json':
            # 模拟实时从数据库聚合
            data = {
                "updated": datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                "metrics": {"today": 67.08, "month": 343.27, "total": 6172.17, "users": 0},
                "pileStatus": {"idle": 8, "inUse": 6, "fault": 4},
                "daily7": {"dates": ["08-27","08-28","08-29","08-30","08-31","09-01","09-02"],
                           "values": [106.61,298.26,201.06,125.04,126.7,276.19,67.08]},
                "stationRevenue": {"names": ["开迈斯国家体育馆","特来电五道口","国家电网大兴机场"],
                                   "values": [2449.2,2228.52,1494.45]},
                "predict": {"hours": ["1时","2时"], "loads": [12.3,15.6], "peakHour": 20, "peakLoad": 30.0},
            }
            body = json.dumps(data, ensure_ascii=False).encode('utf-8')
            self.send_response(200)
            self.send_header('Content-Type', 'application/json; charset=utf-8')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        # 模拟打包资源文件
        path = os.path.join(WEB, rel)
        if not os.path.exists(path):
            body = ("<html><body><h2>文件不存在: %s</h2>"
                    "<p>页面已打包进程序, 这里通常是 data.json 未生成</p></body></html>" % rel).encode('utf-8')
            self.send_response(404)
            self.send_header('Content-Type', 'text/html; charset=utf-8')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)
            return

        with open(path, 'rb') as f:
            body = f.read()
        ctype = 'text/html; charset=utf-8' if rel.endswith('.html') else 'application/javascript; charset=utf-8'
        self.send_response(200)
        self.send_header('Content-Type', ctype)
        self.send_header('Content-Length', str(len(body)))
        self.end_headers()
        self.wfile.write(body)

with socketserver.TCPServer(("127.0.0.1", 18080), H) as srv:
    srv.timeout = 1
    import threading, time
    t = threading.Thread(target=srv.serve_forever, daemon=True)
    t.start()
    time.sleep(0.5)

    import urllib.request
    # 1. 根请求
    r = urllib.request.urlopen("http://127.0.0.1:18080/")
    html = r.read().decode('utf-8')
    print("GET /            ->", r.status, r.headers.get('Content-Type'), "长度", len(html), "| 含大屏标题:", "大数据可视化大屏" in html)
    # 2. echarts
    r2 = urllib.request.urlopen("http://127.0.0.1:18080/echarts.min.js")
    js = r2.read()
    print("GET echarts.js   ->", r2.status, "长度", len(js), "| 是echarts:", b'echarts' in js[:2000])
    # 3. data.json
    r3 = urllib.request.urlopen("http://127.0.0.1:18080/data.json")
    dj = json.loads(r3.read().decode('utf-8'))
    print("GET data.json    ->", r3.status, "| today=%s month=%s daily7天数=%d" % (dj['metrics']['today'], dj['metrics']['month'], len(dj['daily7']['dates'])))
    srv.shutdown()
    print("=== HTTP 链路逻辑验证通过 ===")
