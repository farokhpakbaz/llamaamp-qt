/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "MediaLibrary.h"

#include "PlaylistModel.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QStandardPaths>
#include <QSet>
#include <QUuid>

MediaLibrary::MediaLibrary(const QString &databasePath, QObject *parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("llamaamp-library-%1").arg(QUuid::createUuid().toString()))
    , m_database(QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName))
{
    QString path = databasePath;
    if (path.isEmpty()) {
        const QString dataDirectory = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dataDirectory);
        path = QDir(dataDirectory).filePath(QStringLiteral("media-library.sqlite"));
    }
    m_database.setDatabaseName(path);
    if (!m_database.open()) {
        m_error = m_database.lastError().text();
        return;
    }
    initializeSchema();
    if (!m_error.isEmpty())
        return;

    m_model = new QSqlTableModel(this, m_database);
    m_model->setTable(QStringLiteral("tracks"));
    m_model->setEditStrategy(QSqlTableModel::OnManualSubmit);
    m_model->setHeaderData(Title, Qt::Horizontal, tr("Title"));
    m_model->setHeaderData(Artist, Qt::Horizontal, tr("Artist"));
    m_model->setHeaderData(Album, Qt::Horizontal, tr("Album"));
    m_model->setHeaderData(Duration, Qt::Horizontal, tr("Length"));
    m_model->setSort(Title, Qt::AscendingOrder);
    refresh();
}

MediaLibrary::~MediaLibrary()
{
    if (m_model) {
        m_model->setParent(nullptr);
        delete m_model;
        m_model = nullptr;
    }
    m_database.close();
    m_database = {};
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool MediaLibrary::isOpen() const { return m_database.isOpen() && m_error.isEmpty(); }
QString MediaLibrary::errorString() const { return m_error; }
QSqlTableModel *MediaLibrary::model() const { return m_model; }

void MediaLibrary::initializeSchema()
{
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS tracks ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT, path TEXT NOT NULL UNIQUE, "
            "title TEXT NOT NULL, artist TEXT DEFAULT '', album TEXT DEFAULT '', "
            "duration INTEGER DEFAULT 0, modified INTEGER DEFAULT 0)"))) {
        m_error = query.lastError().text();
    }
}

void MediaLibrary::refresh()
{
    if (m_model && !m_model->select())
        m_error = m_model->lastError().text();
}

void MediaLibrary::addUrl(const QUrl &url)
{
    if (!isOpen() || !url.isLocalFile())
        return;
    const QFileInfo info(url.toLocalFile());
    if (!info.isFile())
        return;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO tracks(path, title, modified) VALUES(?, ?, ?) "
        "ON CONFLICT(path) DO UPDATE SET modified=excluded.modified"));
    query.addBindValue(info.absoluteFilePath());
    query.addBindValue(PlaylistModel::displayName(url));
    query.addBindValue(info.lastModified().toSecsSinceEpoch());
    if (!query.exec())
        m_error = query.lastError().text();
    refresh();
}

void MediaLibrary::addUrls(const QList<QUrl> &urls)
{
    if (!isOpen())
        return;
    m_database.transaction();
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "INSERT INTO tracks(path, title, modified) VALUES(?, ?, ?) "
        "ON CONFLICT(path) DO UPDATE SET modified=excluded.modified"));
    for (const QUrl &url : urls) {
        if (!url.isLocalFile())
            continue;
        const QFileInfo info(url.toLocalFile());
        if (!info.isFile())
            continue;
        query.bindValue(0, info.absoluteFilePath());
        query.bindValue(1, PlaylistModel::displayName(url));
        query.bindValue(2, info.lastModified().toSecsSinceEpoch());
        if (!query.exec()) {
            m_error = query.lastError().text();
            break;
        }
    }
    if (!m_database.commit())
        m_error = m_database.lastError().text();
    refresh();
}

void MediaLibrary::updateMetadata(const QUrl &url, const QString &title, const QString &artist,
                                  const QString &album, qint64 durationMs)
{
    if (!isOpen() || !url.isLocalFile())
        return;
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral(
        "UPDATE tracks SET title=?, artist=?, album=?, duration=? WHERE path=?"));
    query.addBindValue(title);
    query.addBindValue(artist);
    query.addBindValue(album);
    query.addBindValue(durationMs);
    query.addBindValue(QFileInfo(url.toLocalFile()).absoluteFilePath());
    if (!query.exec())
        m_error = query.lastError().text();
    refresh();
}

QUrl MediaLibrary::urlAt(int modelRow) const
{
    if (!m_model || modelRow < 0 || modelRow >= m_model->rowCount())
        return {};
    return QUrl::fromLocalFile(m_model->record(modelRow).value(Path).toString());
}

int MediaLibrary::removeRows(const QList<int> &modelRows)
{
    if (!isOpen() || !m_model)
        return 0;

    QSet<qlonglong> ids;
    for (int row : modelRows) {
        if (row >= 0 && row < m_model->rowCount())
            ids.insert(m_model->record(row).value(Id).toLongLong());
    }

    int removed = 0;
    m_database.transaction();
    QSqlQuery query(m_database);
    query.prepare(QStringLiteral("DELETE FROM tracks WHERE id=?"));
    for (qlonglong id : ids) {
        query.bindValue(0, id);
        if (query.exec())
            removed += query.numRowsAffected();
        else {
            m_error = query.lastError().text();
            break;
        }
    }
    if (!m_database.commit())
        m_error = m_database.lastError().text();
    refresh();
    return removed;
}

int MediaLibrary::removeMissing()
{
    if (!isOpen())
        return 0;
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("SELECT id, path FROM tracks"))) {
        m_error = query.lastError().text();
        return 0;
    }
    QList<qlonglong> missingIds;
    while (query.next()) {
        if (!QFileInfo::exists(query.value(1).toString()))
            missingIds.append(query.value(0).toLongLong());
    }
    int removed = 0;
    QSqlQuery remove(m_database);
    remove.prepare(QStringLiteral("DELETE FROM tracks WHERE id=?"));
    for (qlonglong id : missingIds) {
        remove.bindValue(0, id);
        if (remove.exec())
            removed += remove.numRowsAffected();
    }
    refresh();
    return removed;
}

bool MediaLibrary::clear()
{
    if (!isOpen())
        return false;
    QSqlQuery query(m_database);
    if (!query.exec(QStringLiteral("DELETE FROM tracks"))) {
        m_error = query.lastError().text();
        return false;
    }
    refresh();
    return true;
}
