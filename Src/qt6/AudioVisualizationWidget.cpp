/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "AudioVisualizationWidget.h"

#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
constexpr int SpectrumBands = 48;
constexpr int AnalysisFrames = 1024;
constexpr int WaveformPoints = 512;
}

AudioVisualizationWidget::AudioVisualizationWidget(QWidget *parent)
    : QWidget(parent)
    , m_decayTimer(new QTimer(this))
    , m_spectrum(SpectrumBands, 0.0F)
    , m_peaks(SpectrumBands, 0.0F)
{
    setObjectName(QStringLiteral("audioVisualization"));
    setMinimumHeight(220);
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_decayTimer->setInterval(33);
    connect(m_decayTimer, &QTimer::timeout, this, [this] { decay(); });
    m_decayTimer->start();
}

void AudioVisualizationWidget::setAudioSamples(const QVector<float> &samples,
                                                int channelCount, int sampleRate)
{
    // Audio buffers arrive for the lifetime of the player. Avoid performing the
    // spectrum transform while another Wasabi page (or the native UI) is shown.
    if (!isVisible())
        return;
    if (samples.isEmpty() || channelCount <= 0 || sampleRate <= 0) {
        clear();
        return;
    }

    const qsizetype frameCount = samples.size() / channelCount;
    if (frameCount <= 0) {
        clear();
        return;
    }
    const qsizetype retainedFrames = qMin<qsizetype>(frameCount, 2048);
    const qsizetype firstFrame = frameCount - retainedFrames;
    QVector<float> mono(retainedFrames);
    for (qsizetype frame = 0; frame < retainedFrames; ++frame) {
        float mixed = 0.0F;
        const qsizetype sampleOffset = (firstFrame + frame) * channelCount;
        for (int channel = 0; channel < channelCount; ++channel)
            mixed += samples.at(sampleOffset + channel);
        mono[frame] = mixed / channelCount;
    }

    const qsizetype waveformPoints = qMin<qsizetype>(WaveformPoints, mono.size());
    m_waveform.resize(waveformPoints);
    for (qsizetype point = 0; point < waveformPoints; ++point) {
        const qsizetype sourceIndex = point * mono.size() / waveformPoints;
        m_waveform[point] = mono.at(qMin(sourceIndex, mono.size() - 1));
    }

    const qsizetype analysisCount = qMin<qsizetype>(AnalysisFrames, mono.size());
    const qsizetype analysisStart = mono.size() - analysisCount;
    const float nyquist = sampleRate * 0.5F;
    const float minimumFrequency = 55.0F;
    const float maximumFrequency = qMin(16000.0F, nyquist * 0.94F);
    m_strongestBand = 0;
    float strongestLevel = -1.0F;
    for (int band = 0; band < SpectrumBands; ++band) {
        const float ratio = (band + 0.5F) / SpectrumBands;
        const float frequency = minimumFrequency
            * std::pow(maximumFrequency / minimumFrequency, ratio);
        const float omega = 2.0F * std::numbers::pi_v<float> * frequency / sampleRate;
        const float coefficient = 2.0F * std::cos(omega);
        float previous = 0.0F;
        float previousPrevious = 0.0F;
        for (qsizetype index = 0; index < analysisCount; ++index) {
            const float window = analysisCount > 1
                ? 0.5F - 0.5F * std::cos(2.0F * std::numbers::pi_v<float> * index
                                         / (analysisCount - 1))
                : 1.0F;
            const float current = mono.at(analysisStart + index) * window
                + coefficient * previous - previousPrevious;
            previousPrevious = previous;
            previous = current;
        }
        const float magnitude = 2.0F * std::sqrt(qMax(
            0.0F, previousPrevious * previousPrevious + previous * previous
                - coefficient * previous * previousPrevious)) / qMax<qsizetype>(1, analysisCount);
        const float level = qBound(0.0F, std::log10(1.0F + magnitude * 18.0F)
                                             / std::log10(19.0F), 1.0F);
        m_spectrum[band] = qMax(level, m_spectrum.at(band) * 0.72F);
        m_peaks[band] = qMax(m_spectrum.at(band), m_peaks.at(band));
        if (level > strongestLevel) {
            strongestLevel = level;
            m_strongestBand = band;
        }
    }
    m_hasAudioData = true;
    update();
}

