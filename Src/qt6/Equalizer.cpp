/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "Equalizer.h"

#include <QtMath>

#include <algorithm>
#include <cmath>

namespace {
constexpr std::array<int, Equalizer::BandCount> kFrequencies = {
    31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000
};
}
Equalizer::Equalizer(QObject *parent)
    : QObject(parent)
{
}

float Equalizer::bandGain(int band) const
{
    return band >= 0 && band < BandCount ? m_gains.at(band) : 0.0F;
}

void Equalizer::setBandGain(int band, float decibels)
{
    if (band < 0 || band >= BandCount)
        return;
    decibels = std::clamp(decibels, -12.0F, 12.0F);
    if (qFuzzyCompare(m_gains.at(band), decibels))
        return;
    m_gains[band] = decibels;
    m_dirty = true;
    emit changed();
}

void Equalizer::setGains(const QList<float> &decibels)
{
    for (int band = 0; band < qMin(BandCount, decibels.size()); ++band)
        m_gains[band] = std::clamp(decibels.at(band), -12.0F, 12.0F);
    m_dirty = true;
    emit changed();
}

QList<float> Equalizer::gains() const
{
    return QList<float>(m_gains.begin(), m_gains.end());
}

QStringList Equalizer::presetNames() const
{
    return {tr("Flat"), tr("Rock"), tr("Classical"), tr("Bass Boost"), tr("Vocal")};
}

void Equalizer::applyPreset(const QString &name)
{
    if (name == tr("Rock"))
        setGains({5, 3, -1, -2, 0, 2, 4, 5, 5, 4});
    else if (name == tr("Classical"))
        setGains({4, 3, 2, 1, -1, -1, 0, 2, 3, 4});
    else if (name == tr("Bass Boost"))
        setGains({8, 7, 5, 2, 0, 0, 0, 0, 0, 0});
    else if (name == tr("Vocal"))
        setGains({-2, -2, 0, 3, 5, 5, 3, 1, -1, -2});
    else
        reset();
}

void Equalizer::reset()
{
    m_gains.fill(0.0F);
    m_dirty = true;
    emit changed();
}

int Equalizer::frequency(int band)
{
    return band >= 0 && band < BandCount ? kFrequencies.at(band) : 0;
}

void Equalizer::rebuild(int sampleRate, int channelCount)
{
    m_sampleRate = sampleRate;
    m_channelCount = channelCount;
    m_state.fill({}, BandCount * channelCount);
    constexpr double quality = 1.4;
    for (int band = 0; band < BandCount; ++band) {
        const double usableFrequency = qMin(double(kFrequencies.at(band)), sampleRate * 0.45);
        const double a = std::pow(10.0, m_gains.at(band) / 40.0);
        const double omega = 2.0 * M_PI * usableFrequency / sampleRate;
        const double alpha = std::sin(omega) / (2.0 * quality);
        const double a0 = 1.0 + alpha / a;
        m_coefficients[band] = {
            (1.0 + alpha * a) / a0,
            (-2.0 * std::cos(omega)) / a0,
            (1.0 - alpha * a) / a0,
            (-2.0 * std::cos(omega)) / a0,
            (1.0 - alpha / a) / a0,
        };
    }
    m_dirty = false;
}

void Equalizer::process(float *samples, qsizetype frameCount, int channelCount, int sampleRate)
{
    if (!samples || frameCount <= 0 || channelCount <= 0 || sampleRate <= 0)
        return;
    if (m_dirty || sampleRate != m_sampleRate || channelCount != m_channelCount)
        rebuild(sampleRate, channelCount);
    for (qsizetype frame = 0; frame < frameCount; ++frame) {
        for (int channel = 0; channel < channelCount; ++channel) {
            double sample = samples[frame * channelCount + channel];
            for (int band = 0; band < BandCount; ++band) {
                State &state = m_state[band * channelCount + channel];
                const Coefficients &c = m_coefficients.at(band);
                const double output = c.b0 * sample + c.b1 * state.x1 + c.b2 * state.x2
                    - c.a1 * state.y1 - c.a2 * state.y2;
                state.x2 = state.x1;
                state.x1 = sample;
                state.y2 = state.y1;
                state.y1 = output;
                sample = output;
            }
            samples[frame * channelCount + channel] = std::clamp(float(sample), -1.0F, 1.0F);
        }
    }
}
