/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "PlayerController.h"

#include "Equalizer.h"
#include "PluginManager.h"

#include <QAudioBuffer>
#include <QAudioBufferOutput>
#include <QAudioOutput>
#include <QAudioSink>
#include <QMediaDevices>

#include <algorithm>
#include <limits>

PlayerController::PlayerController(QObject *parent)
    : QObject(parent)
    , m_player(new QMediaPlayer(this))
    , m_equalizer(new Equalizer(this))
    , m_pluginManager(new PluginManager(this))
{
    m_player->setLoops(QMediaPlayer::Once);
    m_pluginManager->discover();
    connect(m_player, &QMediaPlayer::positionChanged, this, &PlayerController::positionChanged);
    connect(m_player, &QMediaPlayer::durationChanged, this, &PlayerController::durationChanged);
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, &PlayerController::playbackStateChanged);
    connect(m_player, &QMediaPlayer::metaDataChanged, this, &PlayerController::metadataChanged);
    connect(m_player, &QMediaPlayer::sourceChanged, this, &PlayerController::sourceChanged);
    connect(m_player, &QMediaPlayer::hasVideoChanged, this, &PlayerController::hasVideoChanged);
    connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
                if (status == QMediaPlayer::EndOfMedia)
                    emit endOfMedia();
            });
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error error, const QString &message) {
                if (error != QMediaPlayer::NoError)
                    emit errorOccurred(message);
            });
    if (!qEnvironmentVariableIsSet("LLAMAAMP_QT_NO_AUDIO")) {
        m_mediaDevices = new QMediaDevices(this);
        m_audioOutput = new QAudioOutput(this);
        m_player->setAudioOutput(m_audioOutput);
        m_bufferOutput = new QAudioBufferOutput(this);
        m_player->setAudioBufferOutput(m_bufferOutput);
        connect(m_bufferOutput, &QAudioBufferOutput::audioBufferReceived,
                this, &PlayerController::processAudioBuffer);
        connect(m_mediaDevices, &QMediaDevices::audioOutputsChanged,
                this, &PlayerController::audioOutputsChanged);
    }
}

void PlayerController::play(const QUrl &source)
{
    if (source != m_player->source())
        m_player->setSource(source);
    m_player->play();
}

void PlayerController::play() { m_player->play(); }
void PlayerController::pause() { m_player->pause(); }
void PlayerController::stop()
{
    m_player->stop();
    stopDspSink();
}
void PlayerController::clear()
{
    m_player->stop();
    m_player->setSource({});
    stopDspSink();
}

void PlayerController::toggle()
{
    if (playbackState() == QMediaPlayer::PlayingState)
        pause();
    else
        play();
}

void PlayerController::seek(qint64 position)
{
    stopDspSink();
    m_player->setPosition(position);
}

void PlayerController::setVolume(int percent)
{
    percent = qBound(0, percent, 100);
    if (percent == m_volume)
        return;
    m_volume = percent;
    if (m_audioOutput)
        m_audioOutput->setVolume(percent / 100.0);
    if (m_dspSink)
        m_dspSink->setVolume(percent / 100.0);
    emit volumeChanged(percent);
}

QUrl PlayerController::source() const { return m_player->source(); }
qint64 PlayerController::position() const { return m_player->position(); }
qint64 PlayerController::duration() const { return m_player->duration(); }
int PlayerController::volume() const { return m_volume; }
QMediaPlayer::PlaybackState PlayerController::playbackState() const { return m_player->playbackState(); }
QMediaMetaData PlayerController::metaData() const { return m_player->metaData(); }
QList<QAudioDevice> PlayerController::audioOutputs() const
{
    return m_mediaDevices ? QMediaDevices::audioOutputs() : QList<QAudioDevice>{};
}
QByteArray PlayerController::audioOutputId() const
{
    return m_audioOutput ? m_audioOutput->device().id() : QByteArray{};
}

bool PlayerController::setAudioOutput(const QByteArray &id)
{
    if (!m_audioOutput)
        return false;
    for (const QAudioDevice &device : audioOutputs()) {
        if (device.id() == id) {
            m_audioOutput->setDevice(device);
            stopDspSink();
            return true;
        }
    }
    return false;
}

