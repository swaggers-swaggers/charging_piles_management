#include "ClientHandler.h"

#include "DatabaseManager.h"
#include "Predictor.h"
#include "dao/OrderDao.h"
#include "dao/PileDao.h"
#include "dao/StationDao.h"
#include "dao/UserDao.h"
#include "protocol.h"
#include "types.h"
#include "GeoUtil.h"

#include <algorithm>
#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QThread>

// 充电模拟参数: 定时器每 3 秒真实时间 = 1 分钟模拟充电
static const int kTickMs = 3000;
static const int kMinutesPerTick = 1;

ClientHandler::ClientHandler(qintptr socketDescriptor, QObject *parent)
    : QObject(parent)
    , m_descriptor(socketDescriptor)
{
}

ClientHandler::~ClientHandler()
{
    if (!m_dbConnName.isEmpty()) {
        QSqlDatabase db = QSqlDatabase::database(m_dbConnName);
        if (db.isOpen())
            db.close();
        db = QSqlDatabase();   // 释放引用后再移除, 避免Qt告警
        QSqlDatabase::removeDatabase(m_dbConnName);
    }
}

void ClientHandler::start()
{
    m_socket = new QTcpSocket(this);
    if (!m_socket->setSocketDescriptor(m_descriptor)) {
        qDebug() << "[ClientHandler] 套接字初始化失败";
        emit finished();
        return;
    }

    // 本线程私有的数据库连接
    m_dbConnName = QString("client-%1")
                       .arg(reinterpret_cast<quintptr>(QThread::currentThreadId()), 0, 16);
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", m_dbConnName);
    db.setDatabaseName(DatabaseManager::instance().databasePath());
    if (db.open()) {
        QSqlQuery pragma(db);
        pragma.exec("PRAGMA busy_timeout=3000");
    } else {
        qDebug() << "[ClientHandler] 数据库连接失败:" << db.lastError().text();
        // 连接保留, 具体业务请求会以数据库错误应答
    }

    connect(m_socket, &QTcpSocket::readyRead,
            this, &ClientHandler::onReadyRead);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &ClientHandler::onDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        // RemoteHostClosed 会紧随 disconnected, 其余错误记日志便于排查
        if (m_socket->error() != QAbstractSocket::RemoteHostClosedError)
            qDebug() << "[ClientHandler] 套接字错误:" << m_socket->errorString();
    });

    qDebug() << "[ClientHandler] 客户端接入:"
             << m_socket->peerAddress().toString() << m_socket->peerPort();
}

void ClientHandler::onReadyRead()
{
    m_buffer.append(m_socket->readAll());

    // 防止异常数据撑爆内存
    if (m_buffer.size() > 1024 * 1024) {
        qDebug() << "[ClientHandler] 缓冲区超限, 断开连接";
        m_socket->disconnectFromHost();
        return;
    }

    int idx;
    while ((idx = m_buffer.indexOf('\n')) >= 0) {
        const QByteArray line = m_buffer.left(idx);
        m_buffer.remove(0, idx + 1);
        processLine(line);
    }
}

void ClientHandler::onDisconnected()
{
    qDebug() << "[ClientHandler] 客户端断开 userId=" << m_userId;
    stopProgressTimer();
    emit finished();
}

void ClientHandler::processLine(const QByteArray &line)
{
    if (line.trimmed().isEmpty())
        return;

    QJsonParseError parseErr;
    const QJsonDocument doc = QJsonDocument::fromJson(line, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) {
        sendError(0, "请求格式错误");
        return;
    }
    handleRequest(doc.object());
}

void ClientHandler::handleRequest(const QJsonObject &request)
{
    const int type = request.value("type").toInt();
    QJsonObject reply;

    switch (type) {
    case Protocol::ReqHeartbeat:
        reply = Protocol::makeReply(type, true);
        break;
    case Protocol::ReqUserLogin:       reply = processUserLogin(request);      break;
    case Protocol::ReqGetUserInfo:     reply = processGetUserInfo(request);    break;
    case Protocol::ReqUpdateProfile:   reply = processUpdateProfile(request);  break;
    case Protocol::ReqRecharge:        reply = processRecharge(request);       break;
    case Protocol::ReqStationList:     reply = processStationList(request);    break;
    case Protocol::ReqStationPiles:    reply = processStationPiles(request);   break;
    case Protocol::ReqUnfinishedOrder: reply = processUnfinishedOrder(request);break;
    case Protocol::ReqStartCharge:     reply = processStartCharge(request);    break;
    case Protocol::ReqStopCharge:      reply = processStopCharge(request);     break;
    default:
        sendError(type, "暂不支持的消息类型");
        return;
    }

    sendJson(reply);
}

