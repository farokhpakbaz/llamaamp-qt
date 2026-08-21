/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "WasabiPlayerWidget.h"

#include "AudioVisualizationWidget.h"
#include "PlayerController.h"
#include "PlaylistModel.h"
#include "MediaLibrary.h"

#include <QBoxLayout>
#include <QFileInfo>
#include <QLabel>
#include <QListView>
#include <QHeaderView>
#include <QMediaMetaData>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QSqlTableModel>
#include <QTabWidget>
#include <QTableView>
#include <QVideoWidget>

#include <functional>

class WasabiDeck final : public QWidget
{
public:
    enum class Action {
        Previous, Play, Pause, Stop, Next, Open, Mute, Shuffle, Repeat,
        FileMenu, PlayMenu, OptionsMenu, ViewMenu, HelpMenu
    };

    explicit WasabiDeck(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("wasabiDeck"));
        setFixedHeight(119);
        setMouseTracking(true);
    }

    void setRuntime(const WasabiRuntime *runtime)
    {
        m_runtime = runtime;
        m_runtimeImages.clear();
        update();
    }

    void setTrack(const QString &title, const QString &details)
    {
        m_title = title;
        m_details = details;
        update();
    }

    void setArtwork(const QImage &artwork) { m_artwork = artwork; update(); }

    void setTiming(qint64 position, qint64 duration)
    {
        m_position = position;
        m_duration = duration;
        update();
    }

    void setPlaying(bool playing)
    {
        m_playing = playing;
        update();
    }

    void setShuffle(bool enabled) { m_shuffle = enabled; update(); }
    void setRepeat(int mode) { m_repeatMode = qBound(0, mode, 2); update(); }

    void setVolume(int volume)
    {
        m_volume = qBound(0, volume, 100);
        update();
    }

    std::function<void(Action)> action;
    std::function<void(int)> volumeChanged;
    std::function<void(qint64)> seekRequested;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::TextAntialiasing);
        painter.fillRect(rect(), QColor(21, 25, 27));

        if (m_runtime && m_runtime->bitmap(QStringLiteral("window.titlebar.grid.left"))) {
            drawRuntimeBitmap(painter, QStringLiteral("window.titlebar.grid.left"),
                              QRect(0, 0, 5, 18));
            drawRuntimeBitmap(painter, QStringLiteral("window.titlebar.grid.middle"),
                              QRect(5, 0, qMax(0, width() - 10), 18));
            drawRuntimeBitmap(painter, QStringLiteral("window.titlebar.grid.right"),
                              QRect(width() - 5, 0, 5, 18));
        } else {
            painter.fillRect(0, 0, width(), 18, QColor(45, 50, 53));
        }
        painter.setPen(QColor(205, 212, 215));
        painter.setFont(QFont(QStringLiteral("Sans Serif"), 7, QFont::Bold));
        painter.drawText(QRect(26, 0, 46, 18), Qt::AlignCenter, QStringLiteral("LLAMAAMP"));
        painter.setFont(QFont(QStringLiteral("Sans Serif"), 6));
        painter.drawText(QRect(76, 0, 230, 18), Qt::AlignVCenter,
                         QStringLiteral("FILE   PLAY   OPTIONS   VIEW   HELP"));
        painter.setFont(QFont(QStringLiteral("Sans Serif"), 7, QFont::Bold));
        painter.drawText(QRect(310, 0, qMax(0, width() - 400), 18), Qt::AlignCenter, m_title);
        if (m_runtime) {
            drawRuntimeBitmap(painter, QStringLiteral("window.titlebar.button.sysmenu.normal"),
                              QRect(5, 2, 15, 13));
            drawRuntimeBitmap(painter, QStringLiteral("window.titlebar.button.minimize.normal"),
                              QRect(width() - 79, 2, 17, 13));
            drawRuntimeBitmap(painter, QStringLiteral("window.titlebar.button.maximize.normal"),
                              QRect(width() - 60, 2, 17, 13));
            drawRuntimeBitmap(painter, QStringLiteral("window.titlebar.button.shade.normal"),
                              QRect(width() - 41, 2, 17, 13));
            drawRuntimeBitmap(painter, QStringLiteral("window.titlebar.button.close.normal"),
                              QRect(width() - 22, 2, 17, 13));
        }

        const QRect display(4, 21, 226, 54);
        if (m_runtime && m_runtime->bitmap(QStringLiteral("player.display.background.left"))) {
            drawRuntimeBitmap(painter, QStringLiteral("player.display.background.left"),
                              QRect(display.x(), display.y(), 80, 54));
            drawRuntimeBitmap(painter, QStringLiteral("player.display.background.center"),
                              QRect(display.x() + 80, display.y(), 14, 54));
            drawRuntimeBitmap(painter, QStringLiteral("player.display.background.right"),
                              QRect(display.right() - 131, display.y(), 132, 54));
        } else {
            painter.fillRect(display, QColor(8, 16, 19));
        }

        painter.setPen(QColor(156, 195, 207));
        painter.setFont(QFont(QStringLiteral("Monospace"), 14, QFont::Bold));
        painter.drawText(QRect(14, 27, 64, 25), Qt::AlignLeft | Qt::AlignVCenter,
                         formatTime(m_position));
        painter.setFont(QFont(QStringLiteral("Sans Serif"), 8, QFont::Bold));
        painter.drawText(QRect(80, 25, 143, 20), Qt::AlignRight | Qt::AlignVCenter,
                         m_title);
        painter.setFont(QFont(QStringLiteral("Sans Serif"), 7));
        painter.drawText(QRect(80, 44, 143, 18), Qt::AlignRight | Qt::AlignVCenter,
                         m_details);

        const int infoX = 238;
        painter.fillRect(QRect(infoX, 21, width() - infoX - 4, 91), QColor(12, 17, 19));
        painter.setPen(QColor(64, 73, 77));
        painter.drawRect(QRect(infoX, 21, width() - infoX - 5, 90));
        const QRect artworkRect(infoX + 8, 28, 64, 64);
        painter.fillRect(artworkRect, QColor(5, 9, 10));
        if (!m_artwork.isNull()) {
            painter.drawImage(artworkRect, m_artwork.scaled(
                artworkRect.size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
        } else if (m_runtime) {
            drawRuntimeBitmap(painter, QStringLiteral("player.button.bolt.normal"),
                              QRect(infoX + 19, 41, 42, 37));
        }
        painter.setPen(QColor(230, 233, 234));
        painter.setFont(QFont(QStringLiteral("Sans Serif"), 10, QFont::Bold));
        painter.drawText(QRect(infoX + 82, 29, width() - infoX - 94, 22),
                         Qt::AlignLeft | Qt::AlignVCenter, m_title);
        painter.setPen(QColor(154, 166, 171));
        painter.setFont(QFont(QStringLiteral("Sans Serif"), 8));
        painter.drawText(QRect(infoX + 82, 53, width() - infoX - 94, 36),
                         Qt::AlignLeft | Qt::AlignVCenter, m_details);

        // player-normal-group.xml: player.layout at (4,21), controls at y=70.
        drawRuntimeBitmap(painter, QStringLiteral("player.button.previous.normal"),
                          QRect(4, 91, 26, 24));
        drawRuntimeBitmap(painter, QStringLiteral("player.button.play.normal"),
                          QRect(29, 91, 22, 24));
        drawRuntimeBitmap(painter, QStringLiteral("player.button.pause.normal"),
                          QRect(50, 91, 22, 24));
        drawRuntimeBitmap(painter, QStringLiteral("player.button.stop.normal"),
                          QRect(71, 91, 22, 24));
        drawRuntimeBitmap(painter, QStringLiteral("player.button.next.normal"),
                          QRect(92, 91, 26, 24));
        drawRuntimeBitmap(painter, QStringLiteral("player.button.eject.normal"),
                          QRect(118, 94, 25, 17));
        drawRuntimeBitmap(painter, QStringLiteral("player.button.shuffle.normal%1")
                                       .arg(m_shuffle ? 1 : 0), QRect(147, 94, 32, 17));
        drawRuntimeBitmap(painter, QStringLiteral("player.button.repeat.normal%1")
                                       .arg(m_repeatMode), QRect(176, 94, 27, 17));
        drawRuntimeBitmap(painter, m_volume == 0 ? QStringLiteral("player.button.demute.normal")
                                                 : QStringLiteral("player.button.mute.normal"),
                          QRect(119, 61, 25, 14));

        painter.setPen(QColor(49, 57, 60));
        painter.drawLine(150, 68, 226, 68);
        painter.setPen(QColor(159, 186, 196));
        painter.drawLine(150, 68, 150 + 76 * m_volume / 100, 68);
        const int volumeThumbX = 150 + (63 * m_volume / 100);
        drawRuntimeBitmap(painter, QStringLiteral("player.volume.thumb.normal"),
                          QRect(volumeThumbX, 64, 17, 10));

        painter.setPen(QColor(49, 57, 60));
        painter.drawLine(8, 83, 227, 83);
        int seekThumbX = 7;
        if (m_duration > 0)
            seekThumbX += int(188 * m_position / m_duration);
        painter.setPen(QColor(159, 186, 196));
        painter.drawLine(8, 83, seekThumbX + 15, 83);
        drawRuntimeBitmap(painter, QStringLiteral("player.posbar.thumb.normal"),
                          QRect(seekThumbX, 79, 31, 10));

        painter.setPen(m_playing ? QColor(160, 205, 216) : QColor(73, 88, 94));
        painter.drawText(QRect(211, 48, 12, 12), Qt::AlignCenter,
                         m_playing ? QStringLiteral("▶") : QStringLiteral("■"));
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        const QPoint point = event->position().toPoint();
        if (point.y() <= 18) {
            if (point.x() >= 76 && point.x() < 108) trigger(Action::FileMenu);
            else if (point.x() < 140 && point.x() >= 108) trigger(Action::PlayMenu);
            else if (point.x() < 190 && point.x() >= 140) trigger(Action::OptionsMenu);
            else if (point.x() < 224 && point.x() >= 190) trigger(Action::ViewMenu);
            else if (point.x() < 260 && point.x() >= 224) trigger(Action::HelpMenu);
            else if (point.x() >= width() - 79 && point.x() < width() - 62)
                window()->showMinimized();
            else if (point.x() >= width() - 60 && point.x() < width() - 43)
                window()->isMaximized() ? window()->showNormal() : window()->showMaximized();
            else if (point.x() >= width() - 22)
                window()->close();
        }
        if (point.y() >= 91 && point.y() <= 115) {
            if (point.x() < 29) trigger(Action::Previous);
            else if (point.x() < 50) trigger(Action::Play);
            else if (point.x() < 71) trigger(Action::Pause);
            else if (point.x() < 92) trigger(Action::Stop);
            else if (point.x() < 118) trigger(Action::Next);
            else if (point.x() >= 118 && point.x() < 143) trigger(Action::Open);
            else if (point.x() >= 147 && point.x() < 179) trigger(Action::Shuffle);
            else if (point.x() >= 179 && point.x() < 206) trigger(Action::Repeat);
        }
        if (point.y() >= 58 && point.y() <= 76 && point.x() >= 119 && point.x() < 145)
            trigger(Action::Mute);
        if (point.y() >= 58 && point.y() <= 77 && point.x() >= 150 && point.x() <= 229) {
            const int volume = qBound(0, (point.x() - 150) * 100 / 76, 100);
            setVolume(volume);
            if (volumeChanged) volumeChanged(volume);
        }
        if (m_duration > 0 && point.y() >= 76 && point.y() <= 91
            && point.x() >= 7 && point.x() <= 229) {
            if (seekRequested)
                seekRequested(m_duration * (point.x() - 7) / 222);
        }
    }

private:
    void drawRuntimeBitmap(QPainter &painter, const QString &id, const QRect &destination)
    {
        if (!m_runtime)
            return;
        const WasabiBitmapDefinition *definition = m_runtime->bitmap(id);
        if (!definition || definition->file.isEmpty() || definition->file.startsWith(QLatin1Char('$')))
            return;
        auto found = m_runtimeImages.find(definition->file);
        if (found == m_runtimeImages.end())
            found = m_runtimeImages.insert(definition->file, QImage(definition->file));
        if (found->isNull())
            return;
        QRect source = definition->sourceRect;
        if (source.width() <= 0) source.setWidth(found->width() - source.x());
        if (source.height() <= 0) source.setHeight(found->height() - source.y());
        painter.drawImage(destination, *found, source);
    }

    void trigger(Action value) { if (action) action(value); }

    static QString formatTime(qint64 milliseconds)
    {
        const qint64 seconds = qMax<qint64>(0, milliseconds / 1000);
        return QStringLiteral("%1:%2").arg(seconds / 60)
            .arg(seconds % 60, 2, 10, QLatin1Char('0'));
    }

    QImage m_artwork;
    const WasabiRuntime *m_runtime = nullptr;
    QHash<QString, QImage> m_runtimeImages;
    QString m_title = QObject::tr("Nothing playing");
    QString m_details;
    qint64 m_position = 0;
    qint64 m_duration = 0;
    int m_volume = 80;
    bool m_playing = false;
    bool m_shuffle = false;
    int m_repeatMode = 0;
};

