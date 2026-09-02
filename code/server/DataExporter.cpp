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
    if (!envDir.isEmpty())
        return envDir;

    const QString appDir = QCoreApplication::applicationDirPath();
    if (QDir(appDir + "/../web").exists())
        return QFileInfo(appDir + "/../web").absoluteFilePath();
    if (QDir("web").exists())
        return QFileInfo("web").absoluteFilePath();
    return QFileInfo(appDir + "/web").absoluteFilePath();
}

void DataExporter::exportNow()
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
    metrics.insert("users", 0);   // 由客户端登录数动态补充的意义不大, 暂以 0 占位
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

    // ---- 写文件(先写临时文件再替换, 避免大屏读到半截 JSON) ----
    const QString finalPath = exportDir() + "/data.json";
    const QString tmpPath = finalPath + ".tmp";
    QFile file(tmpPath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
        file.close();
        QFile::remove(finalPath);
        QFile::rename(tmpPath, finalPath);
    }
}