// ---------- 登录 ----------

QJsonObject ClientHandler::processUserLogin(const QJsonObject &req)
{
    const QString phone = req.value("phone").toString().trimmed();

    static const QRegularExpression phoneReg("^1\\d{10}$");
    if (!phoneReg.match(phone).hasMatch())
        return Protocol::makeReply(Protocol::ReqUserLogin, false, "手机号格式不正确!");

    UserInfo info;
    bool isNewUser = false;
    QString errMsg;
    if (!DatabaseManager::instance().loginOrRegisterUser(phone, &info, &isNewUser,
                                                         &errMsg, m_dbConnName))
        return Protocol::makeReply(Protocol::ReqUserLogin, false, errMsg);

    m_userId = info.id;

    QJsonObject reply = Protocol::makeReply(Protocol::ReqUserLogin, true);
    reply.insert("userId", info.id);
    reply.insert("phone", info.phone);
    reply.insert("nickname", info.nickname);
    reply.insert("balance", info.balance);
    reply.insert("isNew", isNewUser);
    qDebug() << "[ClientHandler] 用户登录:" << phone
             << (isNewUser ? "(新注册)" : "") << "nickname=" << info.nickname;
    return reply;
}

// ---------- 用户信息 ----------

QJsonObject ClientHandler::processGetUserInfo(const QJsonObject &req)
{
    const int userId = req.value("userId").toInt(m_userId);
    UserInfo u;
    QString errMsg;
    if (!UserDao::getById(userId, &u, &errMsg, m_dbConnName))
        return Protocol::makeReply(Protocol::ReqGetUserInfo, false, errMsg);

    QJsonObject reply = Protocol::makeReply(Protocol::ReqGetUserInfo, true);
    reply.insert("nickname", u.nickname);
    reply.insert("balance", u.balance);
    reply.insert("avatar", u.avatar);   // base64, 空串表示默认头像
    reply.insert("status", u.status);
    return reply;
}

QJsonObject ClientHandler::processUpdateProfile(const QJsonObject &req)
{
    const int userId = req.value("userId").toInt(m_userId);
    QString errMsg;
    if (!UserDao::updateProfile(userId,
                                req.value("nickname").toString(),
                                req.value("avatar").toString(),
                                &errMsg, m_dbConnName))
        return Protocol::makeReply(Protocol::ReqUpdateProfile, false, errMsg);

    return processGetUserInfo(req);
}

QJsonObject ClientHandler::processRecharge(const QJsonObject &req)
{
    const int userId = req.value("userId").toInt(m_userId);
    const double amount = req.value("amount").toDouble();

    if (amount <= 0 || amount > 10000)
        return Protocol::makeReply(Protocol::ReqRecharge, false, "充值金额需在 0.01 ~ 10000 元之间!");

    double newBalance = 0;
    QString errMsg;
    if (!UserDao::recharge(userId, amount, &newBalance, &errMsg, m_dbConnName))
        return Protocol::makeReply(Protocol::ReqRecharge, false, errMsg);

    qDebug() << "[ClientHandler] 用户" << userId << "充值" << amount << "元, 余额" << newBalance;
    QJsonObject reply = Protocol::makeReply(Protocol::ReqRecharge, true);
    reply.insert("balance", newBalance);
    return reply;
}

// ---------- 充电站查询 ----------

QJsonObject ClientHandler::processStationList(const QJsonObject &req)
{
    const double lon = req.value("lon").toDouble(123.45);
    const double lat = req.value("lat").toDouble(41.70);

    QList<StationInfo> stations = StationDao::list(m_dbConnName);
    for (StationInfo &s : stations) {
        s.distance = GeoUtil::haversineKm(lat, lon, s.latitude, s.longitude);
        // 负荷预测: 预计 1 小时后的空闲率(阶段5 加分项)
        s.predictIdle = Predictor::predictIdleRate(s.id, s.totalPiles, s.idlePiles, 1,
                                                    m_dbConnName);
    }
    // 按距离由近及远
    std::sort(stations.begin(), stations.end(),
              [](const StationInfo &a, const StationInfo &b) { return a.distance < b.distance; });

    QJsonArray arr;
    for (const StationInfo &s : stations)
        arr.append(s.toJson());

    QJsonObject reply = Protocol::makeReply(Protocol::ReqStationList, true);
    reply.insert("stations", arr);
    return reply;
}

