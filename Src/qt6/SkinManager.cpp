/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "SkinManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QXmlStreamReader>
#include <QFile>

#include <algorithm>

QStringList SkinManager::availableSkins()
{
    return {QStringLiteral("Llama Green"), QStringLiteral("Amber Glow"),
            QStringLiteral("Midnight Blue")};
}

QStringList SkinManager::discoveredLegacySkins()
{
    QStringList skins;
    for (const LegacySkinInfo &skin : legacySkinCatalog())
        skins.append(skin.name);
    return skins;
}

QList<LegacySkinInfo> SkinManager::legacySkinCatalog()
{
    QStringList roots;
    const QString configuredRoots = qEnvironmentVariable("LLAMAAMP_SKIN_PATH");
    if (!configuredRoots.isEmpty())
        roots.append(configuredRoots.split(QDir::listSeparator(), Qt::SkipEmptyParts));
    roots.append({
        QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("skins")),
        QDir(QCoreApplication::applicationDirPath())
            .filePath(QStringLiteral("../share/llamaamp/skins")),
        QDir(QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation))
            .filePath(QStringLiteral("llamaamp/skins")),
        QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
            .filePath(QStringLiteral("skins")),
    });
    roots.removeDuplicates();
    QList<LegacySkinInfo> skins;
    for (const QString &root : roots) {
        QDir directory(root);
        for (const QFileInfo &entry : directory.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            const QString manifest = QDir(entry.absoluteFilePath()).filePath(QStringLiteral("skin.xml"));
            if (!QFileInfo::exists(manifest))
                continue;
            LegacySkinInfo skin;
            skin.name = entry.fileName();
            skin.directory = entry.absoluteFilePath();
            QFile file(manifest);
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                skin.error = file.errorString();
                skins.append(skin);
                continue;
            }
            QXmlStreamReader xml(&file);
            QString currentInfoElement;
            while (!xml.atEnd()) {
                xml.readNext();
                if (xml.isStartElement()) {
                    const QString element = xml.name().toString();
                    if (skin.rootElement.isEmpty())
                        skin.rootElement = element;
                    if (element == QStringLiteral("include"))
                        ++skin.includeCount;
                    else if (element == QStringLiteral("script"))
                        ++skin.scriptCount;
                    else if (element == QStringLiteral("accelerator"))
                        ++skin.acceleratorCount;
                    if (element == QStringLiteral("name") || element == QStringLiteral("version")
                        || element == QStringLiteral("author") || element == QStringLiteral("comment")
                        || element == QStringLiteral("screenshot")) {
                        currentInfoElement = element;
                    }
                } else if (xml.isCharacters() && !xml.isWhitespace() && !currentInfoElement.isEmpty()) {
                    const QString text = xml.text().toString().trimmed();
                    if (currentInfoElement == QStringLiteral("name")) skin.name = text;
                    else if (currentInfoElement == QStringLiteral("version")) skin.version = text;
                    else if (currentInfoElement == QStringLiteral("author")) skin.author = text;
                    else if (currentInfoElement == QStringLiteral("comment")) skin.comment = text;
                    else if (currentInfoElement == QStringLiteral("screenshot"))
                        skin.screenshot = QDir(skin.directory).filePath(text);
                    currentInfoElement.clear();
                }
            }
            if (xml.hasError())
                skin.error = xml.errorString();
            skins.append(skin);
        }
    }
    std::sort(skins.begin(), skins.end(), [](const LegacySkinInfo &left, const LegacySkinInfo &right) {
        return left.name.localeAwareCompare(right.name) < 0;
    });
    for (int index = skins.size() - 1; index > 0; --index) {
        if (skins.at(index).directory == skins.at(index - 1).directory)
            skins.removeAt(index);
    }
    return skins;
}

QString SkinManager::compatibilityReport()
{
    const QStringList legacy = discoveredLegacySkins();
    QString report = QObject::tr("Native Qt skins: %1\n").arg(availableSkins().join(QStringLiteral(", ")));
    report += QObject::tr("Detected compatible XML skins: %1\n")
                  .arg(legacy.isEmpty() ? QObject::tr("none") : legacy.join(QStringLiteral(", ")));
    report += QObject::tr("Compatible user-supplied Bento-family layouts can use the partial "
                          "player surface. No third-party skins are bundled. Other XML layouts "
                          "and MAKI scripts are catalogued but not executed.");
    return report;
}

QString SkinManager::applyPalette(const QString &baseStyleSheet, const QString &skinName)
{
    QString result = baseStyleSheet;
    if (skinName == QStringLiteral("Amber Glow")) {
        const QList<QPair<QString, QString>> colors = {
            {QStringLiteral("#a1ffae"), QStringLiteral("#ffd36a")},
            {QStringLiteral("#92f6a0"), QStringLiteral("#ffc247")},
            {QStringLiteral("#4d9d5a"), QStringLiteral("#a56d16")},
            {QStringLiteral("#72db81"), QStringLiteral("#e5a63d")},
            {QStringLiteral("#67ce76"), QStringLiteral("#d6932f")},
            {QStringLiteral("#a3ffaf"), QStringLiteral("#ffe09a")},
            {QStringLiteral("#2d5134"), QStringLiteral("#62461e")},
            {QStringLiteral("#315c39"), QStringLiteral("#705023")},
            {QStringLiteral("#68d678"), QStringLiteral("#d69b3f")},
        };
        for (const auto &[from, to] : colors)
            result.replace(from, to);
    } else if (skinName == QStringLiteral("Midnight Blue")) {
        const QList<QPair<QString, QString>> colors = {
            {QStringLiteral("#a1ffae"), QStringLiteral("#8fdcff")},
            {QStringLiteral("#92f6a0"), QStringLiteral("#73cfff")},
            {QStringLiteral("#4d9d5a"), QStringLiteral("#327aa3")},
            {QStringLiteral("#72db81"), QStringLiteral("#63bde9")},
            {QStringLiteral("#67ce76"), QStringLiteral("#4ca8d4")},
            {QStringLiteral("#a3ffaf"), QStringLiteral("#a6e4ff")},
            {QStringLiteral("#2d5134"), QStringLiteral("#23475d")},
            {QStringLiteral("#315c39"), QStringLiteral("#28536b")},
            {QStringLiteral("#68d678"), QStringLiteral("#52b8e9")},
        };
        for (const auto &[from, to] : colors)
            result.replace(from, to);
    }
    return result;
}

bool SkinManager::canRenderWasabiSkin(const LegacySkinInfo &skin)
{
    const QString directoryName = QFileInfo(skin.directory).fileName();
    return directoryName.contains(QStringLiteral("Bento"), Qt::CaseInsensitive);
}
