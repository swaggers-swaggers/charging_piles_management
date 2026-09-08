#include "LogManager.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QMutexLocker>
#include <QtGlobal>

#include <cstdio>

namespace {
constexpr qint64 kMaxFileSize = 10 * 1024 * 1024;   // 单文件 10MB 滚动
constexpr int kKeepDays = 7;                        // 保留 7 天
}

QMutex LogManager::s_mutex;
QFile *LogManager::s_file = nullptr;
QString LogManager::s_day;
int LogManager::s_volume = 0;
int LogManager::s_minLevel = 1;   // 默认 INFO

void LogManager::init()
{
    QMutexLocker lock(&s_mutex);
    if (s_file)
        return;

    // 级别过滤: CHARGING_LOG_LEVEL=DEBUG/INFO/WARN/ERROR, 默认 INFO
    const QByteArray lvl = qgetenv("CHARGING_LOG_LEVEL").trimmed().toUpper();
    if (lvl == "DEBUG")
        s_minLevel = 0;
    else if (lvl == "WARN")
        s_minLevel = 2;
    else if (lvl == "ERROR")
        s_minLevel = 3;
    else
        s_minLevel = 1;

    QDir().mkpath(logDir());
    cleanupOld();
    rollIfNeeded();
    qInstallMessageHandler(&LogManager::handle);
}

void LogManager::shutdown()
{
    QMutexLocker lock(&s_mutex);
    if (s_file) {
        s_file->flush();
        delete s_file;
        s_file = nullptr;
    }
    qInstallMessageHandler(nullptr);
}

QString LogManager::logDir()
{
    return QCoreApplication::applicationDirPath() + "/logs";
}

QString LogManager::dayString()
{
    return QDateTime::currentDateTime().toString("yyyyMMdd");
}

void LogManager::rollIfNeeded()
{
    const QString day = dayString();
    if (!s_file) {
        s_day = day;
        s_volume = 0;
    } else if (day != s_day) {
        // 跨天: 关闭旧文件, 开启新的一天
        s_file->flush();
        delete s_file;
        s_file = nullptr;
        s_day = day;
        s_volume = 0;
    } else if (s_file->size() >= kMaxFileSize) {
        // 单文件超限: 滚动续号
        s_file->flush();
        delete s_file;
        s_file = nullptr;
        ++s_volume;
    }

    if (!s_file) {
        QString name = QString("server-%1.log").arg(s_day);
        if (s_volume > 0)
            name = QString("server-%1-%2.log").arg(s_day).arg(s_volume);
        s_file = new QFile(logDir() + "/" + name);
        s_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    }
}

void LogManager::cleanupOld()
{
    const QDir dir(logDir());
    const QStringList names = dir.entryList(QStringList() << "server-*.log",
                                            QDir::Files, QDir::Name);
    const QDate cutoff = QDate::currentDate().addDays(-kKeepDays);
    for (const QString &name : names) {
        // 文件名形如 server-20260908.log / server-20260908-1.log, 第 8 位起是日期
        const QDate d = QDate::fromString(name.mid(7, 8), "yyyyMMdd");
        if (d.isValid() && d < cutoff)
            QFile::remove(dir.filePath(name));
    }
}

void LogManager::handle(QtMsgType type, const QMessageLogContext &ctx, const QString &msg)
{
    int level = 1;
    const char *tag = "INFO";
    switch (type) {
    case QtDebugMsg:    level = 0; tag = "DEBUG"; break;
    case QtInfoMsg:     level = 1; tag = "INFO";  break;
    case QtWarningMsg:  level = 2; tag = "WARN";  break;
    case QtCriticalMsg: level = 3; tag = "ERROR"; break;
    default:            level = 3; tag = "ERROR"; break;
    }

    QMutexLocker lock(&s_mutex);
    if (level < s_minLevel)
        return;
    rollIfNeeded();
    if (!s_file)
        return;

    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    QString line = QString("%1 [%2] %3").arg(ts).arg(tag).arg(msg);
    if (ctx.category && *ctx.category && qstrcmp(ctx.category, "default") != 0)
        line += QString("  (cat: %1)").arg(QString::fromUtf8(ctx.category));

    // UTF-8 直接写字节流, 规避 Qt5/Qt6 QTextStream 编码 API 差异
    const QByteArray bytes = (line + '\n').toUtf8();
    s_file->write(bytes);
    s_file->flush();

    // 控制台同步输出, 后端终端可见(管理端 GUI 不展示日志)
    std::fprintf(stderr, "%s\n", line.toUtf8().constData());
    std::fflush(stderr);
}