QJsonObject ClientHandler::processStationPiles(const QJsonObject &req)
{
    const int stationId = req.value("stationId").toInt();
    if (stationId <= 0)
        return Protocol::makeReply(Protocol::ReqStationPiles, false, "参数错误: 缺少stationId");

    const QList<PileInfo> piles = PileDao::listByStation(stationId, m_dbConnName);
    QJsonArray arr;
    for (const PileInfo &p : piles)
        arr.append(p.toJson());

    QJsonObject reply = Protocol::makeReply(Protocol::ReqStationPiles, true);
    reply.insert("piles", arr);
    return reply;
}

// ---------- 充电与订单 ----------

QJsonObject ClientHandler::processUnfinishedOrder(const QJsonObject &req)
{
    const int userId = req.value("userId").toInt(m_userId);
    OrderInfo order;
    bool has = false;
    QString errMsg;
    if (!OrderDao::getUnfinishedByUser(userId, &order, &has, &errMsg, m_dbConnName))
        return Protocol::makeReply(Protocol::ReqUnfinishedOrder, false, errMsg);

    QJsonObject reply = Protocol::makeReply(Protocol::ReqUnfinishedOrder, true);
    reply.insert("hasOrder", has);
    if (has)
        reply.insert("order", order.toJson());
    return reply;
}

QJsonObject ClientHandler::processStartCharge(const QJsonObject &req)
{
    const int userId = req.value("userId").toInt(m_userId);
    const int pileId = req.value("pileId").toInt();
    if (pileId <= 0)
        return Protocol::makeReply(Protocol::ReqStartCharge, false, "参数错误: 缺少pileId");

    // 1. 该用户已有未完成订单 → 拒绝
    OrderInfo exist;
    bool has = false;
    OrderDao::getUnfinishedByUser(userId, &exist, &has, nullptr, m_dbConnName);
    if (has)
        return Protocol::makeReply(Protocol::ReqStartCharge, false, "您有未完成的充电订单, 请先结算!");

    // 2. 桩状态校验
    PileInfo pile;
    QString errMsg;
    if (!PileDao::getById(pileId, &pile, &errMsg, m_dbConnName))
        return Protocol::makeReply(Protocol::ReqStartCharge, false, errMsg);
    if (pile.status != PileIdle)
        return Protocol::makeReply(Protocol::ReqStartCharge, false,
                                   QString("电桩 %1 当前不可用(%2)")
                                       .arg(pile.code, pile.status == PileFault ? "故障" : "在用"));

    // 3. 建订单 + 桩置在用
    const QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    int orderId = -1;
    if (!OrderDao::create(userId, pileId, pile.stationId, now, &orderId, &errMsg, m_dbConnName)) {
        return Protocol::makeReply(Protocol::ReqStartCharge, false, errMsg);
    }
    PileDao::setStatus(pileId, PileInUse, nullptr, m_dbConnName);

    qDebug() << "[ClientHandler] 用户" << userId << "在电桩" << pile.code << "开始充电, 订单" << orderId;

    // 4. 启动本线程的充电进度模拟
    startProgressTimer(orderId);

    OrderInfo order;
    OrderDao::getById(orderId, &order, nullptr, m_dbConnName);
    QJsonObject reply = Protocol::makeReply(Protocol::ReqStartCharge, true);
    reply.insert("order", order.toJson());
    return reply;
}

