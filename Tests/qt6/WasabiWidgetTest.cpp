/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "AudioVisualizationWidget.h"
#include "MediaLibrary.h"
#include "PlayerController.h"
#include "PlaylistModel.h"
#include "WasabiPlayerWidget.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>
#include <numbers>

class WasabiWidgetTest final : public QObject
{
    Q_OBJECT

private slots:
    void toolbarAndPlayerControlsDispatch();
    void visualizationRendersPcm();
};

void WasabiWidgetTest::toolbarAndPlayerControlsDispatch()
{
    qputenv("LLAMAAMP_QT_NO_AUDIO", QByteArrayLiteral("1"));
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    PlayerController player;
    PlaylistModel playlist;
    MediaLibrary library(directory.filePath(QStringLiteral("library.sqlite")));
    WasabiPlayerWidget widget(&player, &playlist, &library);
    widget.resize(800, 600);
    widget.show();

    QWidget *deck = widget.findChild<QWidget *>(QStringLiteral("wasabiDeck"));
    QVERIFY(deck);
    QSignalSpy menuSpy(&widget, &WasabiPlayerWidget::toolbarMenuRequested);
    QSignalSpy openSpy(&widget, &WasabiPlayerWidget::openRequested);
    QSignalSpy shuffleSpy(&widget, &WasabiPlayerWidget::shuffleToggled);
    QSignalSpy repeatSpy(&widget, &WasabiPlayerWidget::repeatRequested);

    QTest::mouseClick(deck, Qt::LeftButton, {}, QPoint(82, 8));
    QCOMPARE(menuSpy.count(), 1);
    QCOMPARE(menuSpy.first().at(0).toInt(), 0);
    QTest::mouseClick(deck, Qt::LeftButton, {}, QPoint(116, 8));
    QCOMPARE(menuSpy.count(), 2);
    QCOMPARE(menuSpy.at(1).at(0).toInt(), 1);
    QTest::mouseClick(deck, Qt::LeftButton, {}, QPoint(150, 8));
    QCOMPARE(menuSpy.count(), 3);
    QCOMPARE(menuSpy.at(2).at(0).toInt(), 3);
    QTest::mouseClick(deck, Qt::LeftButton, {}, QPoint(198, 8));
    QCOMPARE(menuSpy.count(), 4);
    QCOMPARE(menuSpy.at(3).at(0).toInt(), 4);
    QTest::mouseClick(deck, Qt::LeftButton, {}, QPoint(232, 8));
    QCOMPARE(menuSpy.count(), 5);
    QCOMPARE(menuSpy.at(4).at(0).toInt(), 5);
    QTest::mouseClick(deck, Qt::LeftButton, {}, QPoint(130, 100));
    QCOMPARE(openSpy.count(), 1);
    QTest::mouseClick(deck, Qt::LeftButton, {}, QPoint(154, 100));
    QCOMPARE(shuffleSpy.count(), 1);
    QTest::mouseClick(deck, Qt::LeftButton, {}, QPoint(184, 100));
    QCOMPARE(repeatSpy.count(), 1);
}

void WasabiWidgetTest::visualizationRendersPcm()
{
    AudioVisualizationWidget visualization;
    visualization.resize(720, 360);
    visualization.show();

    constexpr int sampleRate = 48000;
    constexpr int frames = 2048;
    QVector<float> samples(frames * 2);
    for (int frame = 0; frame < frames; ++frame) {
        const float sample = 0.75F * std::sin(2.0F * std::numbers::pi_v<float>
                                             * 440.0F * frame / sampleRate);
        samples[frame * 2] = sample;
        samples[frame * 2 + 1] = sample;
    }
    visualization.setAudioSamples(samples, 2, sampleRate);
    QVERIFY(visualization.hasAudioData());
    QVERIFY(visualization.strongestBand() >= 12);
    QVERIFY(visualization.strongestBand() <= 22);

    QImage rendered(visualization.size(), QImage::Format_RGB32);
    rendered.fill(Qt::black);
    visualization.render(&rendered);
    QVERIFY(!rendered.isNull());
    visualization.clear();
    QVERIFY(!visualization.hasAudioData());
}

QTEST_MAIN(WasabiWidgetTest)
#include "WasabiWidgetTest.moc"
