/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <QVector>
#include <QWidget>

class QTimer;

class AudioVisualizationWidget final : public QWidget
{
public:
    explicit AudioVisualizationWidget(QWidget *parent = nullptr);

    void setAudioSamples(const QVector<float> &samples, int channelCount, int sampleRate);
    void clear();
    bool hasAudioData() const;
    int strongestBand() const;
    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    void decay();

    QTimer *m_decayTimer = nullptr;
    QVector<float> m_waveform;
    QVector<float> m_spectrum;
    QVector<float> m_peaks;
    bool m_hasAudioData = false;
    int m_strongestBand = -1;
};
