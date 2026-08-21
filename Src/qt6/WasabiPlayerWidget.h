/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "SkinManager.h"
#include "WasabiRuntime.h"

#include <QUrl>
#include <QWidget>

class PlayerController;
class PlaylistModel;
class MediaLibrary;
class QListView;
class QTableView;
class QTabWidget;
class QVideoWidget;
class AudioVisualizationWidget;
class WasabiDeck;

class WasabiPlayerWidget final : public QWidget
{
    Q_OBJECT

public:
    explicit WasabiPlayerWidget(PlayerController *player, PlaylistModel *playlist,
                                MediaLibrary *library,
                                QWidget *parent = nullptr);
    void setSkin(const LegacySkinInfo &skin);
    void setShuffleEnabled(bool enabled);
    void setRepeatMode(int mode);
    void setVolume(int volume);
    void setPage(int page);
    QVideoWidget *videoOutput() const;

signals:
    void previousRequested();
    void nextRequested();
    void openRequested();
    void trackActivated(int sourceRow);
    void libraryTrackActivated(const QUrl &url);
    void nativeInterfaceRequested();
    void shuffleToggled(bool enabled);
    void repeatRequested();
    void volumeRequested(int volume);
    void toolbarMenuRequested(int menuIndex, const QPoint &globalPosition);

private:
    void updateTrack();

    PlayerController *m_player = nullptr;
    PlaylistModel *m_playlist = nullptr;
    MediaLibrary *m_library = nullptr;
    WasabiDeck *m_deck = nullptr;
    QListView *m_queue = nullptr;
    QTableView *m_libraryView = nullptr;
    QTabWidget *m_tabs = nullptr;
    QVideoWidget *m_video = nullptr;
    AudioVisualizationWidget *m_visualization = nullptr;
    WasabiRuntime m_runtime;
    bool m_shuffleEnabled = false;
    int m_lastAudibleVolume = 80;
};
