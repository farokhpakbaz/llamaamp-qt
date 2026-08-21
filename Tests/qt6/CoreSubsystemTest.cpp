/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Equalizer.h"
#include "MediaLibrary.h"
#include "PluginManager.h"
#include "SkinManager.h"
#include "WasabiRuntime.h"

#include <QFile>
#include <QSqlRecord>
#include <QSqlTableModel>
#include <QTemporaryDir>
#include <QTest>

#include <cmath>

class CoreSubsystemTest final : public QObject
{
    Q_OBJECT

private slots:
    void flatEqualizerPreservesSamples();
    void equalizerPresetProcessesSamples();
    void mediaLibraryPersistsAndUpdatesMetadata();
    void skinPalettesAndPluginReportAreAvailable();
    void wasabiRuntimeRejectsMalformedAndUnsafeInput();
};

void CoreSubsystemTest::flatEqualizerPreservesSamples()
{
    Equalizer equalizer;
    QVector<float> samples(512);
    for (int index = 0; index < samples.size(); ++index)
        samples[index] = std::sin(index * 0.03F) * 0.5F;
    const QVector<float> original = samples;
    equalizer.process(samples.data(), samples.size() / 2, 2, 48000);
    for (int index = 0; index < samples.size(); ++index)
        QVERIFY(std::abs(samples.at(index) - original.at(index)) < 0.00001F);
}

void CoreSubsystemTest::equalizerPresetProcessesSamples()
{
    Equalizer equalizer;
    equalizer.applyPreset(QStringLiteral("Bass Boost"));
    QVector<float> samples(4096);
    for (int frame = 0; frame < samples.size() / 2; ++frame) {
        const float value = std::sin(2.0 * M_PI * 62.0 * frame / 48000.0) * 0.15F;
        samples[frame * 2] = value;
        samples[frame * 2 + 1] = value;
    }
    const QVector<float> original = samples;
    equalizer.process(samples.data(), samples.size() / 2, 2, 48000);
    QVERIFY(samples != original);
    for (float sample : samples)
        QVERIFY(std::isfinite(sample));
}

void CoreSubsystemTest::mediaLibraryPersistsAndUpdatesMetadata()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString mediaPath = directory.filePath(QStringLiteral("track.mp3"));
    QFile media(mediaPath);
    QVERIFY(media.open(QIODevice::WriteOnly));
    media.write("test");
    media.close();
    const QString databasePath = directory.filePath(QStringLiteral("library.sqlite"));

    {
        MediaLibrary library(databasePath);
        QVERIFY2(library.isOpen(), qPrintable(library.errorString()));
        library.addUrl(QUrl::fromLocalFile(mediaPath));
        QCOMPARE(library.model()->rowCount(), 1);
        library.updateMetadata(QUrl::fromLocalFile(mediaPath), QStringLiteral("Track Title"),
                               QStringLiteral("Artist"), QStringLiteral("Album"), 123000);
        QCOMPARE(library.model()->record(0).value(MediaLibrary::Title).toString(),
                 QStringLiteral("Track Title"));
        QCOMPARE(library.removeMissing(), 0);
    }
    MediaLibrary restored(databasePath);
    QVERIFY(restored.isOpen());
    QCOMPARE(restored.model()->rowCount(), 1);
    QCOMPARE(restored.urlAt(0), QUrl::fromLocalFile(mediaPath));
    QVERIFY(QFile::remove(mediaPath));
    QCOMPARE(restored.removeMissing(), 1);
    QCOMPARE(restored.model()->rowCount(), 0);
}

