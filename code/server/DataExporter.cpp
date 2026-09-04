#include "DataExporter.h"

#include "OrderDao.h"
#include "PileDao.h"
#include "Predictor.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTime>
#include <QTimer>

DataExporter::DataExporter(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(exportDir());

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &DataExporter::exportNow);
    timer->start(10 * 1000);
    exportNow();
}

QString DataExporter::exportDir()
{
    const QString envDir = qEnvironmentVariable("CHARGING_WEB_DIR");
    if (!envDir.isEmpty()) {
        QDir().mkpath(envDir);
        return envDir;
    }

    const QString appDir = QCoreApplication::applicationDirPath();
    // 覆盖常见布局: 直接源码运行 / Qt Creator 构建目录(debug/release 在构建根的下两级)
    const QStringList candidates = {
        QFileInfo(appDir + "/../web").absoluteFilePath(),    // 源码 code/server -> code/web
        QFileInfo(appDir + "/../../web").absoluteFilePath(), // build/debug -> build/../../web
        QFileInfo("web").absoluteFilePath(),
        QFileInfo(appDir + "/web").absoluteFilePath(),
    };
    // 优先找"含 index.html 的 web 目录"(源码大屏页面所在), 保证 HTTP 服务能出页面
    for (const QString &c : candidates) {
        if (QFileInfo(c + "/index.html").exists())
            return c;
    }
    for (const QString &c : candidates) {
        if (QDir(c).exists())
            return c;
    }
    // 都不存在则使用 appDir 下的 web 并自动创建
    const QString fallback = QFileInfo(appDir + "/web").absoluteFilePath();
    QDir().mkpath(fallback);
    qWarning() << "[DataExporter] 未找到含 index.html 的 web 目录, 导出到:" << fallback
               << "(大屏页面将不可用; 可通过环境变量 CHARGING_WEB_DIR 指向源码 web/ 目录)";
    return fallback;
}

QByteArray DataExporter::buildJson() const
{
    // ---- 营收指标 ----
    double today = 0, month = 0, total = 0;
    OrderDao::salesSummary(&today, &month, &total);

    // ---- 近7日趋势 ----
    QJsonArray dailyDates, dailyValues;
    const QVector<QPair<QString, double>> daily = OrderDao::dailyRevenue(7);
    for (const auto &entry : daily) {
        dailyDates.append(entry.first.mid(5));   // MM-dd
        dailyValues.append(qRound(entry.second * 100) / 100.0);
    }

    // ---- 电桩状态分布 ----
    int idle = 0, inUse = 0, fault = 0;
    PileDao::statusCounts(&idle, &inUse, &fault);

    // ---- 各站营收 ----
    QJsonArray stationNames, stationValues;
    const QList<QPair<QString, double>> stationRev = OrderDao::stationRevenue();
    for (const auto &entry : stationRev) {
        stationNames.append(entry.first);
        stationValues.append(qRound(entry.second * 100) / 100.0);
    }

    // ---- 负荷预测(未来24小时) ----
    const QVector<double> forecast = Predictor::forecast24h();
    QJsonArray predictHours, predictLoads;
    const int currentHour = QTime::currentTime().hour();
    double peakLoad = 0;
    int peakHour = 0;
    for (int i = 0; i < forecast.size(); ++i) {
        predictHours.append(QString("%1时").arg((currentHour + 1 + i) % 24));
        predictLoads.append(qRound(forecast[i] * 100) / 100.0);
        if (forecast[i] > peakLoad) {
            peakLoad = forecast[i];
            peakHour = (currentHour + 1 + i) % 24;
        }
    }

    // ---- 汇总 ----
    QJsonObject root;
    root.insert("updated", QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss"));

    QJsonObject metrics;
    metrics.insert("today", qRound(today * 100) / 100.0);
    metrics.insert("month", qRound(month * 100) / 100.0);
    metrics.insert("total", qRound(total * 100) / 100.0);
    int userCount = 0;
    QSqlQuery userQuery(QSqlDatabase::database());
    if (userQuery.exec("SELECT COUNT(*) FROM user") && userQuery.next())
        userCount = userQuery.value(0).toInt();
    metrics.insert("users", userCount);
    root.insert("metrics", metrics);

    QJsonObject piles;
    piles.insert("idle", idle);
    piles.insert("inUse", inUse);
    piles.insert("fault", fault);
    root.insert("pileStatus", piles);

    QJsonObject trend;
    trend.insert("dates", dailyDates);
    trend.insert("values", dailyValues);
    root.insert("daily7", trend);

    QJsonObject stations;
    stations.insert("names", stationNames);
    stations.insert("values", stationValues);
    root.insert("stationRevenue", stations);

    QJsonObject predict;
    predict.insert("hours", predictHours);
    predict.insert("loads", predictLoads);
    predict.insert("peakHour", peakHour);
    predict.insert("peakLoad", qRound(peakLoad * 100) / 100.0);
    root.insert("predict", predict);

    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

void DataExporter::exportNow()
{
    // ---- 写文件(先写临时文件再替换, 避免大屏读到半截 JSON) ----
    const QByteArray json = buildJson();
    const QString finalPath = exportDir() + "/data.json";
    const QString tmpPath = finalPath + ".tmp";
    QFile file(tmpPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(json);
        file.close();
        QFile::remove(finalPath);
        QFile::rename(tmpPath, finalPath);
    }
}
