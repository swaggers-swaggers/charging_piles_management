# -*- coding: utf-8 -*-
"""生成《充电桩平台服务端代码讲解稿》Word 文档"""
import re
from docx import Document
from docx.shared import Pt, Cm, RGBColor
from docx.enum.text import WD_ALIGN_PARAGRAPH, WD_LINE_SPACING
from docx.enum.table import WD_TABLE_ALIGNMENT, WD_ALIGN_VERTICAL
from docx.oxml.ns import qn
from docx.oxml import OxmlElement

OUT = "/Users/gaoruohan/Downloads/充电桩/服务端代码讲解稿.docx"

doc = Document()

# ---------- 页面设置: A4, 上下左右 2.5cm ----------
sec = doc.sections[0]
sec.page_width, sec.page_height = Cm(21.0), Cm(29.7)
sec.top_margin = sec.bottom_margin = sec.left_margin = sec.right_margin = Cm(2.5)

# ---------- 工具函数 ----------
def set_run_font(run, east="宋体", west="Arial", size=12, bold=False, color=RGBColor(0, 0, 0)):
    run.font.name = west
    run.font.size = Pt(size)
    run.font.bold = bold
    run.font.color.rgb = color
    rPr = run._element.get_or_add_rPr()
    rFonts = rPr.find(qn('w:rFonts'))
    if rFonts is None:
        rFonts = OxmlElement('w:rFonts')
        rPr.append(rFonts)
    rFonts.set(qn('w:eastAsia'), east)
    rFonts.set(qn('w:ascii'), west)
    rFonts.set(qn('w:hAnsi'), west)

CODE_RE = re.compile(r'`([^`]+)`')

def add_para(text, east="宋体", west="Arial", size=12, bold=False, align=WD_ALIGN_PARAGRAPH.JUSTIFY,
             indent_chars=2, line=1.5, before=0, after=0, keep_next=False):
    p = doc.add_paragraph()
    pf = p.paragraph_format
    pf.alignment = align
    if line is not None:
        pf.line_spacing_rule = WD_LINE_SPACING.MULTIPLE
        pf.line_spacing = line
    pf.space_before = Pt(before)
    pf.space_after = Pt(after)
    if indent_chars:
        pf.first_line_indent = Pt(size * indent_chars)
    else:
        pf.first_line_indent = Pt(0)
    pf.keep_with_next = keep_next
    pos = 0
    for m in CODE_RE.finditer(text):
        if m.start() > pos:
            r = p.add_run(text[pos:m.start()])
            set_run_font(r, east=east, west=west, size=size, bold=bold)
        r = p.add_run(m.group(1))
        set_run_font(r, east="宋体", west="Consolas", size=size, bold=bold)
        pos = m.end()
    if pos < len(text):
        r = p.add_run(text[pos:])
        set_run_font(r, east=east, west=west, size=size, bold=bold)
    return p

def add_heading(text, level):
    p = doc.add_paragraph()
    pf = p.paragraph_format
    pf.keep_with_next = True
    pf.first_line_indent = Pt(0)
    pf.line_spacing_rule = WD_LINE_SPACING.MULTIPLE
    pf.line_spacing = 1.3
    if level == 1:
        pf.space_before, pf.space_after = Pt(14), Pt(6)
        size, bold, east = 16, True, "黑体"
    elif level == 2:
        pf.space_before, pf.space_after = Pt(11), Pt(5)
        size, bold, east = 14, True, "黑体"
    else:
        pf.space_before, pf.space_after = Pt(9), Pt(4)
        size, bold, east = 12, True, "黑体"
    pf.alignment = WD_ALIGN_PARAGRAPH.LEFT
    pos = 0
    for m in CODE_RE.finditer(text):
        if m.start() > pos:
            r = p.add_run(text[pos:m.start()])
            set_run_font(r, east=east, west="Arial", size=size, bold=bold)
        r = p.add_run(m.group(1))
        set_run_font(r, east="宋体", west="Consolas", size=size, bold=bold)
        pos = m.end()
    if pos < len(text):
        r = p.add_run(text[pos:])
        set_run_font(r, east=east, west="Arial", size=size, bold=bold)
    return p

