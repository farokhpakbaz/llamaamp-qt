/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "MprisService.h"
#include "PlayerController.h"

#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QSignalSpy>
#include <QTest>

class MprisServiceTest final : public QObject
{
    Q_OBJECT

private slots:
    void registersOnSessionBus()
    {
        qputenv("LLAMAAMP_QT_NO_AUDIO", QByteArrayLiteral("1"));
        PlayerController player;
        MprisService service(&player);
        QVERIFY(QDBusConnection::sessionBus().isConnected());
        QVERIFY(service.isRegistered());
        QVERIFY(service.serviceName().startsWith(QStringLiteral("org.mpris.MediaPlayer2.llamaamp")));
        const auto reply = QDBusConnection::sessionBus().interface()->registeredServiceNames();
        QVERIFY(reply.isValid());
        QVERIFY(reply.value().contains(service.serviceName()));

        QSignalSpy volumeChanged(&player, &PlayerController::volumeChanged);
        player.setVolume(65);
        QCOMPARE(player.volume(), 65);
        QCOMPARE(volumeChanged.count(), 1);
    }
};

QTEST_MAIN(MprisServiceTest)
#include "MprisServiceTest.moc"
