/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <QAudioDevice>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QObject>
#include <QUrl>
#include <QVector>

class QAudioOutput;
class QAudioBuffer;
class QAudioBufferOutput;
class QAudioSink;
class QIODevice;
class QMediaDevices;
class Equalizer;
class PluginManager;

class PlayerController final : public QObject
{
    Q_OBJECT

public:
    explicit PlayerController(QObject *parent = nullptr);

    void play(const QUrl &source);
    void play();
    void pause();
    void stop();
    void clear();
    void toggle();
    void seek(qint64 position);
    void setVolume(int percent);

    QUrl source() const;
    qint64 position() const;
    qint64 duration() const;
    int volume() const;
    QMediaPlayer::PlaybackState playbackState() const;
    QMediaMetaData metaData() const;

    QList<QAudioDevice> audioOutputs() const;
    QByteArray audioOutputId() const;
    bool setAudioOutput(const QByteArray &id);
    bool setAudioOutput(const QString &name);
    void setVideoOutput(QObject *output);
    Equalizer *equalizer() const;
    PluginManager *pluginManager() const;
    bool dspEnabled() const;
    void setDspEnabled(bool enabled);

signals:
    void positionChanged(qint64 position);
    void durationChanged(qint64 duration);
    void playbackStateChanged(QMediaPlayer::PlaybackState state);
    void metadataChanged();
    void sourceChanged(const QUrl &source);
    void endOfMedia();
    void errorOccurred(const QString &message);
    void audioOutputsChanged();
    void hasVideoChanged(bool available);
    void volumeChanged(int percent);
    void dspEnabledChanged(bool enabled);
    void audioSamplesReady(const QVector<float> &samples, int channelCount, int sampleRate);

private:
    QMediaPlayer *m_player = nullptr;
    QMediaDevices *m_mediaDevices = nullptr;
    QAudioOutput *m_audioOutput = nullptr;
    QAudioBufferOutput *m_bufferOutput = nullptr;
    QAudioSink *m_dspSink = nullptr;
    QIODevice *m_dspDevice = nullptr;
    Equalizer *m_equalizer = nullptr;
    PluginManager *m_pluginManager = nullptr;
    bool m_dspEnabled = false;
    int m_volume = 100;

    void processAudioBuffer(const QAudioBuffer &buffer);
    void stopDspSink();
};
