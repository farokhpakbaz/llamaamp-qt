/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <QDialog>
#include <QPointer>
#include <QVector>

class QAction;
class QEvent;
class QLineEdit;
class QListWidget;

class CommandPaletteDialog final : public QDialog
{
    Q_OBJECT

public:
    explicit CommandPaletteDialog(QWidget *parent = nullptr);
    void setActions(const QList<QAction *> &actions);
    void openPalette();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    struct Command {
        QPointer<QAction> action;
        QString label;
        QString searchableText;
    };

    void refresh(const QString &query = {});
    void moveSelection(int offset);
    void triggerCurrent();

    QLineEdit *m_search = nullptr;
    QListWidget *m_results = nullptr;
    QVector<Command> m_commands;
    QVector<QPointer<QAction>> m_visibleActions;
};
