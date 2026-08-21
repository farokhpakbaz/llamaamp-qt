/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <QString>
#include <QtPlugin>

struct LlamaAmpPluginContext {
    int apiVersion = 1;
};

class ILlamaAmpPlugin
{
public:
    virtual ~ILlamaAmpPlugin() = default;
    virtual QString name() const = 0;
    virtual QString version() const = 0;
    virtual bool initialize(const LlamaAmpPluginContext &context) = 0;
    virtual void shutdown() = 0;
};

class ILlamaAmpDspPlugin
{
public:
    virtual ~ILlamaAmpDspPlugin() = default;
    virtual void process(float *interleavedSamples, qsizetype frameCount,
                         int channelCount, int sampleRate) = 0;
};

#define LlamaAmpPlugin_iid "io.github.farokhpakbaz.llamaamp.ILlamaAmpPlugin/1.0"
#define LlamaAmpDspPlugin_iid "io.github.farokhpakbaz.llamaamp.ILlamaAmpDspPlugin/1.0"
Q_DECLARE_INTERFACE(ILlamaAmpPlugin, LlamaAmpPlugin_iid)
Q_DECLARE_INTERFACE(ILlamaAmpDspPlugin, LlamaAmpDspPlugin_iid)
