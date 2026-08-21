/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "PluginManager.h"

#include "LlamaAmpPlugin.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPluginLoader>
#include <QSet>
#include <QStandardPaths>

PluginManager::PluginManager(QObject *parent)
    : QObject(parent)
{
}

PluginManager::~PluginManager()
{
    for (ILlamaAmpPlugin *plugin : m_plugins)
        plugin->shutdown();
    for (QPluginLoader *loader : m_loaders) {
        loader->unload();
        delete loader;
    }
}

QStringList PluginManager::pluginDirectories() const
{
    QStringList directories = {
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("plugins")),
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("../lib/llamaamp/plugins")),
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("../lib64/llamaamp/plugins")),
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
            .filePath(QStringLiteral("plugins")),
    };
    directories.removeDuplicates();
    return directories;
}

void PluginManager::discover()
{
    if (!m_loaders.isEmpty() || !m_plugins.isEmpty())
        return;
    for (const QString &directoryPath : pluginDirectories()) {
        QDir directory(directoryPath);
        if (!directory.exists())
            continue;
        for (const QFileInfo &entry : directory.entryInfoList(QDir::Files)) {
            if (entry.suffix().compare(QStringLiteral("dll"), Qt::CaseInsensitive) == 0) {
                m_diagnostics.append(tr("Legacy Win32 plug-in detected but not loaded on Linux: %1")
                                         .arg(entry.fileName()));
                continue;
            }
            if (entry.suffix() != QStringLiteral("so"))
                continue;
            auto *loader = new QPluginLoader(entry.absoluteFilePath());
            QObject *instance = loader->instance();
            auto *plugin = qobject_cast<ILlamaAmpPlugin *>(instance);
            if (!plugin) {
                m_diagnostics.append(tr("Rejected %1: %2").arg(entry.fileName(), loader->errorString()));
                delete loader;
                continue;
            }
            if (!plugin->initialize({1})) {
                m_diagnostics.append(tr("Initialization failed: %1").arg(plugin->name()));
                loader->unload();
                delete loader;
                continue;
            }
            m_loaders.append(loader);
            m_plugins.append(plugin);
        }
    }
}

QStringList PluginManager::loadedPlugins() const
{
    QStringList result;
    for (const ILlamaAmpPlugin *plugin : m_plugins)
        result.append(QStringLiteral("%1 %2").arg(plugin->name(), plugin->version()));
    return result;
}

QStringList PluginManager::diagnostics() const { return m_diagnostics; }

void PluginManager::process(float *interleavedSamples, qsizetype frameCount,
                            int channelCount, int sampleRate)
{
    if (!interleavedSamples || frameCount <= 0 || channelCount <= 0 || sampleRate <= 0)
        return;
    for (ILlamaAmpPlugin *plugin : m_plugins) {
        if (auto *dsp = dynamic_cast<ILlamaAmpDspPlugin *>(plugin))
            dsp->process(interleavedSamples, frameCount, channelCount, sampleRate);
    }
}

QString PluginManager::report() const
{
    QStringList lines;
    lines.append(tr("Native Linux plug-ins: %1").arg(m_plugins.size()));
    lines.append(loadedPlugins());
    if (!m_diagnostics.isEmpty()) {
        lines.append(QString{});
        lines.append(tr("Diagnostics:"));
        lines.append(m_diagnostics);
    }
    lines.append(QString{});
    lines.append(tr("Search paths:"));
    lines.append(pluginDirectories());
    return lines.join(QLatin1Char('\n'));
}
