/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "PlaylistModel.h"

#include <QColor>
#include <QDataStream>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QMimeData>
#include <QSet>
#include <QTextStream>

namespace {
constexpr auto kPlaylistRowMime = "application/x-llamaamp-qt-playlist-row";
}

PlaylistModel::PlaylistModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int PlaylistModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant PlaylistModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size())
        return {};

    const Entry &entry = m_entries.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return entry.title;
    case Qt::ToolTipRole:
    case LocationRole:
        return entry.url.isLocalFile() ? entry.url.toLocalFile() : entry.url.toDisplayString();
    case UrlRole:
        return entry.url;
    case CurrentRole:
        return index.row() == m_currentRow;
    case Qt::FontRole: {
        QFont font;
        font.setBold(index.row() == m_currentRow);
        return font;
    }
    case Qt::ForegroundRole:
        if (index.row() == m_currentRow)
            return QColor(QStringLiteral("#8cff9b"));
        return {};
    default:
        return {};
    }
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex &index) const
{
    if (!index.isValid())
        return Qt::ItemIsDropEnabled;
    return QAbstractListModel::flags(index) | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled;
}

Qt::DropActions PlaylistModel::supportedDropActions() const
{
    return Qt::MoveAction;
}

QStringList PlaylistModel::mimeTypes() const
{
    return {QString::fromLatin1(kPlaylistRowMime)};
}

QMimeData *PlaylistModel::mimeData(const QModelIndexList &indexes) const
{
    auto *mime = new QMimeData;
    if (!indexes.isEmpty()) {
        QByteArray encoded;
        QDataStream stream(&encoded, QIODevice::WriteOnly);
        stream << indexes.first().row();
        mime->setData(QString::fromLatin1(kPlaylistRowMime), encoded);
    }
    return mime;
}

bool PlaylistModel::dropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                                 int, const QModelIndex &parent)
{
    if (action == Qt::IgnoreAction)
        return true;
    if (!data->hasFormat(QString::fromLatin1(kPlaylistRowMime)))
        return false;
    QByteArray encoded = data->data(QString::fromLatin1(kPlaylistRowMime));
    QDataStream stream(&encoded, QIODevice::ReadOnly);
    int sourceRow = -1;
    stream >> sourceRow;
    int destination = row;
    if (destination < 0)
        destination = parent.isValid() ? parent.row() : rowCount();
    return moveRows({}, sourceRow, 1, {}, destination);
}

bool PlaylistModel::removeRows(int row, int count, const QModelIndex &parent)
{
    if (parent.isValid() || row < 0 || count <= 0 || row + count > m_entries.size())
        return false;
    const QUrl currentUrl = urlAt(m_currentRow);
    beginRemoveRows({}, row, row + count - 1);
    for (int index = 0; index < count; ++index)
        m_entries.removeAt(row);
    endRemoveRows();
    m_currentRow = findUrl(currentUrl);
    return true;
}

bool PlaylistModel::moveRows(const QModelIndex &sourceParent, int sourceRow, int count,
                             const QModelIndex &destinationParent, int destinationChild)
{
    if (sourceParent.isValid() || destinationParent.isValid() || count <= 0 || sourceRow < 0
        || sourceRow + count > m_entries.size() || destinationChild < 0
        || destinationChild > m_entries.size()
        || (destinationChild >= sourceRow && destinationChild <= sourceRow + count)) {
        return false;
    }

    const QUrl currentUrl = urlAt(m_currentRow);
    beginMoveRows({}, sourceRow, sourceRow + count - 1, {}, destinationChild);
    QList<Entry> moving;
    moving.reserve(count);
    for (int index = 0; index < count; ++index)
        moving.append(m_entries.takeAt(sourceRow));
    int insertionRow = destinationChild;
    if (destinationChild > sourceRow)
        insertionRow -= count;
    for (int index = 0; index < moving.size(); ++index)
        m_entries.insert(insertionRow + index, moving.at(index));
    endMoveRows();
    m_currentRow = findUrl(currentUrl);
    emit dataChanged(this->index(0), this->index(qMax(0, rowCount() - 1)),
                     {CurrentRole, Qt::FontRole, Qt::ForegroundRole});
    return true;
}

