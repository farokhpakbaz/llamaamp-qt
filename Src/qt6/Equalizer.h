/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <QObject>
#include <QVector>

#include <array>

class Equalizer final : public QObject
{
    Q_OBJECT

public:
    static constexpr int BandCount = 10;

    explicit Equalizer(QObject *parent = nullptr);
    float bandGain(int band) const;
    void setBandGain(int band, float decibels);
    void setGains(const QList<float> &decibels);
    QList<float> gains() const;
    QStringList presetNames() const;
    void applyPreset(const QString &name);
    void reset();
    void process(float *interleavedSamples, qsizetype frameCount,
                 int channelCount, int sampleRate);
    static int frequency(int band);

signals:
    void changed();

private:
    struct Coefficients { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    struct State { double x1 = 0, x2 = 0, y1 = 0, y2 = 0; };
    void rebuild(int sampleRate, int channelCount);

    std::array<float, BandCount> m_gains{};
    std::array<Coefficients, BandCount> m_coefficients{};
    QVector<State> m_state;
    int m_sampleRate = 0;
    int m_channelCount = 0;
    bool m_dirty = true;
};