WasabiPlayerWidget::WasabiPlayerWidget(PlayerController *player, PlaylistModel *playlist,
                                       MediaLibrary *library, QWidget *parent)
    : QWidget(parent), m_player(player), m_playlist(playlist), m_library(library)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);
    m_deck = new WasabiDeck(this);
    root->addWidget(m_deck);

    m_tabs = new QTabWidget(this);
    auto *playlistPage = new QWidget(m_tabs);
    auto *playlistLayout = new QVBoxLayout(playlistPage);
    playlistLayout->setContentsMargins(2, 2, 2, 2);
    playlistLayout->setSpacing(2);
    m_queue = new QListView(playlistPage);
    m_queue->setModel(playlist);
    m_queue->setAlternatingRowColors(true);
    m_queue->setEditTriggers(QAbstractItemView::NoEditTriggers);
    playlistLayout->addWidget(m_queue, 1);
    auto *playlistTools = new QHBoxLayout;
    auto *addButton = new QPushButton(tr("＋ ADD"), playlistPage);
    auto *removeButton = new QPushButton(tr("− REMOVE"), playlistPage);
    auto *clearButton = new QPushButton(tr("CLEAR"), playlistPage);
    playlistTools->addWidget(addButton);
    playlistTools->addWidget(removeButton);
    playlistTools->addWidget(clearButton);
    playlistTools->addStretch();
    playlistLayout->addLayout(playlistTools);
    m_tabs->addTab(playlistPage, tr("PLAYLIST"));

    m_libraryView = new QTableView(m_tabs);
    if (library->model()) {
        m_libraryView->setModel(library->model());
        m_libraryView->hideColumn(MediaLibrary::Id);
        m_libraryView->hideColumn(MediaLibrary::Path);
        m_libraryView->hideColumn(MediaLibrary::Duration);
        m_libraryView->hideColumn(MediaLibrary::Modified);
    }
    m_libraryView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_libraryView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_libraryView->setAlternatingRowColors(true);
    m_libraryView->horizontalHeader()->setStretchLastSection(true);
    m_tabs->addTab(m_libraryView, tr("MEDIA LIBRARY"));

    m_video = new QVideoWidget(m_tabs);
    m_video->setAspectRatioMode(Qt::KeepAspectRatio);
    m_tabs->addTab(m_video, tr("VIDEO"));
    m_visualization = new AudioVisualizationWidget(m_tabs);
    m_tabs->addTab(m_visualization, tr("VISUALIZATION"));
    auto *browser = new QLabel(
        tr("The legacy embedded-browser service is intentionally unavailable."), m_tabs);
    browser->setAlignment(Qt::AlignCenter);
    m_tabs->addTab(browser, tr("BROWSER"));
    root->addWidget(m_tabs, 1);

    m_deck->action = [this](WasabiDeck::Action value) {
        switch (value) {
        case WasabiDeck::Action::Previous: emit previousRequested(); break;
        case WasabiDeck::Action::Play: m_player->play(); break;
        case WasabiDeck::Action::Pause: m_player->pause(); break;
        case WasabiDeck::Action::Stop: m_player->stop(); break;
        case WasabiDeck::Action::Next: emit nextRequested(); break;
        case WasabiDeck::Action::Open: emit openRequested(); break;
        case WasabiDeck::Action::Mute:
            if (m_player->volume() > 0) {
                m_lastAudibleVolume = m_player->volume();
                emit volumeRequested(0);
            } else {
                emit volumeRequested(m_lastAudibleVolume);
            }
            break;
        case WasabiDeck::Action::Shuffle: emit shuffleToggled(!m_shuffleEnabled); break;
        case WasabiDeck::Action::Repeat: emit repeatRequested(); break;
        case WasabiDeck::Action::FileMenu:
            emit toolbarMenuRequested(0, m_deck->mapToGlobal(QPoint(76, 18))); break;
        case WasabiDeck::Action::PlayMenu:
            emit toolbarMenuRequested(1, m_deck->mapToGlobal(QPoint(108, 18))); break;
        case WasabiDeck::Action::OptionsMenu:
            emit toolbarMenuRequested(3, m_deck->mapToGlobal(QPoint(140, 18))); break;
        case WasabiDeck::Action::ViewMenu:
            emit toolbarMenuRequested(4, m_deck->mapToGlobal(QPoint(190, 18))); break;
        case WasabiDeck::Action::HelpMenu:
            emit toolbarMenuRequested(5, m_deck->mapToGlobal(QPoint(224, 18))); break;
        }
    };
    m_deck->volumeChanged = [this](int volume) { emit volumeRequested(volume); };
    m_deck->seekRequested = [this](qint64 position) { m_player->seek(position); };

    connect(m_queue, &QListView::doubleClicked, this,
            [this](const QModelIndex &index) { emit trackActivated(index.row()); });
    connect(m_libraryView, &QTableView::doubleClicked, this, [this](const QModelIndex &index) {
        const QUrl url = m_library->urlAt(index.row());
        if (!url.isEmpty()) emit libraryTrackActivated(url);
    });
    connect(addButton, &QPushButton::clicked, this, &WasabiPlayerWidget::openRequested);
    connect(removeButton, &QPushButton::clicked, this, [this] {
        QList<int> rows;
        for (const QModelIndex &index : m_queue->selectionModel()->selectedRows())
            rows.append(index.row());
        std::sort(rows.begin(), rows.end(), std::greater<>());
        for (int row : rows) m_playlist->removeRow(row);
    });
    connect(clearButton, &QPushButton::clicked, m_playlist, &PlaylistModel::clear);
    setContextMenuPolicy(Qt::CustomContextMenu);
    connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        QMenu menu(this);
        menu.addAction(tr("Return to native interface"), this,
                       &WasabiPlayerWidget::nativeInterfaceRequested);
        menu.exec(mapToGlobal(point));
    });
    connect(m_player, &PlayerController::positionChanged, this,
            [this](qint64 position) { m_deck->setTiming(position, m_player->duration()); });
    connect(m_player, &PlayerController::durationChanged, this,
            [this](qint64 duration) { m_deck->setTiming(m_player->position(), duration); });
    connect(m_player, &PlayerController::metadataChanged, this, &WasabiPlayerWidget::updateTrack);
    connect(m_player, &PlayerController::sourceChanged, this, &WasabiPlayerWidget::updateTrack);
    connect(m_player, &PlayerController::sourceChanged, m_visualization,
            [this] { m_visualization->clear(); });
    connect(m_player, &PlayerController::audioSamplesReady, m_visualization,
            &AudioVisualizationWidget::setAudioSamples);
    connect(m_player, &PlayerController::playbackStateChanged, this,
            [this](QMediaPlayer::PlaybackState state) {
                m_deck->setPlaying(state == QMediaPlayer::PlayingState);
            });
    updateTrack();
}

