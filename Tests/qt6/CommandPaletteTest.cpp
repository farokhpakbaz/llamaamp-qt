/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "CommandPaletteDialog.h"

#include <QAction>
#include <QLineEdit>
#include <QListWidget>
#include <QSignalSpy>
#include <QTest>

class CommandPaletteTest final : public QObject
{
    Q_OBJECT

private slots:
    void filtersAndTriggersCommand()
    {
        CommandPaletteDialog palette;
        QAction open(QStringLiteral("Open files"), &palette);
        QAction next(QStringLiteral("Next track"), &palette);
        next.setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Right));
        QSignalSpy triggered(&next, &QAction::triggered);
        palette.setActions({&open, &next});
        palette.openPalette();

        auto *search = palette.findChild<QLineEdit *>(QStringLiteral("commandSearch"));
        auto *results = palette.findChild<QListWidget *>(QStringLiteral("commandResults"));
        QVERIFY(search);
        QVERIFY(results);
        search->setText(QStringLiteral("next right"));
        QCOMPARE(results->count(), 1);
        QTest::keyClick(search, Qt::Key_Return);
        QCOMPARE(triggered.count(), 1);
    }
};

QTEST_MAIN(CommandPaletteTest)
#include "CommandPaletteTest.moc"
