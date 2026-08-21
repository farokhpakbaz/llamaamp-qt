/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "WasabiRuntime.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QXmlStreamReader>

#include <algorithm>

bool WasabiRuntime::load(const QString &skinDirectory, QString *errorMessage)
{
    m_skinDirectory = QFileInfo(skinDirectory).canonicalFilePath();
    if (m_skinDirectory.isEmpty())
        m_skinDirectory = QDir(skinDirectory).absolutePath();
    m_skinRoot = QFileInfo(m_skinDirectory).absolutePath();
    m_visitedFiles.clear();
    m_bitmaps.clear();
    m_groups.clear();
    m_layouts.clear();
    m_actions.clear();
    m_scripts.clear();
    m_diagnostics.clear();
    m_acceleratorCount = 0;
    m_parseFailed = false;

    const QString manifest = QDir(m_skinDirectory).filePath(QStringLiteral("skin.xml"));
    if (!QFileInfo(manifest).isFile()) {
        if (errorMessage)
            *errorMessage = QStringLiteral("Missing skin.xml in %1").arg(m_skinDirectory);
        return false;
    }
    return parseFile(manifest, errorMessage);
}

QString WasabiRuntime::skinDirectory() const { return m_skinDirectory; }

QStringList WasabiRuntime::bitmapIds() const
{
    QStringList result = m_bitmaps.keys();
    result.sort(Qt::CaseInsensitive);
    return result;
}

QStringList WasabiRuntime::groupIds() const
{
    QStringList result = m_groups.keys();
    result.sort(Qt::CaseInsensitive);
    return result;
}

QStringList WasabiRuntime::layoutIds() const
{
    QStringList result = m_layouts.keys();
    result.sort(Qt::CaseInsensitive);
    return result;
}

QStringList WasabiRuntime::actions() const
{
    QStringList result(m_actions.cbegin(), m_actions.cend());
    result.sort(Qt::CaseInsensitive);
    return result;
}

QStringList WasabiRuntime::diagnostics() const { return m_diagnostics; }
QStringList WasabiRuntime::scriptFiles() const { return m_scripts; }
int WasabiRuntime::acceleratorCount() const { return m_acceleratorCount; }

const WasabiBitmapDefinition *WasabiRuntime::bitmap(const QString &id) const
{
    const auto found = m_bitmaps.constFind(id.toLower());
    return found == m_bitmaps.cend() ? nullptr : &found.value();
}

const WasabiNode *WasabiRuntime::group(const QString &id) const
{
    const auto found = m_groups.constFind(id.toLower());
    return found == m_groups.cend() ? nullptr : &found.value();
}

const WasabiNode *WasabiRuntime::layout(const QString &id) const
{
    const auto found = m_layouts.constFind(id.toLower());
    return found == m_layouts.cend() ? nullptr : &found.value();
}

bool WasabiRuntime::parseFile(const QString &path, QString *errorMessage)
{
    const QFileInfo info(path);
    QString canonicalPath = info.canonicalFilePath();
    if (canonicalPath.isEmpty())
        canonicalPath = info.absoluteFilePath();
    if (!pathIsAllowed(canonicalPath)) {
        m_diagnostics.append(QStringLiteral("Blocked include outside skin root: %1").arg(path));
        return true;
    }
    if (m_visitedFiles.contains(canonicalPath))
        return true;
    m_visitedFiles.insert(canonicalPath);

    QFile file(canonicalPath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        m_diagnostics.append(QStringLiteral("Could not read %1: %2")
                                 .arg(canonicalPath, file.errorString()));
        return true;
    }
    QString contents = QString::fromUtf8(file.readAll());
    contents.remove(QRegularExpression(QStringLiteral(R"(<\?xml[^?]*\?>)"),
                                       QRegularExpression::CaseInsensitiveOption));
    contents.prepend(QStringLiteral("<wasabi-fragment>"));
    contents.append(QStringLiteral("</wasabi-fragment>"));
    QXmlStreamReader xml(contents);
    xml.setNamespaceProcessing(false);
    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isStartElement()) {
            const WasabiNode root = readNode(xml, canonicalPath, errorMessage);
            registerNode(root, canonicalPath);
        }
    }
    if (xml.hasError()) {
        const QString message = QStringLiteral("%1:%2: %3")
                                    .arg(canonicalPath).arg(xml.lineNumber())
                                    .arg(xml.errorString());
        if (errorMessage) *errorMessage = message;
        m_parseFailed = true;
        return false;
    }
    return !m_parseFailed;
}