void WasabiPlayerWidget::setSkin(const LegacySkinInfo &skin)
{
    QString error;
    if (!m_runtime.load(skin.directory, &error)) {
        setToolTip(tr("XML-skin runtime error: %1").arg(error));
        return;
    }
    setToolTip(tr("XML-skin runtime: %1 bitmaps, %2 groups, %3 layouts, %4 actions")
                   .arg(m_runtime.bitmapIds().size()).arg(m_runtime.groupIds().size())
                   .arg(m_runtime.layoutIds().size()).arg(m_runtime.actions().size()));
    m_deck->setRuntime(&m_runtime);
}

void WasabiPlayerWidget::setShuffleEnabled(bool enabled)
{
    m_shuffleEnabled = enabled;
    m_deck->setShuffle(enabled);
}

void WasabiPlayerWidget::setRepeatMode(int mode) { m_deck->setRepeat(mode); }

void WasabiPlayerWidget::setVolume(int volume)
{
    if (volume > 0) m_lastAudibleVolume = volume;
    m_deck->setVolume(volume);
}

void WasabiPlayerWidget::setPage(int page)
{
    if (m_tabs && page >= 0 && page < m_tabs->count())
        m_tabs->setCurrentIndex(page);
}

QVideoWidget *WasabiPlayerWidget::videoOutput() const { return m_video; }

void WasabiPlayerWidget::updateTrack()
{
    const QMediaMetaData metadata = m_player->metaData();
    QString title = metadata.stringValue(QMediaMetaData::Title);
    if (title.isEmpty() && m_player->source().isLocalFile())
        title = QFileInfo(m_player->source().toLocalFile()).completeBaseName();
    if (title.isEmpty())
        title = tr("Nothing playing");
    QStringList details;
    const QString artist = metadata.stringValue(QMediaMetaData::ContributingArtist);
    const QString album = metadata.stringValue(QMediaMetaData::AlbumTitle);
    if (!artist.isEmpty()) details.append(artist);
    if (!album.isEmpty()) details.append(album);
    m_deck->setTrack(title, details.join(QStringLiteral("  •  ")));
    m_deck->setArtwork(metadata.value(QMediaMetaData::CoverArtImage).value<QImage>());
}