void CoreSubsystemTest::skinPalettesAndPluginReportAreAvailable()
{
    QVERIFY(SkinManager::availableSkins().contains(QStringLiteral("Amber Glow")));
    const QString styled = SkinManager::applyPalette(QStringLiteral("color:#a1ffae;"),
                                                       QStringLiteral("Amber Glow"));
    QVERIFY(styled.contains(QStringLiteral("#ffd36a")));

    QTemporaryDir skinRoot;
    QVERIFY(skinRoot.isValid());
    const QString skinDirectory = skinRoot.filePath(QStringLiteral("TestSkin"));
    QVERIFY(QDir().mkpath(skinDirectory));
    QFile manifest(QDir(skinDirectory).filePath(QStringLiteral("skin.xml")));
    QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::Text));
    manifest.write(R"(<WinampAbstractionLayer>
        <bitmap id="player.button.play.normal" file="player.png" x="0" y="0" w="16" h="16"/>
        <groupdef id="player.layout"><button action="PLAY" menu="WA5:FILE"/></groupdef>
        <layout id="normal"/>
        <script file="player.maki"/>
    </WinampAbstractionLayer>)");
    manifest.close();
    qputenv("LLAMAAMP_SKIN_PATH", skinRoot.path().toUtf8());
    bool catalogued = false;
    for (const LegacySkinInfo &skin : SkinManager::legacySkinCatalog())
        catalogued = catalogued || skin.directory == skinDirectory;
    qunsetenv("LLAMAAMP_SKIN_PATH");
    QVERIFY(catalogued);
    WasabiRuntime runtime;
    QString runtimeError;
    QVERIFY2(runtime.load(skinDirectory, &runtimeError), qPrintable(runtimeError));
    QVERIFY2(runtime.bitmap(QStringLiteral("player.button.play.normal")) != nullptr,
             qPrintable(QStringLiteral("bitmaps=%1 diagnostics=%2")
                            .arg(runtime.bitmapIds().join(QLatin1Char(',')),
                                 runtime.diagnostics().join(QLatin1Char('|')))));
    QVERIFY(runtime.group(QStringLiteral("player.layout")) != nullptr);
    QVERIFY(runtime.layout(QStringLiteral("normal")) != nullptr);
    QVERIFY(runtime.actions().contains(QStringLiteral("PLAY")));
    QVERIFY(runtime.actions().contains(QStringLiteral("MENU:WA5:FILE")));
    QVERIFY(!runtime.scriptFiles().isEmpty());
    PluginManager plugins;
    plugins.discover();
    QVERIFY(plugins.loadedPlugins().contains(QStringLiteral("Soft Clipper 1.0")));
    float sample = 0.9F;
    plugins.process(&sample, 1, 1, 48000);
    QVERIFY(sample != 0.9F);
    QVERIFY(plugins.report().contains(QStringLiteral("Search paths")));
}

void CoreSubsystemTest::wasabiRuntimeRejectsMalformedAndUnsafeInput()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    const QString skinDirectory = directory.filePath(QStringLiteral("skins/Test"));
    QVERIFY(QDir().mkpath(skinDirectory));

    QFile manifest(QDir(skinDirectory).filePath(QStringLiteral("skin.xml")));
    QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::Text));
    manifest.write(R"(<WinampAbstractionLayer>
        <bitmap id="safe" file="images/safe.png"/>
        <bitmap id="unsafe" file="../../outside.png"/>
        <script file="../../outside.maki"/>
        <include file="../../outside.xml"/>
    </WinampAbstractionLayer>)");
    manifest.close();

    WasabiRuntime runtime;
    QString error;
    QVERIFY2(runtime.load(skinDirectory, &error), qPrintable(error));
    QVERIFY(runtime.bitmap(QStringLiteral("safe")) != nullptr);
    QVERIFY(runtime.bitmap(QStringLiteral("unsafe")) != nullptr);
    QVERIFY(runtime.bitmap(QStringLiteral("unsafe"))->file.isEmpty());
    QVERIFY(runtime.scriptFiles().isEmpty());
    QVERIFY(runtime.diagnostics().join(QLatin1Char('\n')).contains(QStringLiteral("Blocked")));

    QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    manifest.write("<broken><xml></broken>");
    manifest.close();
    QVERIFY(!runtime.load(skinDirectory, &error));
    QVERIFY(!error.isEmpty());

    QFile include(QDir(skinDirectory).filePath(QStringLiteral("broken.xml")));
    QVERIFY(include.open(QIODevice::WriteOnly | QIODevice::Text));
    include.write("<included><broken></included>");
    include.close();
    QVERIFY(manifest.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text));
    manifest.write("<include file=\"broken.xml\"/>");
    manifest.close();
    error.clear();
    QVERIFY(!runtime.load(skinDirectory, &error));
    QVERIFY(error.contains(QStringLiteral("broken.xml")));
}

QTEST_GUILESS_MAIN(CoreSubsystemTest)
#include "CoreSubsystemTest.moc"
