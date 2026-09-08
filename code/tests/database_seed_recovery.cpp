#include <QCoreApplication>
#include <QDebug>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>

#include "DatabaseManager.h"

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temp;
    if (!temp.isValid())
        return 1;

    const QString dbPath = temp.filePath("test.db");
    const bool freshDatabase = app.arguments().contains("--fresh");
    if (!freshDatabase) {
        const QString connection = "partial-seed-setup";
        QSqlDatabase partial = QSqlDatabase::addDatabase("QSQLITE", connection);
        partial.setDatabaseName(dbPath);
        if (!partial.open())
            return 2;
        QSqlQuery setup(partial);
        if (!setup.exec("CREATE TABLE station(id INTEGER PRIMARY KEY AUTOINCREMENT,"
                        "name TEXT NOT NULL,address TEXT,longitude REAL,latitude REAL,"
                        "price REAL DEFAULT 1.0,status INTEGER DEFAULT 0,"
                        "create_time TEXT DEFAULT CURRENT_TIMESTAMP)"))
            return 3;
        if (!setup.exec("CREATE TABLE app_data_migration(version TEXT PRIMARY KEY,"
                        "applied_at TEXT DEFAULT CURRENT_TIMESTAMP)"))
            return 4;
        if (!setup.exec("INSERT INTO station(name,address,longitude,latitude,price) "
                        "VALUES('遗留测试站点','北京市',116.4,39.9,1.2)"))
            return 5;
        if (!setup.exec("INSERT INTO app_data_migration(version) "
                        "VALUES('beijing_real_stations_20260908_v1')"))
            return 6;
        partial.close();
        partial = QSqlDatabase();
        QSqlDatabase::removeDatabase(connection);
    }

    qputenv("CHARGING_DB", dbPath.toUtf8());
    QString error;
    if (!DatabaseManager::instance().init(&error)) {
        qCritical() << error;
        return 7;
    }

    QSqlQuery query;
    const int expectedPiles = freshDatabase ? 147 : 59;
    if (!query.exec("SELECT COUNT(*) FROM pile") || !query.next()
        || query.value(0).toInt() < expectedPiles)
        return 8;
    if (!query.exec("SELECT COUNT(*) FROM user") || !query.next()
        || query.value(0).toInt() < 1)
        return 9;
    if (!query.exec("SELECT COUNT(*) FROM charge_order") || !query.next()
        || query.value(0).toInt() < 90)
        return 10;

    qInfo() << (freshDatabase
                    ? "PASS: fresh database populated with bundled data"
                    : "PASS: incomplete database restored with piles, user and demo orders");
    return 0;
}
