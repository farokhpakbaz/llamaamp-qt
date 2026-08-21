/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <QObject>
#include <QString>

class PlayerController;
class MprisRootAdaptor;
class MprisPlayerAdaptor;

class MprisService final : public QObject
{
    Q_OBJECT

public:
    explicit MprisService(PlayerController *player, QObject *parent = nullptr);
    ~MprisService() override;

    bool isRegistered() const { return m_registered; }
    QString serviceName() const { return m_serviceName; }
    void setCanGoNext(bool enabled);
    void setCanGoPrevious(bool enabled);
    void setShuffleState(bool enabled);
    void setLoopStatusState(const QString &status);

signals:
    void raiseRequested();
    void quitRequested();
    void nextRequested();
    void previousRequested();
    void playRequested();
    void openUriRequested(const QString &uri);
    void shuffleRequested(bool enabled);
    void loopStatusRequested(const QString &status);

private:
    friend class MprisRootAdaptor;
    friend class MprisPlayerAdaptor;

    void notifyPlayerProperties(const QVariantMap &properties);
    void emitSeeked(qint64 microseconds);

    PlayerController *m_player = nullptr;
    MprisRootAdaptor *m_rootAdaptor = nullptr;
    MprisPlayerAdaptor *m_playerAdaptor = nullptr;
    QString m_serviceName;
    bool m_registered = false;
    bool m_canGoNext = false;
    bool m_canGoPrevious = false;
    bool m_shuffle = false;
    QString m_loopStatus = QStringLiteral("None");
};
