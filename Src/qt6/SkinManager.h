/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <QString>
#include <QStringList>

struct LegacySkinInfo {
    QString name;
    QString version;
    QString author;
    QString comment;
    QString directory;
    QString screenshot;
    QString rootElement;
    int includeCount = 0;
    int scriptCount = 0;
    int acceleratorCount = 0;
    QString error;
};

class QApplication;

class SkinManager final
{
public:
    static QStringList availableSkins();
    static QStringList discoveredLegacySkins();
    static QList<LegacySkinInfo> legacySkinCatalog();
    static QString compatibilityReport();
    static QString applyPalette(const QString &baseStyleSheet, const QString &skinName);
    static bool canRenderWasabiSkin(const LegacySkinInfo &skin);
};
