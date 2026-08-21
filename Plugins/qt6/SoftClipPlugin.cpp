/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SoftClipPlugin.h"

#include <cmath>

QString SoftClipPlugin::name() const { return tr("Soft Clipper"); }
QString SoftClipPlugin::version() const { return QStringLiteral("1.0"); }

bool SoftClipPlugin::initialize(const LlamaAmpPluginContext &context)
{
    m_initialized = context.apiVersion == 1;
    return m_initialized;
}

void SoftClipPlugin::shutdown() { m_initialized = false; }

void SoftClipPlugin::process(float *interleavedSamples, qsizetype frameCount,
                             int channelCount, int sampleRate)
{
    Q_UNUSED(sampleRate)
    if (!m_initialized || !interleavedSamples)
        return;

    constexpr float drive = 1.15F;
    const float normalization = std::tanh(drive);
    const qsizetype sampleCount = frameCount * channelCount;
    for (qsizetype i = 0; i < sampleCount; ++i)
        interleavedSamples[i] = std::tanh(interleavedSamples[i] * drive) / normalization;
}