void AudioVisualizationWidget::clear()
{
    m_waveform.clear();
    std::fill(m_spectrum.begin(), m_spectrum.end(), 0.0F);
    std::fill(m_peaks.begin(), m_peaks.end(), 0.0F);
    m_hasAudioData = false;
    m_strongestBand = -1;
    update();
}

bool AudioVisualizationWidget::hasAudioData() const { return m_hasAudioData; }
int AudioVisualizationWidget::strongestBand() const { return m_strongestBand; }
QSize AudioVisualizationWidget::sizeHint() const { return QSize(720, 360); }

void AudioVisualizationWidget::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    QLinearGradient background(0, 0, 0, height());
    background.setColorAt(0.0, QColor(7, 15, 18));
    background.setColorAt(1.0, QColor(2, 7, 9));
    painter.fillRect(rect(), background);

    const QRectF graph = QRectF(rect()).adjusted(14, 28, -14, -14);
    painter.setPen(QColor(30, 55, 61, 145));
    for (int row = 1; row < 5; ++row) {
        const qreal y = graph.top() + graph.height() * row / 5.0;
        painter.drawLine(QPointF(graph.left(), y), QPointF(graph.right(), y));
    }
    for (int column = 1; column < 12; ++column) {
        const qreal x = graph.left() + graph.width() * column / 12.0;
        painter.drawLine(QPointF(x, graph.top()), QPointF(x, graph.bottom()));
    }

    painter.setPen(QColor(141, 188, 200));
    painter.setFont(QFont(QStringLiteral("Sans Serif"), 8, QFont::Bold));
    painter.drawText(QRectF(14, 5, width() - 28, 18), Qt::AlignLeft | Qt::AlignVCenter,
                     tr("SPECTRUM ANALYZER  •  48 BANDS"));

    if (!m_hasAudioData) {
        painter.setPen(QColor(108, 135, 142));
        painter.setFont(QFont(QStringLiteral("Sans Serif"), 11));
        painter.drawText(graph, Qt::AlignCenter,
                         tr("Start playback to activate the visualization"));
        return;
    }

    const qreal slotWidth = graph.width() / SpectrumBands;
    const qreal barWidth = qMax(1.0, slotWidth - 2.0);
    for (int band = 0; band < SpectrumBands; ++band) {
        const qreal level = qBound(0.0, qreal(m_spectrum.at(band)), 1.0);
        const qreal barHeight = level * graph.height();
        const qreal x = graph.left() + slotWidth * band + 1.0;
        QLinearGradient barGradient(0, graph.bottom(), 0, graph.top());
        barGradient.setColorAt(0.0, QColor(65, 220, 113));
        barGradient.setColorAt(0.68, QColor(230, 214, 61));
        barGradient.setColorAt(1.0, QColor(255, 87, 51));
        painter.fillRect(QRectF(x, graph.bottom() - barHeight, barWidth, barHeight),
                         barGradient);
        const qreal peakY = graph.bottom() - qBound(0.0, qreal(m_peaks.at(band)), 1.0)
            * graph.height();
        painter.fillRect(QRectF(x, peakY, barWidth, 2.0), QColor(219, 240, 230));
    }

    if (m_waveform.size() > 1) {
        QPainterPath waveform;
        const qreal center = graph.center().y();
        for (qsizetype index = 0; index < m_waveform.size(); ++index) {
            const qreal x = graph.left() + graph.width() * index / (m_waveform.size() - 1);
            const qreal y = center - qBound(-1.0, qreal(m_waveform.at(index)), 1.0)
                * graph.height() * 0.32;
            if (index == 0)
                waveform.moveTo(x, y);
            else
                waveform.lineTo(x, y);
        }
        painter.setPen(QPen(QColor(127, 215, 242, 185), 1.25));
        painter.drawPath(waveform);
    }
}

void AudioVisualizationWidget::decay()
{
    if (!m_hasAudioData)
        return;
    bool active = false;
    for (int band = 0; band < SpectrumBands; ++band) {
        m_spectrum[band] *= 0.91F;
        m_peaks[band] = qMax(m_spectrum.at(band), m_peaks.at(band) - 0.018F);
        active = active || m_spectrum.at(band) > 0.001F || m_peaks.at(band) > 0.001F;
    }
    for (float &sample : m_waveform)
        sample *= 0.94F;
    if (active)
        update();
}
