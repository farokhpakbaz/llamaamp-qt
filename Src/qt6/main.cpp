/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "MainWindow.h"

#include <QApplication>
#include <QAudioDevice>
#include <QCommandLineParser>
#include <QDir>
#include <QMediaDevices>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QUrl>

int main(int argc, char *argv[])
{
#ifdef Q_OS_LINUX
    // Qt 6.10's direct PipeWire device parser can omit Bluetooth sinks when
    // used with newer PipeWire releases. The PulseAudio compatibility API is
    // backed by PipeWire on current Linux desktops and follows its real default
    // sink. Respect an explicit user override when one is already configured.
    if (qEnvironmentVariableIsEmpty("QT_AUDIO_BACKEND"))
        qputenv("QT_AUDIO_BACKEND", QByteArrayLiteral("pulseaudio"));
#endif

    QApplication application(argc, argv);
    QApplication::setApplicationName(QStringLiteral("LlamaAmp"));
    QApplication::setApplicationDisplayName(QStringLiteral("LlamaAmp Qt"));
    QApplication::setApplicationVersion(QStringLiteral("0.7.0"));
    QApplication::setOrganizationName(QStringLiteral("LlamaAmp"));
    QApplication::setOrganizationDomain(QStringLiteral("farokhpakbaz.github.io"));
    // Register a portal application ID only when its matching desktop entry is
    // installed. Development-tree launches otherwise cause the desktop portal
    // to report a misleading "App info not found" warning.
    const QString desktopEntry =
        QStringLiteral("applications/io.github.farokhpakbaz.LlamaAmp.desktop");
    if (!QStandardPaths::locate(QStandardPaths::GenericDataLocation, desktopEntry).isEmpty())
        QApplication::setDesktopFileName(QStringLiteral("io.github.farokhpakbaz.LlamaAmp"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/llamaamp.svg")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Native Qt 6 audio player"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption listAudioDevicesOption(
        QStringLiteral("list-audio-devices"),
        QStringLiteral("List available audio output devices and exit."));
    parser.addOption(listAudioDevicesOption);
    const QCommandLineOption audioOutputOption(
        {QStringLiteral("o"), QStringLiteral("audio-output")},
        QStringLiteral("Use an audio output whose name contains <name>."),
        QStringLiteral("name"));
    parser.addOption(audioOutputOption);
    const QCommandLineOption dspOption(
        QStringLiteral("dsp"), QStringLiteral("Enable the processed DSP audio path."));
    parser.addOption(dspOption);
    const QCommandLineOption equalizerOption(
        QStringLiteral("equalizer"), QStringLiteral("Apply an equalizer <preset>."),
        QStringLiteral("preset"));
    parser.addOption(equalizerOption);
    const QCommandLineOption skinOption(
        {QStringLiteral("s"), QStringLiteral("skin")},
        QStringLiteral("Apply a native or compatible XML <skin>."),
        QStringLiteral("skin"));
    parser.addOption(skinOption);
    QCommandLineOption screenshotOption(
        QStringLiteral("screenshot"), QStringLiteral("Save a UI screenshot and exit."),
        QStringLiteral("path"));
    screenshotOption.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(screenshotOption);
    parser.addPositionalArgument(QStringLiteral("files"),
                                 QStringLiteral("Audio files, folders, or playlists to open."),
                                 QStringLiteral("[files...]"));
    parser.process(application);

    if (parser.isSet(listAudioDevicesOption)) {
        QTextStream output(stdout);
        const QByteArray defaultId = QMediaDevices::defaultAudioOutput().id();
        for (const QAudioDevice &device : QMediaDevices::audioOutputs()) {
            output << (device.id() == defaultId ? "* " : "  ")
                   << device.description() << "\t" << device.id() << '\n';
        }
        return 0;
    }

    MainWindow window;
    if (parser.isSet(audioOutputOption)
        && !window.setAudioOutput(parser.value(audioOutputOption))) {
        QTextStream(stderr) << "Audio output not found: "
                            << parser.value(audioOutputOption) << '\n';
    }
    if (parser.isSet(equalizerOption))
        window.setEqualizerPreset(parser.value(equalizerOption));
    if (parser.isSet(dspOption))
        window.setDspEnabled(true);
    if (parser.isSet(skinOption) && !window.setSkin(parser.value(skinOption))) {
        QTextStream(stderr) << "Skin not found: " << parser.value(skinOption) << '\n';
        return 2;
    }
    if (!parser.positionalArguments().isEmpty()) {
        QStringList paths;
        paths.reserve(parser.positionalArguments().size());
        for (const QString &argument : parser.positionalArguments()) {
            const QUrl candidate(argument);
            if (candidate.isLocalFile())
                paths.append(QDir::cleanPath(candidate.toLocalFile()));
            else if (candidate.isValid() && !candidate.scheme().isEmpty())
                paths.append(argument);
            else
                paths.append(QDir::cleanPath(QDir::current().absoluteFilePath(argument)));
        }
        window.addPaths(paths, true);
    }
    window.show();

    if (parser.isSet(screenshotOption)) {
        const QString screenshotPath = parser.value(screenshotOption);
        QTimer::singleShot(700, &window, [&window, screenshotPath] {
            window.grab().save(screenshotPath);
            QApplication::quit();
        });
    }

    return application.exec();
}