bool PlayerController::setAudioOutput(const QString &name)
{
    if (!m_audioOutput)
        return false;
    for (const QAudioDevice &device : audioOutputs()) {
        if (device.description().contains(name, Qt::CaseInsensitive)
            || QString::fromUtf8(device.id()).compare(name, Qt::CaseInsensitive) == 0) {
            m_audioOutput->setDevice(device);
            stopDspSink();
            return true;
        }
    }
    return false;
}

void PlayerController::setVideoOutput(QObject *output)
{
    m_player->setVideoOutput(output);
}

Equalizer *PlayerController::equalizer() const { return m_equalizer; }
PluginManager *PlayerController::pluginManager() const { return m_pluginManager; }
bool PlayerController::dspEnabled() const { return m_dspEnabled; }

void PlayerController::setDspEnabled(bool enabled)
{
    if (enabled == m_dspEnabled || !m_audioOutput)
        return;
    m_dspEnabled = enabled;
    m_audioOutput->setMuted(enabled);
    if (!enabled)
        stopDspSink();
    emit dspEnabledChanged(enabled);
}

void PlayerController::stopDspSink()
{
    m_dspDevice = nullptr;
    if (m_dspSink) {
        m_dspSink->stop();
        delete m_dspSink;
        m_dspSink = nullptr;
    }
}

void PlayerController::processAudioBuffer(const QAudioBuffer &buffer)
{
    if (!buffer.isValid() || buffer.frameCount() <= 0)
        return;
    const QAudioFormat format = buffer.format();
    if (format.channelCount() <= 0 || format.sampleRate() <= 0)
        return;

    const qsizetype sampleCount = buffer.sampleCount();
    QVector<float> samples(sampleCount);
    switch (format.sampleFormat()) {
    case QAudioFormat::UInt8: {
        const auto *source = buffer.constData<quint8>();
        for (qsizetype i = 0; i < sampleCount; ++i)
            samples[i] = (source[i] - 128.0F) / 128.0F;
        break;
    }
    case QAudioFormat::Int16: {
        const auto *source = buffer.constData<qint16>();
        for (qsizetype i = 0; i < sampleCount; ++i)
            samples[i] = source[i] / 32768.0F;
        break;
    }
    case QAudioFormat::Int32: {
        const auto *source = buffer.constData<qint32>();
        for (qsizetype i = 0; i < sampleCount; ++i)
            samples[i] = source[i] / 2147483648.0F;
        break;
    }
    case QAudioFormat::Float: {
        const auto *source = buffer.constData<float>();
        std::copy(source, source + sampleCount, samples.begin());
        break;
    }
    default:
        return;
    }

    emit audioSamplesReady(samples, format.channelCount(), format.sampleRate());
    if (!m_dspEnabled || !m_audioOutput)
        return;
    if (!m_dspSink || m_dspSink->format() != format) {
        stopDspSink();
        m_dspSink = new QAudioSink(m_audioOutput->device(), format, this);
        m_dspSink->setVolume(m_audioOutput->volume());
        m_dspDevice = m_dspSink->start();
    }
    if (!m_dspDevice)
        return;

    m_equalizer->process(samples.data(), buffer.frameCount(), format.channelCount(),
                         format.sampleRate());
    m_pluginManager->process(samples.data(), buffer.frameCount(), format.channelCount(),
                             format.sampleRate());
    QByteArray processed(buffer.byteCount(), Qt::Uninitialized);
    switch (format.sampleFormat()) {
    case QAudioFormat::UInt8: {
        auto *target = reinterpret_cast<quint8 *>(processed.data());
        for (qsizetype i = 0; i < sampleCount; ++i)
            target[i] = quint8(std::clamp(samples[i] * 128.0F + 128.0F, 0.0F, 255.0F));
        break;
    }
    case QAudioFormat::Int16: {
        auto *target = reinterpret_cast<qint16 *>(processed.data());
        for (qsizetype i = 0; i < sampleCount; ++i)
            target[i] = qint16(std::clamp(samples[i] * 32767.0F, -32768.0F, 32767.0F));
        break;
    }
    case QAudioFormat::Int32: {
        auto *target = reinterpret_cast<qint32 *>(processed.data());
        for (qsizetype i = 0; i < sampleCount; ++i)
            target[i] = qint32(std::clamp(double(samples[i]) * 2147483647.0,
                                         -2147483648.0, 2147483647.0));
        break;
    }
    case QAudioFormat::Float:
        std::copy(samples.cbegin(), samples.cend(), reinterpret_cast<float *>(processed.data()));
        break;
    default:
        return;
    }
    m_dspDevice->write(processed);
}
