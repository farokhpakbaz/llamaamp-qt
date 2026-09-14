/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "AudioVisualizationWidget.h"
#include "Equalizer.h"
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
    widget.resize(445, 680);
    widget.show();

    QWidget *deck = widget.findChild<QWidget *>(QStringLiteral("wasabiDeck"));
    QVERIFY(deck);
    QSignalSpy openSpy(&widget, &WasabiPlayerWidget::openRequested);
    QSignalSpy shuffleSpy(&widget, &WasabiPlayerWidget::shuffleToggled);
    QSignalSpy repeatSpy(&widget, &WasabiPlayerWidget::repeatRequested);
    QSignalSpy shadeSpy(&widget, &WasabiPlayerWidget::windowShadeChanged);

    QTest::mouseClick(deck, Qt::LeftButton, {}, QPoint(244, 155));
    QCOMPARE(openSpy.count(), 1);
    QTest::mouseClick(deck, Qt::LeftButton, {}, QPoint(300, 155));
    QCOMPARE(shuffleSpy.count(), 1);
    QTest::mouseClick(deck, Qt::LeftButton, {}, QPoint(370, 155));
    QCOMPARE(repeatSpy.count(), 1);
    QTest::mouseClick(deck, Qt::LeftButton, {}, QPoint(deck->width() - 44, 12));
    QCOMPARE(shadeSpy.count(), 1);
    QVERIFY(widget.isWindowShaded());
    QTest::mouseClick(deck, Qt::LeftButton, {}, QPoint(deck->width() - 44, 12));
    QCOMPARE(shadeSpy.count(), 2);
    QVERIFY(!widget.isWindowShaded());

    QWidget *equalizer = widget.findChild<QWidget *>(QStringLiteral("classicEqualizer"));
    QVERIFY(equalizer);
    QTest::mouseClick(equalizer, Qt::LeftButton, {}, QPoint(395, 35));
    QTest::mouseClick(equalizer, Qt::LeftButton, {}, QPoint(395, 35));
    QCOMPARE(player.equalizer()->bandGain(0), 5.0F);
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
