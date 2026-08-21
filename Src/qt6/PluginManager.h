/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <QObject>
#include <QStringList>

class ILlamaAmpPlugin;
class QPluginLoader;

class PluginManager final : public QObject
{
    Q_OBJECT

public:
    explicit PluginManager(QObject *parent = nullptr);
    ~PluginManager() override;

    void discover();
    QStringList loadedPlugins() const;
    QStringList diagnostics() const;
    QString report() const;
    void process(float *interleavedSamples, qsizetype frameCount,
                 int channelCount, int sampleRate);

private:
    QStringList pluginDirectories() const;

    QList<QPluginLoader *> m_loaders;
    QList<ILlamaAmpPlugin *> m_plugins;
    QStringList m_diagnostics;
};
