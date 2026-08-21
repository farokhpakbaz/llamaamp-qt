/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "MprisService.h"

#include "PlayerController.h"
#include "PlaylistModel.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDBusAbstractAdaptor>
#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QMediaMetaData>

namespace {
constexpr auto kObjectPath = "/org/mpris/MediaPlayer2";
constexpr auto kPlayerInterface = "org.mpris.MediaPlayer2.Player";

QString playbackStatus(const PlayerController *player)
{
    switch (player->playbackState()) {
    case QMediaPlayer::PlayingState: return QStringLiteral("Playing");
    case QMediaPlayer::PausedState: return QStringLiteral("Paused");
    case QMediaPlayer::StoppedState: return QStringLiteral("Stopped");
    }
    return QStringLiteral("Stopped");
}

QDBusObjectPath trackId(const QUrl &url)
{
    if (url.isEmpty())
        return QDBusObjectPath(QStringLiteral("/org/mpris/MediaPlayer2/TrackList/NoTrack"));
    const QByteArray digest = QCryptographicHash::hash(url.toEncoded(), QCryptographicHash::Sha1)
                                  .toHex();
    return QDBusObjectPath(QStringLiteral("/org/mpris/MediaPlayer2/track/t%1")
                               .arg(QString::fromLatin1(digest)));
}
}

class MprisRootAdaptor final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit CONSTANT)
    Q_PROPERTY(bool CanRaise READ canRaise CONSTANT)
    Q_PROPERTY(bool HasTrackList READ hasTrackList CONSTANT)
    Q_PROPERTY(QString Identity READ identity CONSTANT)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes CONSTANT)

public:
    explicit MprisRootAdaptor(MprisService *service)
        : QDBusAbstractAdaptor(service), m_service(service) {}
    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return QStringLiteral("LlamaAmp Qt"); }
    QString desktopEntry() const { return QStringLiteral("io.github.farokhpakbaz.LlamaAmp"); }
    QStringList supportedUriSchemes() const
    {
        return {QStringLiteral("file"), QStringLiteral("http"), QStringLiteral("https")};
    }
    QStringList supportedMimeTypes() const
    {
        return {QStringLiteral("audio/mpeg"), QStringLiteral("audio/ogg"),
                QStringLiteral("audio/flac"), QStringLiteral("audio/x-wav"),
                QStringLiteral("audio/mp4"), QStringLiteral("audio/aac"),
                QStringLiteral("video/mp4"), QStringLiteral("video/x-matroska"),
                QStringLiteral("video/webm")};
    }

public slots:
    void Raise() { emit m_service->raiseRequested(); }
    void Quit() { emit m_service->quitRequested(); }

private:
    MprisService *m_service;
};

class MprisPlayerAdaptor final : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle)
    Q_PROPERTY(QVariantMap Metadata READ metadata)
    Q_PROPERTY(double Volume READ volume WRITE setVolume)
    Q_PROPERTY(qint64 Position READ position)
    Q_PROPERTY(double MinimumRate READ minimumRate CONSTANT)
    Q_PROPERTY(double MaximumRate READ maximumRate CONSTANT)
    Q_PROPERTY(bool CanGoNext READ canGoNext)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious)
    Q_PROPERTY(bool CanPlay READ canPlay)
    Q_PROPERTY(bool CanPause READ canPause CONSTANT)
    Q_PROPERTY(bool CanSeek READ canSeek)
    Q_PROPERTY(bool CanControl READ canControl CONSTANT)

public:
    explicit MprisPlayerAdaptor(MprisService *service)
        : QDBusAbstractAdaptor(service), m_service(service) {}
    QString playbackStatus() const { return ::playbackStatus(m_service->m_player); }
    QString loopStatus() const { return m_service->m_loopStatus; }
    void setLoopStatus(const QString &status)
    {
        if (status != QStringLiteral("None") && status != QStringLiteral("Track")
            && status != QStringLiteral("Playlist"))
            return;
        emit m_service->loopStatusRequested(status);
    }
    double rate() const { return 1.0; }
    void setRate(double) {}
    bool shuffle() const { return m_service->m_shuffle; }
    void setShuffle(bool enabled) { emit m_service->shuffleRequested(enabled); }
    QVariantMap metadata() const
    {
        const PlayerController *player = m_service->m_player;
        const QUrl source = player->source();
        const QMediaMetaData data = player->metaData();
        QVariantMap result;
        result.insert(QStringLiteral("mpris:trackid"), QVariant::fromValue(trackId(source)));
        if (source.isEmpty())
            return result;
        QString title = data.stringValue(QMediaMetaData::Title);
        if (title.isEmpty())
            title = PlaylistModel::displayName(source);
        result.insert(QStringLiteral("xesam:title"), title);
        QString artist = data.stringValue(QMediaMetaData::AlbumArtist);
        if (artist.isEmpty())
            artist = data.stringValue(QMediaMetaData::ContributingArtist);
        if (!artist.isEmpty())
            result.insert(QStringLiteral("xesam:artist"), QStringList{artist});
        const QString album = data.stringValue(QMediaMetaData::AlbumTitle);
        if (!album.isEmpty())
            result.insert(QStringLiteral("xesam:album"), album);
        result.insert(QStringLiteral("xesam:url"), source.toString(QUrl::FullyEncoded));
        if (player->duration() > 0)
            result.insert(QStringLiteral("mpris:length"), player->duration() * 1000);
        return result;
    }
    double volume() const { return m_service->m_player->volume() / 100.0; }
    void setVolume(double value) { m_service->m_player->setVolume(qRound(value * 100.0)); }
    qint64 position() const { return m_service->m_player->position() * 1000; }
    double minimumRate() const { return 1.0; }
    double maximumRate() const { return 1.0; }
    bool canGoNext() const { return m_service->m_canGoNext; }
    bool canGoPrevious() const { return m_service->m_canGoPrevious; }
    bool canPlay() const
    {
        return m_service->m_canGoNext || !m_service->m_player->source().isEmpty();
    }
    bool canPause() const { return true; }
    bool canSeek() const { return m_service->m_player->duration() > 0; }
    bool canControl() const { return true; }

