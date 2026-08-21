/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QUrl>

class QSqlTableModel;

class MediaLibrary final : public QObject
{
    Q_OBJECT

public:
    enum Column { Id, Path, Title, Artist, Album, Duration, Modified, ColumnCount };

    explicit MediaLibrary(const QString &databasePath = {}, QObject *parent = nullptr);
    ~MediaLibrary() override;

    bool isOpen() const;
    QString errorString() const;
    QSqlTableModel *model() const;
    void addUrl(const QUrl &url);
    void addUrls(const QList<QUrl> &urls);
    void updateMetadata(const QUrl &url, const QString &title, const QString &artist,
                        const QString &album, qint64 durationMs);
    QUrl urlAt(int modelRow) const;
    int removeRows(const QList<int> &modelRows);
    int removeMissing();
    bool clear();

private:
    void initializeSchema();
    void refresh();

    QString m_connectionName;
    QSqlDatabase m_database;
    QSqlTableModel *m_model = nullptr;
    QString m_error;
};
