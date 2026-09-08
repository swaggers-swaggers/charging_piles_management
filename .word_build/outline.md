# 服务端代码讲解稿 · 内容大纲

**交付物**：Word 文档《充电桩平台服务端代码讲解稿》
**体裁**：通用文档（技术讲解稿，面向课程答辩/演示）
**风格**：书面讲解稿，每节"先功能、后代码实现方式"，用代码名（类名/函数名）指代具体实现，不贴代码
**编号体系**：阿拉伯小数层级（1 → 1.1 → 1.1.1）
**目录**：不设（现场讲解稿性质，正文预计 4~5 页）
**页码**：页脚居中阿拉伯数字

## 结构

- 标题：充电桩平台服务端代码讲解稿
- 副标题：以代码名为线索——主要功能与代码实现方式
- 引言段：讲解定位（面向代码的讲解，用类名/函数名指代实现）
- 表 1：服务端功能与核心代码对照（8 行）
- 1 启动总控：main.cpp
- 2 数据库层：DatabaseManager 与 7 个 DAO
  - 2.1 功能定位
  - 2.2 数据库定位与初始化（resolveDatabaseFile / init / WAL）
  - 2.3 建表、迁移与去重（createTables / 三套 migrate / deduplicateUsers）
  - 2.4 种子数据（seedDefaultData 等 4 个方法）
  - 2.5 数据访问层与连接管理（7 个 DAO、connName）
- 3 通信层：TcpServer、ClientHandler 与 protocol.h
  - 3.1 每连接一线程（incomingConnection / moveToThread）
  - 3.2 协议设计（JSON 单行、\n 分帧、请求应答/推送分段、ChargeConfig）
  - 3.3 消息分发与缓冲区保护（onReadyRead / handleRequest / switch）
- 4 充电引擎：ChargingEngine（核心）
  - 4.1 引擎总览（单例、QTimer、onTick 三扫描）
  - 4.2 开启充电：startCharging 事务（BEGIN IMMEDIATE、校验、currentPrice、calcFreeze、PileDao::acquire、OrderDao::create、RAII 回滚）
  - 4.3 进度推进：sweepActiveOrders（ChargingPowerModel::averageKw、四种结束条件、PushOrderProgress）
  - 4.4 统一结算：settleOrder（解冻实扣、落单、释放桩、assignQueueHead；forceFinish / refundOrder）
- 5 排队与预约：ReservationDao 与 sweepReservations()
- 6 消息推送：registerClient / unregisterClient / pushToUser
- 7 大屏数据服务：HttpServer、DataExporter、Predictor 与 GeoUtil
- 8 管理后台：AdminLoginDialog、AdminMainWindow 与 6 个页面
- 9 总结：三个最值得说的实现点
  - 9.1 数据一致性
  - 9.2 进程可恢复
  - 9.3 安全与隐私

## 事实来源

全部内容来自对项目 server 目录、common 目录代码的阅读（main.cpp、DatabaseManager、TcpServer、ClientHandler、ChargingEngine、ChargingPowerModel、HttpServer、DataExporter、Predictor、GeoUtil、7 个 DAO、AdminLoginDialog、AdminMainWindow、6 个管理页面、protocol.h）。
