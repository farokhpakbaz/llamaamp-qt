/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <QMainWindow>
#include <QMediaPlayer>
#include <QStringList>
#include <QUrl>

class PlayerController;
class PlaylistModel;
class MediaLibrary;
class WasabiPlayerWidget;
class QCloseEvent;
class QComboBox;
class QLabel;
class QLineEdit;
class QListView;
class QModelIndex;
class QPushButton;
class QSlider;
class QSortFilterProxyModel;
class QStackedWidget;
class QTabWidget;
class QTableView;
class QVideoWidget;

class MainWindow final : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    void addPaths(const QStringList &paths, bool playFirst = false);
    bool setAudioOutput(const QString &deviceName);
    void setDspEnabled(bool enabled);
    void setEqualizerPreset(const QString &preset);
    bool setSkin(const QString &name);

protected:
    void closeEvent(QCloseEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    enum class RepeatMode { Off, All, One };

    void buildUi();
    void buildMenus();
    void connectSignals();
    void applyStyle();
    void restoreState();
    void saveState() const;
    void openFiles();
    void openFolder();
    void addUrls(const QList<QUrl> &urls, bool playFirst = false);
    void addDirectory(const QString &path);
    void savePlaylist();
    void removeSelected();
    void clearPlaylist();
    QList<int> selectedSourceRows() const;
    void playCurrent();
    void playSourceRow(int row);
    void playViewIndex(const QModelIndex &index);
    void togglePlayback();
    void previousTrack();
    void nextTrack(bool automatic = false);
    int nextRow(bool automatic);
    void resetShuffleCycle();
    void cycleRepeatMode();
    void updateRepeatButton();
    void refreshAudioOutputs();
    void selectAudioOutput(int index);
    void updateNowPlaying();
    void updateArtwork();
    void updatePosition(qint64 position);
    void updateDuration(qint64 duration);
    void updatePlaybackState(QMediaPlayer::PlaybackState state);
    void updateQueueSummary();
    void showEqualizer();
    void showSkinBrowser();
    static QString formatTime(qint64 milliseconds);

    PlayerController *m_player = nullptr;
    PlaylistModel *m_playlistModel = nullptr;
    MediaLibrary *m_mediaLibrary = nullptr;
    QSortFilterProxyModel *m_filterModel = nullptr;
    QSortFilterProxyModel *m_libraryFilterModel = nullptr;
    QListView *m_playlistView = nullptr;
    QTableView *m_libraryView = nullptr;
    QTabWidget *m_contentTabs = nullptr;
    QLineEdit *m_searchEdit = nullptr;
    QLabel *m_artworkLabel = nullptr;
    QLabel *m_titleLabel = nullptr;
    QLabel *m_detailsLabel = nullptr;
    QLabel *m_locationLabel = nullptr;
    QLabel *m_elapsedLabel = nullptr;
    QLabel *m_durationLabel = nullptr;
    QLabel *m_queueSummaryLabel = nullptr;
    QLabel *m_resultCountLabel = nullptr;
    QSlider *m_seekSlider = nullptr;
    QSlider *m_volumeSlider = nullptr;
    QComboBox *m_audioDeviceCombo = nullptr;
    QComboBox *m_skinCombo = nullptr;
    QVideoWidget *m_videoWidget = nullptr;
    QStackedWidget *m_centralStack = nullptr;
    WasabiPlayerWidget *m_wasabiWidget = nullptr;
    QPushButton *m_playButton = nullptr;
    QPushButton *m_shuffleButton = nullptr;
    QPushButton *m_repeatButton = nullptr;
    RepeatMode m_repeatMode = RepeatMode::Off;
    QList<int> m_shuffleRemaining;
    bool m_shuffleCycleActive = false;
    bool m_userSeeking = false;
};