public slots:
    void Next() { emit m_service->nextRequested(); }
    void Previous() { emit m_service->previousRequested(); }
    void Pause() { m_service->m_player->pause(); }
    void PlayPause()
    {
        if (m_service->m_player->playbackState() == QMediaPlayer::PlayingState)
            m_service->m_player->pause();
        else
            emit m_service->playRequested();
    }
    void Stop() { m_service->m_player->stop(); }
    void Play() { emit m_service->playRequested(); }
    void Seek(qint64 offset)
    {
        const qint64 target = qBound<qint64>(0, position() + offset,
                                             m_service->m_player->duration() * 1000);
        m_service->m_player->seek(target / 1000);
        m_service->emitSeeked(target);
    }
    void SetPosition(const QDBusObjectPath &id, qint64 requestedPosition)
    {
        if (id.path() != trackId(m_service->m_player->source()).path())
            return;
        const qint64 target = qBound<qint64>(0, requestedPosition,
                                             m_service->m_player->duration() * 1000);
        m_service->m_player->seek(target / 1000);
        m_service->emitSeeked(target);
    }
    void OpenUri(const QString &uri) { emit m_service->openUriRequested(uri); }

signals:
    void Seeked(qint64 position);

private:
    MprisService *m_service;
};

MprisService::MprisService(PlayerController *player, QObject *parent)
    : QObject(parent), m_player(player)
{
    m_rootAdaptor = new MprisRootAdaptor(this);
    m_playerAdaptor = new MprisPlayerAdaptor(this);
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected())
        return;
    if (!bus.registerObject(QString::fromLatin1(kObjectPath), this,
                            QDBusConnection::ExportAdaptors))
        return;
    m_serviceName = QStringLiteral("org.mpris.MediaPlayer2.llamaamp");
    if (!bus.registerService(m_serviceName)) {
        m_serviceName += QStringLiteral(".instance%1").arg(QApplication::applicationPid());
        if (!bus.registerService(m_serviceName)) {
            bus.unregisterObject(QString::fromLatin1(kObjectPath));
            m_serviceName.clear();
            return;
        }
    }
    m_registered = true;

    connect(player, &PlayerController::playbackStateChanged, this, [this] {
        notifyPlayerProperties({{QStringLiteral("PlaybackStatus"), playbackStatus(m_player)}});
    });
    auto notifyMetadata = [this] {
        notifyPlayerProperties({{QStringLiteral("Metadata"), m_playerAdaptor->metadata()},
                                {QStringLiteral("CanPlay"),
                                 m_canGoNext || !m_player->source().isEmpty()},
                                {QStringLiteral("CanSeek"), m_player->duration() > 0}});
    };
    connect(player, &PlayerController::metadataChanged, this, notifyMetadata);
    connect(player, &PlayerController::sourceChanged, this, notifyMetadata);
    connect(player, &PlayerController::durationChanged, this, notifyMetadata);
    connect(player, &PlayerController::volumeChanged, this, [this](int percent) {
        notifyPlayerProperties({{QStringLiteral("Volume"), percent / 100.0}});
    });
}

MprisService::~MprisService()
{
    if (!m_registered)
        return;
    QDBusConnection bus = QDBusConnection::sessionBus();
    bus.unregisterService(m_serviceName);
    bus.unregisterObject(QString::fromLatin1(kObjectPath));
}

void MprisService::setCanGoNext(bool enabled)
{
    if (m_canGoNext == enabled)
        return;
    m_canGoNext = enabled;
    notifyPlayerProperties({{QStringLiteral("CanGoNext"), enabled},
                            {QStringLiteral("CanPlay"),
                             enabled || !m_player->source().isEmpty()}});
}

void MprisService::setCanGoPrevious(bool enabled)
{
    if (m_canGoPrevious == enabled)
        return;
    m_canGoPrevious = enabled;
    notifyPlayerProperties({{QStringLiteral("CanGoPrevious"), enabled}});
}

void MprisService::setShuffleState(bool enabled)
{
    if (m_shuffle == enabled)
        return;
    m_shuffle = enabled;
    notifyPlayerProperties({{QStringLiteral("Shuffle"), enabled}});
}

void MprisService::setLoopStatusState(const QString &status)
{
    if (m_loopStatus == status)
        return;
    m_loopStatus = status;
    notifyPlayerProperties({{QStringLiteral("LoopStatus"), status}});
}

void MprisService::notifyPlayerProperties(const QVariantMap &properties)
{
    if (!m_registered)
        return;
    QDBusMessage message = QDBusMessage::createSignal(
        QString::fromLatin1(kObjectPath), QStringLiteral("org.freedesktop.DBus.Properties"),
        QStringLiteral("PropertiesChanged"));
    message.setArguments({QString::fromLatin1(kPlayerInterface), properties, QStringList{}});
    QDBusConnection::sessionBus().send(message);
}

void MprisService::emitSeeked(qint64 microseconds)
{
    if (m_playerAdaptor)
        emit m_playerAdaptor->Seeked(microseconds);
}

#include "MprisService.moc"
