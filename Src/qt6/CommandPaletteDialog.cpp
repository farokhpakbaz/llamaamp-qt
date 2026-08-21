/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CommandPaletteDialog.h"

#include <QAction>
#include <QBoxLayout>
#include <QEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QRegularExpression>

#include <utility>

namespace {
QString cleanLabel(QString text)
{
    text.remove(QLatin1Char('&'));
    text.remove(QRegularExpression(QStringLiteral("[\\.\\x{2026}]+$")));
    return text.trimmed();
}
}

CommandPaletteDialog::CommandPaletteDialog(QWidget *parent)
    : QDialog(parent)
{
    setObjectName(QStringLiteral("commandPalette"));
    setWindowTitle(tr("Command Palette"));
    setModal(true);
    resize(560, 410);
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(10);
    auto *title = new QLabel(tr("Jump to anything"), this);
    title->setObjectName(QStringLiteral("sectionTitle"));
    layout->addWidget(title);
    m_search = new QLineEdit(this);
    m_search->setObjectName(QStringLiteral("commandSearch"));
    m_search->setPlaceholderText(tr("Type a command…"));
    m_search->setClearButtonEnabled(true);
    m_search->setAccessibleName(tr("Command search"));
    m_search->installEventFilter(this);
    layout->addWidget(m_search);
    m_results = new QListWidget(this);
    m_results->setObjectName(QStringLiteral("commandResults"));
    m_results->setAccessibleName(tr("Matching commands"));
    m_results->setAlternatingRowColors(true);
    layout->addWidget(m_results, 1);
    auto *hint = new QLabel(tr("↑↓ navigate   Enter run   Esc close"), this);
    hint->setObjectName(QStringLiteral("eyebrow"));
    layout->addWidget(hint);

    connect(m_search, &QLineEdit::textChanged, this, &CommandPaletteDialog::refresh);
    connect(m_search, &QLineEdit::returnPressed, this, &CommandPaletteDialog::triggerCurrent);
    connect(m_results, &QListWidget::itemActivated, this,
            [this] { triggerCurrent(); });
}

void CommandPaletteDialog::setActions(const QList<QAction *> &actions)
{
    m_commands.clear();
    for (QAction *action : actions) {
        if (!action || action->isSeparator() || action->menu() || action->text().isEmpty())
            continue;
        QString category;
        if (const auto *menu = qobject_cast<const QMenu *>(action->parent()))
            category = cleanLabel(menu->title());
        const QString name = cleanLabel(action->text());
        const QString label = category.isEmpty() ? name
            : tr("%1  ·  %2").arg(category, name);
        const QString shortcut = action->shortcut().toString(QKeySequence::NativeText);
        m_commands.append({action, shortcut.isEmpty() ? label
                                                       : tr("%1    %2").arg(label, shortcut),
                           QStringLiteral("%1 %2 %3 %4")
                               .arg(category, name, action->toolTip(), shortcut).toCaseFolded()});
    }
    refresh(m_search->text());
}

void CommandPaletteDialog::openPalette()
{
    m_search->clear();
    refresh();
    show();
    raise();
    activateWindow();
    m_search->setFocus();
}

void CommandPaletteDialog::refresh(const QString &query)
{
    const QStringList terms = query.toCaseFolded().split(QRegularExpression(QStringLiteral("\\s+")),
                                                         Qt::SkipEmptyParts);
    m_results->clear();
    m_visibleActions.clear();
    for (const Command &command : std::as_const(m_commands)) {
        if (!command.action || !command.action->isEnabled())
            continue;
        bool matches = true;
        for (const QString &term : terms)
            matches = matches && command.searchableText.contains(term);
        if (!matches)
            continue;
        m_results->addItem(command.label);
        m_visibleActions.append(command.action);
    }
    if (m_results->count() > 0)
        m_results->setCurrentRow(0);
}

bool CommandPaletteDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_search && event->type() == QEvent::KeyPress) {
        const auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Down) {
            moveSelection(1);
            return true;
        }
        if (key->key() == Qt::Key_Up) {
            moveSelection(-1);
            return true;
        }
    }
    return QDialog::eventFilter(watched, event);
}

void CommandPaletteDialog::moveSelection(int offset)
{
    if (m_results->count() == 0)
        return;
    const int current = qMax(0, m_results->currentRow());
    m_results->setCurrentRow(qBound(0, current + offset, m_results->count() - 1));
}

void CommandPaletteDialog::triggerCurrent()
{
    const int row = m_results->currentRow();
    if (row < 0 || row >= m_visibleActions.size() || !m_visibleActions.at(row))
        return;
    const QPointer<QAction> action = m_visibleActions.at(row);
    accept();
    if (action)
        action->trigger();
}