QJsonObject ClientHandler::processStopCharge(const QJsonObject &req)
{
    const int userId = req.value("userId").toInt(m_userId);
    const int orderId = req.value("orderId").toInt();
    if (orderId <= 0)
        return Protocol::makeReply(Protocol::ReqStopCharge, false, "参数错误: 缺少orderId");

    OrderContext ctx;
    QString errMsg;
    if (!OrderDao::getContext(orderId, &ctx, &errMsg, m_dbConnName))
        return Protocol::makeReply(Protocol::ReqStopCharge, false, errMsg);
    if (ctx.order.userId != userId)
        return Protocol::makeReply(Protocol::ReqStopCharge, false, "无权操作该订单");
    if (ctx.order.status != OrderCharging)
        return Protocol::makeReply(Protocol::ReqStopCharge, false, "订单已完成结算");

    // 1. 余额校验与扣款
    UserInfo user;
    UserDao::getById(userId, &user, nullptr, m_dbConnName);
    if (user.balance < ctx.order.amount)
        return Protocol::makeReply(Protocol::ReqStopCharge, false,
                                   QString("余额不足(需 %1 元, 当前 %2 元), 请先充值!")
                                       .arg(ctx.order.amount, 0, 'f', 2)
                                       .arg(user.balance, 0, 'f', 2));
    UserDao::adjustBalance(userId, -ctx.order.amount, nullptr, m_dbConnName);

    // 2. 计费时长: 优先用模拟分钟数, 兜底用真实经过分钟数
    const int minutes = m_chargingOrderId == orderId
                            ? qMax(m_simMinutes, 1)
                            : qMax<int>(QDateTime::fromString(ctx.order.startTime,
                                                              "yyyy-MM-dd hh:mm:ss")
                                            .secsTo(QDateTime::currentDateTime()) / 60, 1);

    // 3. 完成订单 + 更新桩统计与状态
    const QString now = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
    OrderDao::finish(orderId, now, ctx.order.energy, ctx.order.amount, &errMsg, m_dbConnName);
    PileDao::addUsage(ctx.order.pileId, minutes, nullptr, m_dbConnName);
    PileDao::setStatus(ctx.order.pileId, PileIdle, nullptr, m_dbConnName);

    stopProgressTimer();

    qDebug() << "[ClientHandler] 订单" << orderId << "结算: 电量" << ctx.order.energy
             << "度, 金额" << ctx.order.amount << "元";

    UserInfo after;
    UserDao::getById(userId, &after, nullptr, m_dbConnName);
    OrderInfo order;
    OrderDao::getById(orderId, &order, nullptr, m_dbConnName);
    QJsonObject reply = Protocol::makeReply(Protocol::ReqStopCharge, true);
    reply.insert("order", order.toJson());
    reply.insert("balance", after.balance);
    return reply;
}

// ---------- 充电进度模拟 ----------

void ClientHandler::startProgressTimer(int orderId)
{
    stopProgressTimer();
    m_chargingOrderId = orderId;
    m_simMinutes = 0;

    m_progressTimer = new QTimer(this);
    connect(m_progressTimer, &QTimer::timeout, this, &ClientHandler::onProgressTick);
    m_progressTimer->start(kTickMs);
}

void ClientHandler::stopProgressTimer()
{
    if (m_progressTimer) {
        m_progressTimer->stop();
        m_progressTimer->deleteLater();
        m_progressTimer = nullptr;
    }
    m_chargingOrderId = -1;
}

void ClientHandler::onProgressTick()
{
    if (m_chargingOrderId < 0)
        return;

    OrderContext ctx;
    if (!OrderDao::getContext(m_chargingOrderId, &ctx, nullptr, m_dbConnName)
        || ctx.order.status != OrderCharging) {
        // 订单已不存在/已完成(如服务重启后的旧订单), 停止模拟
        stopProgressTimer();
        return;
    }

    m_simMinutes += kMinutesPerTick;

    // 电量按桩功率累计(kWh), 金额按站电价累计(元)
    const double energy = ctx.order.energy + ctx.power * kMinutesPerTick / 60.0;
    const double amount = energy * ctx.price;
    OrderDao::updateProgress(m_chargingOrderId, energy, amount, m_dbConnName);

    // 推送进度给客户端
    QJsonObject push;
    push.insert("type", Protocol::PushOrderProgress);
    push.insert("orderId", m_chargingOrderId);
    push.insert("energy", qRound(energy * 100) / 100.0);
    push.insert("amount", qRound(amount * 100) / 100.0);
    push.insert("minutes", m_simMinutes);
    sendJson(push);
}

// ---------- 发送 ----------

void ClientHandler::sendJson(const QJsonObject &obj)
{
    if (!m_socket)
        return;
    m_socket->write(QJsonDocument(obj).toJson(QJsonDocument::Compact) + '\n');
}

void ClientHandler::sendError(int type, const QString &text)
{
    sendJson(Protocol::makeReply(type, false, text));
}