# ---------- 标题与副标题 ----------
p = doc.add_paragraph()
p.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_after = Pt(18)
p.paragraph_format.first_line_indent = Pt(0)
r = p.add_run("充电桩平台服务端代码讲解稿")
set_run_font(r, east="黑体", west="Arial", size=18, bold=True)

p = doc.add_paragraph()
p.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER
p.paragraph_format.space_after = Pt(12)
p.paragraph_format.first_line_indent = Pt(0)
r = p.add_run("以代码名为线索——主要功能与代码实现方式")
set_run_font(r, east="楷体", west="Arial", size=12)

# ---------- 引言 ----------
add_para("本讲解稿从代码实现的角度介绍项目服务端。全文以代码中的类名与函数名为线索，先说明每个模块的功能，"
         "再说明实现方式，讲解时可对照源码按名查看。服务端是全系统的大脑：客户端的所有业务请求、数据库的所有读写、"
         "Web 大屏的所有数据，都由它统一承载。")

# ---------- 表 1: 功能与核心代码对照 ----------
caption = doc.add_paragraph()
caption.paragraph_format.alignment = WD_ALIGN_PARAGRAPH.CENTER
caption.paragraph_format.space_before = Pt(8)
caption.paragraph_format.space_after = Pt(4)
caption.paragraph_format.first_line_indent = Pt(0)
r = caption.add_run("表1  服务端功能与核心代码对照")
set_run_font(r, east="宋体", west="Arial", size=10.5, bold=True)

rows_data = [
    ("功能模块", "核心代码", "主要职责"),
    ("启动总控", "main.cpp", "初始化数据库、恢复现场、启动 TCP/HTTP 服务与管理后台"),
    ("数据库层", "DatabaseManager 与 7 个 XxxDao", "建表迁移、种子数据、加密脱敏、统一数据访问"),
    ("通信层", "TcpServer、ClientHandler、protocol.h", "每连接一线程、JSON 协议分帧与分发"),
    ("充电核心", "ChargingEngine、ChargingPowerModel、PileDao::acquire", "事务开充、3 秒心跳推进、统一结算"),
    ("排队预约", "ReservationDao、sweepReservations", "排队顺延、30 秒确认、10 分钟提醒、宽限过期"),
    ("消息推送", "registerClient、pushToUser", "在线注册表与跨线程投递"),
    ("大屏数据", "HttpServer、DataExporter、Predictor、GeoUtil", "自研 HTTP、10 秒聚合导出、负荷预测"),
    ("管理后台", "AdminLoginDialog、AdminMainWindow、6 个页面", "管理员登录、六大运营页面、操作日志"),
]
table = doc.add_table(rows=len(rows_data), cols=3)
table.style = 'Table Grid'
table.alignment = WD_TABLE_ALIGNMENT.CENTER
table.autofit = True

def fill_cell(cell, text, header=False, code=False):
    cell.vertical_alignment = WD_ALIGN_VERTICAL.CENTER
    p = cell.paragraphs[0]
    p.paragraph_format.first_line_indent = Pt(0)
    p.paragraph_format.line_spacing_rule = WD_LINE_SPACING.SINGLE
    p.paragraph_format.space_before = Pt(1)
    p.paragraph_format.space_after = Pt(1)
    if header:
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        r = p.add_run(text)
        set_run_font(r, east="宋体", west="Arial", size=10.5, bold=True)
    else:
        p.alignment = WD_ALIGN_PARAGRAPH.LEFT
        if code:
            r = p.add_run(text)
            set_run_font(r, east="宋体", west="Consolas", size=10.5)
        else:
            r = p.add_run(text)
            set_run_font(r, east="宋体", west="Arial", size=10.5)

for i, row in enumerate(rows_data):
    for j, val in enumerate(row):
        cell = table.rows[i].cells[j]
        if i == 0:
            fill_cell(cell, val, header=True)
        else:
            fill_cell(cell, val, code=(j == 1))

# 表头底纹浅灰
for j in range(3):
    tcPr = table.rows[0].cells[j]._tc.get_or_add_tcPr()
    shd = OxmlElement('w:shd')
    shd.set(qn('w:val'), 'clear')
    shd.set(qn('w:fill'), 'F2F2F2')
    tcPr.append(shd)

# 列宽
widths = (Cm(3.0), Cm(5.6), Cm(7.4))
for j, w in enumerate(widths):
    for row in table.rows:
        row.cells[j].width = w

