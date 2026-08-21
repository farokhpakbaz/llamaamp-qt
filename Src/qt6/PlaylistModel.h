/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <QAbstractListModel>
#include <QUrl>

class PlaylistModel final : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        UrlRole = Qt::UserRole + 1,
        LocationRole,
        CurrentRole,
    };

    explicit PlaylistModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    Qt::DropActions supportedDropActions() const override;
    QStringList mimeTypes() const override;
    QMimeData *mimeData(const QModelIndexList &indexes) const override;
    bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row, int column,
                      const QModelIndex &parent) override;
    bool removeRows(int row, int count, const QModelIndex &parent = {}) override;
    bool moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                  const QModelIndex &destinationParent, int destinationChild) override;

    void addUrl(const QUrl &url);
    void addUrls(const QList<QUrl> &urls);
    void clear();
    QUrl urlAt(int row) const;
    QList<QUrl> urls() const;
    int findUrl(const QUrl &url) const;
    int currentRow() const;
    void setCurrentRow(int row);
    void setTitleForUrl(const QUrl &url, const QString &title);

    bool loadM3u(const QString &path, QString *errorMessage = nullptr);
    bool saveM3u(const QString &path, QString *errorMessage = nullptr) const;

    static QString displayName(const QUrl &url);
    static bool isPlaylistFile(const QString &path);

private:
    struct Entry {
        QUrl url;
        QString title;
    };

    QList<Entry> m_entries;
    int m_currentRow = -1;
};