WasabiNode WasabiRuntime::readNode(QXmlStreamReader &xml, const QString &sourceFile,
                                   QString *errorMessage)
{
    WasabiNode node;
    node.tag = xml.qualifiedName().toString().toLower();
    for (const QXmlStreamAttribute &attribute : xml.attributes())
        node.attributes.insert(attribute.qualifiedName().toString().toLower(),
                               attribute.value().toString());
    node.id = node.attributes.value(QStringLiteral("id"));

    while (!xml.atEnd()) {
        xml.readNext();
        if (xml.isEndElement())
            break;
        if (!xml.isStartElement())
            continue;
        const QString tag = xml.qualifiedName().toString().toLower();
        if (tag == QStringLiteral("include")) {
            const QString includeName = xml.attributes().value(QStringLiteral("file")).toString();
            if (!includeName.isEmpty()) {
                const QString includePath = QDir(QFileInfo(sourceFile).absolutePath())
                                                .absoluteFilePath(includeName);
                parseFile(includePath, errorMessage);
            }
            xml.skipCurrentElement();
        } else {
            node.children.append(readNode(xml, sourceFile, errorMessage));
        }
    }
    return node;
}

void WasabiRuntime::registerNode(const WasabiNode &node, const QString &sourceFile)
{
    Q_UNUSED(sourceFile)
    const QString key = node.id.toLower();
    if (node.tag == QStringLiteral("bitmap") && !key.isEmpty()) {
        WasabiBitmapDefinition bitmap;
        bitmap.id = node.id;
        const QString fileName = node.attributes.value(QStringLiteral("file"));
        if (fileName.startsWith(QLatin1Char('$'))) {
            bitmap.file = fileName;
        } else if (!fileName.isEmpty()) {
            const QString assetPath = QDir(m_skinDirectory).absoluteFilePath(fileName);
            if (pathIsAllowed(assetPath)) {
                bitmap.file = assetPath;
            } else {
                m_diagnostics.append(
                    QStringLiteral("Blocked bitmap asset outside skin root: %1").arg(assetPath));
            }
        }
        bitmap.sourceRect = QRect(integerAttribute(node.attributes, QStringLiteral("x")),
                                  integerAttribute(node.attributes, QStringLiteral("y")),
                                  integerAttribute(node.attributes, QStringLiteral("w"), -1),
                                  integerAttribute(node.attributes, QStringLiteral("h"), -1));
        bitmap.gammaGroup = node.attributes.value(QStringLiteral("gammagroup"));
        m_bitmaps.insert(key, bitmap);
    } else if (node.tag == QStringLiteral("groupdef") && !key.isEmpty()) {
        m_groups.insert(key, node);
    } else if (node.tag == QStringLiteral("layout") && !key.isEmpty()) {
        m_layouts.insert(key, node);
    } else if (node.tag == QStringLiteral("script")) {
        const QString fileName = node.attributes.value(QStringLiteral("file"));
        if (!fileName.isEmpty()) {
            const QString scriptPath = QDir(m_skinDirectory).absoluteFilePath(fileName);
            if (pathIsAllowed(scriptPath)) {
                m_scripts.append(scriptPath);
            } else {
                m_diagnostics.append(
                    QStringLiteral("Blocked script asset outside skin root: %1").arg(scriptPath));
            }
        }
    } else if (node.tag == QStringLiteral("accelerator")) {
        ++m_acceleratorCount;
    }

    const QString action = node.attributes.value(QStringLiteral("action"));
    if (!action.isEmpty())
        m_actions.insert(action.toUpper());
    const QString menu = node.attributes.value(QStringLiteral("menu"));
    if (!menu.isEmpty())
        m_actions.insert(QStringLiteral("MENU:%1").arg(menu).toUpper());
    for (const WasabiNode &child : node.children)
        registerNode(child, sourceFile);
}

bool WasabiRuntime::pathIsAllowed(const QString &path) const
{
    const QString cleanRoot = QDir::cleanPath(m_skinRoot) + QDir::separator();
    const QFileInfo pathInfo(path);
    QString resolvedPath = pathInfo.canonicalFilePath();
    if (resolvedPath.isEmpty())
        resolvedPath = pathInfo.absoluteFilePath();
    const QString cleanPath = QDir::cleanPath(resolvedPath);
    return cleanPath == QDir::cleanPath(m_skinRoot) || cleanPath.startsWith(cleanRoot);
}

int WasabiRuntime::integerAttribute(const QHash<QString, QString> &attributes,
                                    const QString &name, int fallback)
{
    bool ok = false;
    const int value = attributes.value(name).toInt(&ok);
    return ok ? value : fallback;
}
