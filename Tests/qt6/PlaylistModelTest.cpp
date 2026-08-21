/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "PlaylistModel.h"

#include <QFile>
#include <QTemporaryDir>
#include <QTest>

class PlaylistModelTest final : public QObject
{
    Q_OBJECT

private slots:
    void addsRemovesAndTracksCurrentRow();
    void reordersWithoutLosingCurrentTrack();
    void roundTripsM3uPlaylist();
};

void PlaylistModelTest::addsRemovesAndTracksCurrentRow()
{
    PlaylistModel model;
    const QUrl first = QUrl::fromLocalFile(QStringLiteral("/music/First Song.mp3"));
    const QUrl second = QUrl::fromLocalFile(QStringLiteral("/music/Second Song.flac"));
    model.addUrls({first, second});
    model.addUrl(first);
    model.addUrls({second, first});

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0), Qt::DisplayRole).toString(), QStringLiteral("First Song"));
    QCOMPARE(model.urlAt(1), second);

    model.setCurrentRow(1);
    QVERIFY(model.data(model.index(1), PlaylistModel::CurrentRole).toBool());
    QVERIFY(model.removeRow(0));
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.currentRow(), 0);
    QCOMPARE(model.urlAt(0), second);
}

void PlaylistModelTest::reordersWithoutLosingCurrentTrack()
{
    PlaylistModel model;
    const QUrl a(QStringLiteral("https://example.test/a.mp3"));
    const QUrl b(QStringLiteral("https://example.test/b.mp3"));
    const QUrl c(QStringLiteral("https://example.test/c.mp3"));
    model.addUrls({a, b, c});
    model.setCurrentRow(1);

    QVERIFY(model.moveRows({}, 0, 1, {}, 3));
    QCOMPARE(model.urls(), QList<QUrl>({b, c, a}));
    QCOMPARE(model.currentRow(), 0);
}

void PlaylistModelTest::roundTripsM3uPlaylist()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString path = directory.filePath(QStringLiteral("queue.m3u8"));
    PlaylistModel source;
    source.addUrls({QUrl::fromLocalFile(QStringLiteral("/music/local.mp3")),
                    QUrl(QStringLiteral("https://radio.example.test/live"))});

    QString error;
    QVERIFY2(source.saveM3u(path, &error), qPrintable(error));
    PlaylistModel restored;
    QVERIFY2(restored.loadM3u(path, &error), qPrintable(error));
    QCOMPARE(restored.urls(), source.urls());
}

QTEST_GUILESS_MAIN(PlaylistModelTest)
#include "PlaylistModelTest.moc"
