/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <QHash>
#include <QRect>
#include <QSet>
#include <QStringList>

struct WasabiBitmapDefinition {
    QString id;
    QString file;
    QRect sourceRect;
    QString gammaGroup;
};

struct WasabiNode {
    QString tag;
    QString id;
    QHash<QString, QString> attributes;
    QList<WasabiNode> children;
};

class QXmlStreamReader;

class WasabiRuntime final
{
public:
    bool load(const QString &skinDirectory, QString *errorMessage = nullptr);

    QString skinDirectory() const;
    QStringList bitmapIds() const;
    QStringList groupIds() const;
    QStringList layoutIds() const;
    QStringList actions() const;
    QStringList diagnostics() const;
    QStringList scriptFiles() const;
    int acceleratorCount() const;

    const WasabiBitmapDefinition *bitmap(const QString &id) const;
    const WasabiNode *group(const QString &id) const;
    const WasabiNode *layout(const QString &id) const;

private:
    bool parseFile(const QString &path, QString *errorMessage);
    WasabiNode readNode(QXmlStreamReader &xml, const QString &sourceFile,
                        QString *errorMessage);
    void registerNode(const WasabiNode &node, const QString &sourceFile);
    bool pathIsAllowed(const QString &path) const;
    static int integerAttribute(const QHash<QString, QString> &attributes,
                                const QString &name, int fallback = 0);

    QString m_skinDirectory;
    QString m_skinRoot;
    QSet<QString> m_visitedFiles;
    QHash<QString, WasabiBitmapDefinition> m_bitmaps;
    QHash<QString, WasabiNode> m_groups;
    QHash<QString, WasabiNode> m_layouts;
    QSet<QString> m_actions;
    QStringList m_scripts;
    QStringList m_diagnostics;
    int m_acceleratorCount = 0;
    bool m_parseFailed = false;
};