# ---------- 正文章节 ----------
add_heading("1  启动总控：main.cpp", 1)
add_para("功能：一个入口把整个服务端拉起来，做到开箱即用。")
add_para("实现：`main.cpp` 按顺序执行六个环节：① `DatabaseManager::init()` 初始化数据库（建表、迁移、种数据，"
         "失败直接退出）；② `ChargingEngine::start()` 启动充电引擎并恢复充电现场；③ `TcpServer` 在 9527 端口监听客户端；"
         "④ `DataExporter` 启动大屏数据定时导出；⑤ `HttpServer` 在 8080 端口监听浏览器访问；⑥ 弹出 `AdminLoginDialog` "
         "登录框，通过后才进入 `AdminMainWindow` 主界面。")

add_heading("2  数据库层：DatabaseManager 与 7 个 DAO", 1)
add_heading("2.1  功能定位", 2)
add_para("功能：集中管理全系统数据，服务端是数据库的唯一持有者，客户端不直连数据库。")
add_para("实现：核心类是单例 `DatabaseManager`；表操作全部拆到 7 个 DAO——`UserDao`、`StationDao`、`PileDao`、"
         "`OrderDao`、`PriceRuleDao`、`ReservationDao`、`LogDao`，统一使用预编译参数绑定，防止 SQL 注入。")
add_heading("2.2  数据库定位与初始化", 2)
add_para("功能：无论从哪个目录启动，都能找到同一份数据。")
add_para("实现：`resolveDatabaseFile()` 按“环境变量 `CHARGING_DB` → 工作目录 `test.db` → 可执行文件目录向上查找 → "
         "都没有则新建”的顺序定位；`init()` 打开库后立即设置 WAL 日志模式（写不阻塞读）、`busy_timeout` 忙等待 3 秒、"
         "开启外键约束，再依次建表与迁移。")
add_heading("2.3  建表、迁移与去重", 2)
add_para("功能：数据结构自愈，旧库无需人工处理。")
add_para("实现：`createTables()` 创建 8 张业务表（管理员、用户、充电站、充电桩、订单、分时费率、排队预约、充值流水、"
         "操作日志）与查询索引；三套迁移分别处理手机号明文升级为哈希、订单表 v2 扩展列补齐、旧管理员表密码列改造；"
         "`deduplicateUsers()` 在事务里合并历史重复用户，并转移其订单、充值、预约与余额。另建数据库触发器，"
         "从库层面禁止一个用户同时存在两个进行中的订单。")
add_heading("2.4  种子数据", 2)
add_para("功能：空库开箱即有演示数据。")
add_para("实现：`seedDefaultData()` 创建默认管理员 `admin/123456`、12 个北京演示充电站与演示用户；"
         "`seedDefaultFeeRules()` 生成每站 7 段分时费率；`seedDemoOrders()` 生成近 30 天每天 3～8 单的演示订单；"
         "`importBundledStations()` 导入打包的真实特斯拉北京站点数据，靠 `app_data_migration` 表做版本标记防重复导入。")
add_heading("2.5  数据访问层与连接管理", 2)
add_para("功能：多线程环境下安全访问数据库。")
add_para("实现：每个客户端工作线程创建独立的 `QSqlDatabase` 连接（连接名区分），DAO 通过 `connName` 参数选择连接；"
         "主线程管理页面使用默认连接，规避 Qt 数据库连接不能跨线程共用的限制。")

add_heading("3  通信层：TcpServer、ClientHandler 与 protocol.h", 1)
add_heading("3.1  每连接一线程", 2)
add_para("功能：多个客户端并发在线互不影响。")
add_para("实现：`TcpServer::incomingConnection()` 每来一个连接就创建一个 `QThread`，把 `ClientHandler` 通过 "
         "`moveToThread` 挪进工作线程执行，套接字读写、JSON 解析与业务处理全部在该线程内完成。")
add_heading("3.2  协议设计", 2)
add_para("功能：客户端与服务端字段对齐、消息有明确边界。")
add_para("实现：`protocol.h` 即协议文档——UTF-8 JSON、一行一条、以换行符分帧；type 前段为请求与应答并回显配对，"
         "后段为服务端主动推送；`ChargeConfig` 集中定义 3 秒心跳、50 元默认冻结、排队上限等常量，双端共享。")