void PlaylistModel::addUrl(const QUrl &url)
{
    if (!url.isValid() || findUrl(url) >= 0)
        return;
    const int row = m_entries.size();
    beginInsertRows({}, row, row);
    m_entries.append({url, displayName(url)});
    endInsertRows();
}

void PlaylistModel::addUrls(const QList<QUrl> &urls)
{
    QList<Entry> additions;
    QSet<QUrl> seen;
    for (const Entry &entry : std::as_const(m_entries))
        seen.insert(entry.url);
    for (const QUrl &url : urls) {
        if (url.isValid() && !seen.contains(url)) {
            additions.append({url, displayName(url)});
            seen.insert(url);
        }
    }
    if (additions.isEmpty())
        return;
    const int first = m_entries.size();
    beginInsertRows({}, first, first + additions.size() - 1);
    m_entries.append(additions);
    endInsertRows();
}

void PlaylistModel::clear()
{
    if (m_entries.isEmpty())
        return;
    beginResetModel();
    m_entries.clear();
    m_currentRow = -1;
    endResetModel();
}

QUrl PlaylistModel::urlAt(int row) const
{
    return row >= 0 && row < m_entries.size() ? m_entries.at(row).url : QUrl{};
}

QList<QUrl> PlaylistModel::urls() const
{
    QList<QUrl> result;
    result.reserve(m_entries.size());
    for (const Entry &entry : m_entries)
        result.append(entry.url);
    return result;
}

int PlaylistModel::findUrl(const QUrl &url) const
{
    if (url.isEmpty())
        return -1;
    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).url == url)
            return row;
    }
    return -1;
}

int PlaylistModel::currentRow() const
{
    return m_currentRow;
}

void PlaylistModel::setCurrentRow(int row)
{
    row = row >= 0 && row < m_entries.size() ? row : -1;
    if (row == m_currentRow)
        return;
    const int previous = m_currentRow;
    m_currentRow = row;
    if (previous >= 0)
        emit dataChanged(index(previous), index(previous),
                         {CurrentRole, Qt::FontRole, Qt::ForegroundRole});
    if (m_currentRow >= 0)
        emit dataChanged(index(m_currentRow), index(m_currentRow),
                         {CurrentRole, Qt::FontRole, Qt::ForegroundRole});
}

void PlaylistModel::setTitleForUrl(const QUrl &url, const QString &title)
{
    const int row = findUrl(url);
    if (row < 0 || title.trimmed().isEmpty() || m_entries.at(row).title == title)
        return;
    m_entries[row].title = title;
    emit dataChanged(index(row), index(row), {Qt::DisplayRole});
}

bool PlaylistModel::loadM3u(const QString &path, QString *errorMessage)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    const QDir baseDirectory = QFileInfo(path).absoluteDir();
    QList<QUrl> parsed;
    QTextStream stream(&file);
    while (!stream.atEnd()) {
        const QString line = stream.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const QUrl candidate(line);
        if (candidate.isValid() && !candidate.scheme().isEmpty())
            parsed.append(candidate);
        else
            parsed.append(QUrl::fromLocalFile(baseDirectory.absoluteFilePath(line)));
    }
    addUrls(parsed);
    return true;
}

bool PlaylistModel::saveM3u(const QString &path, QString *errorMessage) const
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = file.errorString();
        return false;
    }
    QTextStream stream(&file);
    stream << "#EXTM3U\n";
    for (const Entry &entry : m_entries)
        stream << (entry.url.isLocalFile() ? entry.url.toLocalFile() : entry.url.toString()) << '\n';
    return true;
}

QString PlaylistModel::displayName(const QUrl &url)
{
    if (url.isLocalFile())
        return QFileInfo(url.toLocalFile()).completeBaseName();
    return url.fileName().isEmpty() ? url.toDisplayString() : url.fileName();
}

bool PlaylistModel::isPlaylistFile(const QString &path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QStringLiteral("m3u") || suffix == QStringLiteral("m3u8");
}
