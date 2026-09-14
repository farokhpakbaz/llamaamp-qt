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
class QLabel;
class QListView;
class QTableView;
class QTabWidget;
class QVideoWidget;
class AudioVisualizationWidget;
class ClassicEqualizerWidget;
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
    void useBuiltInSkin();
    void setWindowShaded(bool shaded);
    bool isWindowShaded() const;
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
    void windowShadeChanged(bool shaded);

private:
    void updateTrack();

    PlayerController *m_player = nullptr;
    PlaylistModel *m_playlist = nullptr;
    MediaLibrary *m_library = nullptr;
    WasabiDeck *m_deck = nullptr;
    QLabel *m_pageTitle = nullptr;
    QListView *m_queue = nullptr;
    QTableView *m_libraryView = nullptr;
    QTabWidget *m_tabs = nullptr;
    QVideoWidget *m_video = nullptr;
    AudioVisualizationWidget *m_visualization = nullptr;
    ClassicEqualizerWidget *m_equalizer = nullptr;
    WasabiRuntime m_runtime;
    bool m_shuffleEnabled = false;
    int m_lastAudibleVolume = 80;
    bool m_windowShaded = false;
};