add_heading("3.3  消息分发与缓冲区保护", 2)
add_para("功能：统一入口处理全部请求并防异常输入。")
add_para("实现：`ClientHandler::onReadyRead()` 收字节流 → 按换行拆出完整消息 → `processLine()` → `handleRequest()` "
         "内 `switch(type)` 分发到 18 个 `processXxx()` 业务函数 → `sendJson()` 回包；缓冲区超过 1MB 直接断开连接，"
         "防止内存耗尽。")

add_heading("4  充电引擎：ChargingEngine（核心）", 1)
add_heading("4.1  引擎总览", 2)
add_para("功能：管理所有充电订单的生命周期，是全系统业务量最大的模块。")
add_para("实现：`ChargingEngine` 是主线程单例，内部一个 3 秒 `QTimer` 驱动 `onTick()`，每次心跳做三件事："
         "`sweepActiveOrders()` 推进充电、`sweepReservations()` 扫描排队预约、`notifyOrderEnded()` 通知订单结束。")
add_heading("4.2  开启充电：startCharging 事务", 2)
add_para("功能：一键开始充电，并发下不出脏数据。")
add_para("实现：`startCharging()` 内 `BEGIN IMMEDIATE` 提前拿写锁，在一个事务里完成：校验用户与桩 → "
         "`PriceRuleDao::currentPrice()` 取分时计价 → `calcFreeze()` 按四种目标（不限、电量、金额、时长）计算冻结额 → "
         "`PileDao::acquire()` 用一条带条件的 UPDATE 原子抢桩（仅空闲桩能抢到，同时抢只有一个成功）→ 冻结扣款"
         "（余额不足的判断揉进 SQL 条件）→ `OrderDao::create()` 建单并记录单价快照 → 履约当日预约；"
         "通过 RAII 守卫保证任一步失败自动回滚。")
add_heading("4.3  进度推进：sweepActiveOrders", 2)
add_para("功能：充电过程中电量、金额实时增长，到目标自动结束。")
add_para("实现：每 3 秒把 `OrderDao::listActive()` 查出的在充订单全部扫描：调用 `ChargingPowerModel::averageKw()` "
         "（模拟启动爬坡、波动、电量超过 80% 后降功率的真实曲线）计算功率 → 累加电量与金额写回数据库 → 构造 "
         "type=101 的 `PushOrderProgress` 推送进度；逐单检查电量、金额、时长目标达成与余额耗尽四种结束条件，"
         "命中即进入结算。")
add_heading("4.4  统一结算：settleOrder", 2)
add_para("功能：停止充电时一次完成解冻、扣款、落单、放桩。")
add_para("实现：`settleOrder()` 在一个事务内：解冻冻结额并实扣消费（余额加上冻结、减去消费）→ 更新订单状态与结束原因"
         "（故障单标记为异常中断）→ 释放充电桩（故障桩保持故障不恢复）→ `assignQueueHead()` 把桩分给队首排队者；"
         "管理端强制结束走 `forceFinish()`，退款走 `refundOrder()` 并校验退款不超消费额。")

add_heading("5  排队与预约：ReservationDao 与 sweepReservations()", 1)
add_para("功能：桩被占用时可现场排队，也可预约未来时段，系统自动提醒与过期。")
add_para("实现：数据操作集中在 `ReservationDao`——`enqueue()` 入队（查重、10 人上限）、`queuePosition()` 查排位、"
         "`nextPending()` 取队首、`markAssigned()` 置已分配并写 30 秒到期时间、`appointCreate()` 建预约"
         "（校验时段未过、不可重复、区间重叠判断）、`bookedSlots()` 查某日占用时段；定时逻辑在 "
         "`ChargingEngine::sweepReservations()`，每 3 秒批量捞出“超时未确认、进入 10 分钟提醒窗口、超出宽限期”"
         "三类记录分别处理。")

add_heading("6  消息推送：registerClient / unregisterClient / pushToUser", 1)
add_para("功能：充电进度、轮到您了、预约提醒、充值到账等消息主动推给客户端。")
add_para("实现：`ChargingEngine` 维护用户 ID 到 `ClientHandler` 的在线注册表，用 `QMutex` 保护；`registerClient()` "
         "登录时登记、`unregisterClient()` 断开时注销；`pushToUser()` 按用户查连接，用 `Qt::QueuedConnection` "
         "跨线程投递到 `ClientHandler::pushToClient()` 真正发出；用户不在线时消息丢弃，但进度已落库，重连后仍可查。")

