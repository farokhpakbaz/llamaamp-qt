/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "LlamaAmpPlugin.h"

#include <QObject>

class SoftClipPlugin final : public QObject, public ILlamaAmpPlugin, public ILlamaAmpDspPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID LlamaAmpPlugin_iid FILE "softclip.json")
    Q_INTERFACES(ILlamaAmpPlugin ILlamaAmpDspPlugin)

public:
    QString name() const override;
    QString version() const override;
    bool initialize(const LlamaAmpPluginContext &context) override;
    void shutdown() override;
    void process(float *interleavedSamples, qsizetype frameCount,
                 int channelCount, int sampleRate) override;

private:
    bool m_initialized = false;
};