add_heading("7  大屏数据服务：HttpServer、DataExporter、Predictor 与 GeoUtil", 1)
add_para("功能：浏览器访问 8080 端口查看营收、桩状态、各站营收与未来 24 小时负荷预测四张经营图表。")
add_para("实现：`HttpServer` 继承 `QTcpServer`，每个连接由 `HttpConnection` 处理——累积到 `\\r\\n\\r\\n` 解析请求头、"
         "只放行 GET、路径剥离 query 并禁止 `..` 防目录穿越、按扩展名返回 MIME；页面文件打包进 qrc 资源；`/data.json` "
         "请求实时调用 `DataExporter::buildJson()` 从数据库现算返回。`DataExporter` 内 10 秒 `QTimer` 驱动 "
         "`exportNow()`：聚合营收、桩状态、各站营收与 24 小时预测 → 先写 `.tmp` 再 `rename` 原子替换。`Predictor` "
         "做负荷预测：近 14 天订单按小时聚合 → 星期系数与峰谷时段系数 → 环形平滑；`predictIdleRate()` 把全网负荷"
         "按各站功率占比分摊，并与实时空闲率按 0.6/0.4 加权融合。`GeoUtil::haversineKm()` 负责“附近找站”的"
         "球面距离计算与排序。")

add_heading("8  管理后台：AdminLoginDialog、AdminMainWindow 与 6 个页面", 1)
add_para("功能：管理员登录后通过六个页面完成运营管理。")
add_para("实现：`AdminLoginDialog` 调 `DatabaseManager::verifyAdmin()` 加盐哈希验证，通过后 `ServerSession` 记录会话；"
         "`AdminMainWindow` 提供左侧导航、打开大屏入口与连接地址面板（`showConnectionInfo()` 枚举网卡地址，10 秒刷新）；"
         "六个页面为 `SalesPage`（营收指标卡与近 7 日/30 日柱状图，走 `OrderDao::salesSummary()`）、`PileStatusPage`"
         "（桩状态分布环图）、`PileManagePage`（远程重启故障桩）、`OrderManagePage`（订单与预约双 Tab，调 "
         "`ChargingEngine::forceFinish()` 与 `refundOrder()`）、`StationManagePage`（新增站点事务）、`UserManagePage`"
         "（脱敏号搜索与冻结解冻）；所有页面都是“调 DAO → 填表格 + 10 秒 `QTimer` 自动刷新”，敏感操作统一由 "
         "`LogDao::record()` 写操作日志。")

add_heading("9  总结：三个最值得说的实现点", 1)
add_heading("9.1  数据一致性", 2)
add_para("`startCharging()`、`settleOrder()`、`StationDao::add()` 等关键流程全部使用 `BEGIN IMMEDIATE` 事务，"
         "配合原子 UPDATE 抢桩与 RAII 自动回滚，并发下不会出现脏数据。")
add_heading("9.2  进程可恢复", 2)
add_para("充电进度全部存放在 `charge_order` 表，`ChargingEngine::recoverOnStart()` 启动即恢复在充订单并释放孤儿桩，"
         "服务端重启、客户端断线都不丢进度。")
add_heading("9.3  安全与隐私", 2)
add_para("手机号哈希存储与脱敏展示、密码加盐哈希、触发器防重复订单、连接缓冲区上限、HTTP 目录穿越防护，多层设防。")

# ---------- 页脚页码 ----------
footer = sec.footer
fp = footer.paragraphs[0]
fp.alignment = WD_ALIGN_PARAGRAPH.CENTER
run = fp.add_run()
fld1 = OxmlElement('w:fldChar'); fld1.set(qn('w:fldCharType'), 'begin')
instr = OxmlElement('w:instrText'); instr.set(qn('xml:space'), 'preserve'); instr.text = ' PAGE '
fld2 = OxmlElement('w:fldChar'); fld2.set(qn('w:fldCharType'), 'end')
run._r.append(fld1); run._r.append(instr); run._r.append(fld2)
set_run_font(run, east="宋体", west="Arial", size=10.5)

doc.save(OUT)
print("saved:", OUT)
