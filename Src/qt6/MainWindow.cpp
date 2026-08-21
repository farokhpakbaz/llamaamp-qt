/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "MainWindow.h"

#include "PlayerController.h"
#include "PlaylistModel.h"
#include "MediaLibrary.h"
#include "SkinManager.h"
#include "WasabiPlayerWidget.h"
#include "PluginManager.h"
#include "Equalizer.h"

#include <QApplication>
#include <QAudioDevice>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QCheckBox>
#include <QComboBox>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDirIterator>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFrame>
#include <QImage>
#include <QHeaderView>
#include <QItemSelectionModel>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QListWidget>
#include <QMediaMetaData>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QSignalBlocker>
#include <QSlider>
#include <QSortFilterProxyModel>
#include <QStackedWidget>
#include <QSqlTableModel>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QTableView>
#include <QVideoWidget>

#include <algorithm>
#include <climits>

namespace {
constexpr auto kAudioFilter =
    "Media files (*.mp3 *.ogg *.oga *.opus *.flac *.wav *.m4a *.aac *.wma *.aiff *.aif "
    "*.mp4 *.mkv *.webm *.avi *.mov);;"
    "Playlists (*.m3u *.m3u8);;All files (*)";
const QStringList kAudioExtensions = {
    QStringLiteral("mp3"), QStringLiteral("ogg"), QStringLiteral("oga"),
    QStringLiteral("opus"), QStringLiteral("flac"), QStringLiteral("wav"),
    QStringLiteral("m4a"), QStringLiteral("aac"), QStringLiteral("wma"),
    QStringLiteral("aiff"), QStringLiteral("aif"), QStringLiteral("mp4"),
    QStringLiteral("mkv"), QStringLiteral("webm"), QStringLiteral("avi"),
    QStringLiteral("mov")
};
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_player(new PlayerController(this))
    , m_playlistModel(new PlaylistModel(this))
    , m_mediaLibrary(new MediaLibrary({}, this))
    , m_filterModel(new QSortFilterProxyModel(this))
    , m_libraryFilterModel(new QSortFilterProxyModel(this))
{
    m_filterModel->setSourceModel(m_playlistModel);
    m_filterModel->setFilterRole(Qt::DisplayRole);
    m_filterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    if (m_mediaLibrary->model()) {
        m_libraryFilterModel->setSourceModel(m_mediaLibrary->model());
        m_libraryFilterModel->setFilterKeyColumn(-1);
        m_libraryFilterModel->setFilterCaseSensitivity(Qt::CaseInsensitive);
    }
    setAcceptDrops(true);
    setMinimumSize(760, 560);
    resize(1040, 700);
    setWindowTitle(QStringLiteral("LlamaAmp Qt"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/llamaamp.svg")));
    buildMenus();
    buildUi();
    connectSignals();
    applyStyle();
    restoreState();
    updateNowPlaying();
    updateQueueSummary();
}

bool MainWindow::setSkin(const QString &name)
{
    if (!m_skinCombo)
        return false;
    int index = m_skinCombo->findText(name, Qt::MatchFixedString);
    QString legacyName = name;
    legacyName.remove(QRegularExpression(
        QStringLiteral(R"(^\s*(?:XML|Wasabi)(?:\s+partial)?\s*[·:-]?\s*)"),
        QRegularExpression::CaseInsensitiveOption));
    if (index < 0) {
        for (int candidate = 0; candidate < m_skinCombo->count(); ++candidate) {
            const QString label = m_skinCombo->itemText(candidate);
            if (label.startsWith(QStringLiteral("XML"))
                && label.contains(legacyName, Qt::CaseInsensitive)) {
                index = candidate;
                break;
            }
        }
    }
    if (index < 0)
        return false;
    m_skinCombo->setCurrentIndex(index);
    return true;
}

void MainWindow::buildMenus()
{
    auto *fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("Open &files…"), QKeySequence::Open, this, &MainWindow::openFiles);
    fileMenu->addAction(tr("Open &folder…"),
                        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O),
                        this, &MainWindow::openFolder);
    fileMenu->addAction(tr("Save playlist…"), QKeySequence::SaveAs,
                        this, &MainWindow::savePlaylist);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("&Quit"), QKeySequence::Quit, qApp, &QApplication::quit);

    auto *playbackMenu = menuBar()->addMenu(tr("&Playback"));
    playbackMenu->addAction(tr("Play/Pause"), QKeySequence(Qt::Key_Space),
                            this, &MainWindow::togglePlayback);
    playbackMenu->addAction(tr("Previous"), QKeySequence(Qt::CTRL | Qt::Key_Left),
                            this, &MainWindow::previousTrack);
    playbackMenu->addAction(tr("Next"), QKeySequence(Qt::CTRL | Qt::Key_Right),
                            this, [this] { nextTrack(false); });
    playbackMenu->addAction(tr("Video full screen"), QKeySequence(Qt::Key_F11), this, [this] {
        if (m_videoWidget && m_videoWidget->isVisible())
            m_videoWidget->setFullScreen(!m_videoWidget->isFullScreen());
    });

    auto *queueMenu = menuBar()->addMenu(tr("&Queue"));
    queueMenu->addAction(tr("Remove selected"), QKeySequence::Delete,
                         this, &MainWindow::removeSelected);
    queueMenu->addAction(tr("Clear queue"), this, &MainWindow::clearPlaylist);

    auto *toolsMenu = menuBar()->addMenu(tr("&Tools"));
    toolsMenu->addAction(tr("Equalizer…"), QKeySequence(Qt::CTRL | Qt::Key_E),
                         this, &MainWindow::showEqualizer);
    toolsMenu->addAction(tr("Plug-ins…"), this, [this] {
        QMessageBox::information(this, tr("LlamaAmp Qt Plug-ins"),
                                 m_player->pluginManager()->report());
    });
    toolsMenu->addAction(tr("XML skin browser…"), this, &MainWindow::showSkinBrowser);
    toolsMenu->addAction(tr("Remove missing library files"), this, [this] {
        const int removed = m_mediaLibrary->removeMissing();
        statusBar()->showMessage(tr("Removed %n missing file(s) from the library", nullptr, removed),
                                 4000);
    });

    auto *viewMenu = menuBar()->addMenu(tr("&View"));
    viewMenu->addAction(tr("Playlist"), QKeySequence(Qt::CTRL | Qt::Key_1), this, [this] {
        if (m_centralStack && m_wasabiWidget
            && m_centralStack->currentWidget() == m_wasabiWidget) {
            m_wasabiWidget->setPage(0);
        } else if (m_contentTabs) {
            m_contentTabs->setCurrentIndex(0);
        }
    });
    viewMenu->addAction(tr("Media Library"), QKeySequence(Qt::CTRL | Qt::Key_2), this, [this] {
        if (m_centralStack && m_wasabiWidget
            && m_centralStack->currentWidget() == m_wasabiWidget) {
            m_wasabiWidget->setPage(1);
        } else if (m_contentTabs) {
            m_contentTabs->setCurrentIndex(1);
        }
    });
    viewMenu->addAction(tr("Video"), QKeySequence(Qt::CTRL | Qt::Key_3), this, [this] {
        if (m_wasabiWidget && m_centralStack
            && m_centralStack->currentWidget() == m_wasabiWidget)
            m_wasabiWidget->setPage(2);
    });
    viewMenu->addAction(tr("Visualization"), QKeySequence(Qt::CTRL | Qt::Key_4), this,
                        [this] {
        if (m_wasabiWidget && m_centralStack
            && m_centralStack->currentWidget() == m_wasabiWidget)
            m_wasabiWidget->setPage(3);
    });
    viewMenu->addAction(tr("Browser"), QKeySequence(Qt::CTRL | Qt::Key_5), this, [this] {
        if (m_wasabiWidget && m_centralStack
            && m_centralStack->currentWidget() == m_wasabiWidget)
            m_wasabiWidget->setPage(4);
    });

    auto *helpMenu = menuBar()->addMenu(tr("&Help"));
    helpMenu->addAction(tr("XML-skin compatibility report"), this, [this] {
        QMessageBox::information(this, tr("XML-skin compatibility"),
                                 SkinManager::compatibilityReport());
    });
    helpMenu->addAction(tr("About LlamaAmp Qt"), this, [this] {
        QMessageBox::about(this, tr("About LlamaAmp Qt"),
                           tr("LlamaAmp Qt %1\nNative Qt 6 player for Linux.")
                               .arg(QApplication::applicationVersion()));
    });
}

void MainWindow::buildUi()
{
    auto *central = new QWidget(this);
    auto *shell = new QHBoxLayout(central);
    shell->setContentsMargins(0, 0, 0, 0);
    shell->setSpacing(0);

    auto *sidebar = new QFrame(central);
    sidebar->setObjectName(QStringLiteral("sidebar"));
    sidebar->setFixedWidth(210);
    auto *sideLayout = new QVBoxLayout(sidebar);
    sideLayout->setContentsMargins(18, 22, 18, 18);
    sideLayout->setSpacing(10);
    auto *brandRow = new QHBoxLayout;
    auto *brandIcon = new QLabel(sidebar);
    brandIcon->setPixmap(QIcon(QStringLiteral(":/icons/llamaamp.svg")).pixmap(38, 38));
    auto *brand = new QLabel(QStringLiteral("LLAMAAMP"), sidebar);
    brand->setObjectName(QStringLiteral("brand"));
    brandRow->addWidget(brandIcon);
    brandRow->addWidget(brand);
    brandRow->addStretch();
    sideLayout->addLayout(brandRow);

    auto *libraryLabel = new QLabel(tr("YOUR MUSIC"), sidebar);
    libraryLabel->setObjectName(QStringLiteral("sideHeading"));
    sideLayout->addSpacing(16);
    sideLayout->addWidget(libraryLabel);
    auto *openFilesButton = new QPushButton(tr("＋  Add files"), sidebar);
    auto *openFolderButton = new QPushButton(tr("▣  Add folder"), sidebar);
    auto *saveButton = new QPushButton(tr("⇩  Save playlist"), sidebar);
    for (QPushButton *button : {openFilesButton, openFolderButton, saveButton})
        button->setObjectName(QStringLiteral("sideButton"));
    sideLayout->addWidget(openFilesButton);
    sideLayout->addWidget(openFolderButton);
    sideLayout->addWidget(saveButton);

    m_queueSummaryLabel = new QLabel(sidebar);
    m_queueSummaryLabel->setObjectName(QStringLiteral("queueSummary"));
    m_queueSummaryLabel->setWordWrap(true);
    sideLayout->addSpacing(16);
    sideLayout->addWidget(m_queueSummaryLabel);
    sideLayout->addStretch();

    auto *outputHeading = new QLabel(tr("AUDIO OUTPUT"), sidebar);
    outputHeading->setObjectName(QStringLiteral("sideHeading"));
    m_audioDeviceCombo = new QComboBox(sidebar);
    m_audioDeviceCombo->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_audioDeviceCombo->setToolTip(tr("Audio output device"));
    auto *volumeRow = new QHBoxLayout;
    volumeRow->addWidget(new QLabel(QStringLiteral("🔊"), sidebar));
    m_volumeSlider = new QSlider(Qt::Horizontal, sidebar);
    m_volumeSlider->setRange(0, 100);
    volumeRow->addWidget(m_volumeSlider);
    sideLayout->addWidget(outputHeading);
    sideLayout->addWidget(m_audioDeviceCombo);
    sideLayout->addLayout(volumeRow);
    auto *skinHeading = new QLabel(tr("SKIN"), sidebar);
    skinHeading->setObjectName(QStringLiteral("sideHeading"));
    m_skinCombo = new QComboBox(sidebar);
    m_skinCombo->addItems(SkinManager::availableSkins());
    for (const LegacySkinInfo &skin : SkinManager::legacySkinCatalog()) {
        if (SkinManager::canRenderWasabiSkin(skin))
            m_skinCombo->addItem(tr("XML partial · %1").arg(skin.name), skin.directory);
    }
    sideLayout->addSpacing(8);
    sideLayout->addWidget(skinHeading);
    sideLayout->addWidget(m_skinCombo);
    shell->addWidget(sidebar);

    auto *content = new QWidget(central);
    auto *root = new QVBoxLayout(content);
    root->setContentsMargins(24, 22, 24, 18);
    root->setSpacing(14);
    auto *nowPlaying = new QFrame(content);
    nowPlaying->setObjectName(QStringLiteral("nowPlaying"));
    auto *nowLayout = new QHBoxLayout(nowPlaying);
    nowLayout->setContentsMargins(16, 16, 20, 16);
    nowLayout->setSpacing(18);
    m_artworkLabel = new QLabel(nowPlaying);
    m_artworkLabel->setObjectName(QStringLiteral("artwork"));
    m_artworkLabel->setFixedSize(118, 118);
    m_artworkLabel->setAlignment(Qt::AlignCenter);
    auto *metaLayout = new QVBoxLayout;
    auto *eyebrow = new QLabel(tr("NOW PLAYING"), nowPlaying);
    eyebrow->setObjectName(QStringLiteral("eyebrow"));
    m_titleLabel = new QLabel(nowPlaying);
    m_titleLabel->setObjectName(QStringLiteral("trackTitle"));
    m_titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_detailsLabel = new QLabel(nowPlaying);
    m_detailsLabel->setObjectName(QStringLiteral("trackDetails"));
    m_locationLabel = new QLabel(nowPlaying);
    m_locationLabel->setObjectName(QStringLiteral("trackLocation"));
    m_locationLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_locationLabel->setWordWrap(true);
    metaLayout->addWidget(eyebrow);
    metaLayout->addStretch();
    metaLayout->addWidget(m_titleLabel);
    metaLayout->addWidget(m_detailsLabel);
    metaLayout->addWidget(m_locationLabel);
    metaLayout->addStretch();
    nowLayout->addWidget(m_artworkLabel);
    nowLayout->addLayout(metaLayout, 1);
    root->addWidget(nowPlaying);

    m_videoWidget = new QVideoWidget(content);
    m_videoWidget->setObjectName(QStringLiteral("videoOutput"));
    m_videoWidget->setMinimumHeight(260);
    m_videoWidget->setAspectRatioMode(Qt::KeepAspectRatio);
    m_videoWidget->hide();
    m_player->setVideoOutput(m_videoWidget);
    root->addWidget(m_videoWidget, 1);

    auto *queueHeader = new QHBoxLayout;
    auto *queueTitle = new QLabel(tr("MUSIC"), content);
    queueTitle->setObjectName(QStringLiteral("sectionTitle"));
    m_searchEdit = new QLineEdit(content);
    m_searchEdit->setPlaceholderText(tr("Search queue…"));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMaximumWidth(280);
    m_resultCountLabel = new QLabel(content);
    auto *removeButton = new QPushButton(tr("Remove"), content);
    auto *clearButton = new QPushButton(tr("Clear"), content);
    queueHeader->addWidget(queueTitle);
    queueHeader->addWidget(m_resultCountLabel);
    queueHeader->addStretch();
    queueHeader->addWidget(m_searchEdit);
    queueHeader->addWidget(removeButton);
    queueHeader->addWidget(clearButton);
    root->addLayout(queueHeader);

    m_playlistView = new QListView(content);
    m_playlistView->setModel(m_filterModel);
    m_playlistView->setAlternatingRowColors(true);
    m_playlistView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_playlistView->setDragDropMode(QAbstractItemView::InternalMove);
    m_playlistView->setDefaultDropAction(Qt::MoveAction);
    m_playlistView->setDropIndicatorShown(true);
    m_playlistView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_playlistView->setContextMenuPolicy(Qt::CustomContextMenu);
    auto *queuePage = new QWidget(content);
    auto *queuePageLayout = new QVBoxLayout(queuePage);
    queuePageLayout->setContentsMargins(0, 8, 0, 0);
    queuePageLayout->addWidget(m_playlistView);

    m_libraryView = new QTableView(content);
    m_libraryView->setModel(m_libraryFilterModel);
    m_libraryView->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_libraryView->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_libraryView->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_libraryView->setAlternatingRowColors(true);
    m_libraryView->setContextMenuPolicy(Qt::CustomContextMenu);
    m_libraryView->setSortingEnabled(true);
    m_libraryView->verticalHeader()->hide();
    m_libraryView->horizontalHeader()->setStretchLastSection(true);
    m_libraryView->hideColumn(MediaLibrary::Id);
    m_libraryView->hideColumn(MediaLibrary::Path);
    m_libraryView->hideColumn(MediaLibrary::Duration);
    m_libraryView->hideColumn(MediaLibrary::Modified);
    auto *libraryPage = new QWidget(content);
    auto *libraryPageLayout = new QVBoxLayout(libraryPage);
    libraryPageLayout->setContentsMargins(0, 8, 0, 0);
    libraryPageLayout->addWidget(m_libraryView);

    m_contentTabs = new QTabWidget(content);
    m_contentTabs->addTab(queuePage, tr("Queue"));
    m_contentTabs->addTab(libraryPage, tr("Media Library"));
    root->addWidget(m_contentTabs, 1);

    auto *transport = new QFrame(content);
    transport->setObjectName(QStringLiteral("transport"));
    auto *transportRoot = new QVBoxLayout(transport);
    transportRoot->setContentsMargins(14, 10, 14, 10);
    auto *seekLayout = new QHBoxLayout;
    m_elapsedLabel = new QLabel(QStringLiteral("0:00"), transport);
    m_durationLabel = new QLabel(QStringLiteral("0:00"), transport);
    m_seekSlider = new QSlider(Qt::Horizontal, transport);
    m_seekSlider->setRange(0, 0);
    seekLayout->addWidget(m_elapsedLabel);
    seekLayout->addWidget(m_seekSlider, 1);
    seekLayout->addWidget(m_durationLabel);
    transportRoot->addLayout(seekLayout);
    auto *controls = new QHBoxLayout;
    auto makeButton = [this, transport](QStyle::StandardPixmap icon, const QString &tip) {
        auto *button = new QPushButton(transport);
        button->setIcon(style()->standardIcon(icon));
        button->setToolTip(tip);
        button->setFixedSize(44, 36);
        return button;
    };
    auto *previousButton = makeButton(QStyle::SP_MediaSkipBackward, tr("Previous"));
    m_playButton = makeButton(QStyle::SP_MediaPlay, tr("Play/Pause (Space)"));
    m_playButton->setObjectName(QStringLiteral("primaryPlay"));
    auto *stopButton = makeButton(QStyle::SP_MediaStop, tr("Stop"));
    auto *nextButton = makeButton(QStyle::SP_MediaSkipForward, tr("Next"));
    m_shuffleButton = new QPushButton(tr("Shuffle"), transport);
    m_shuffleButton->setCheckable(true);
    m_repeatButton = new QPushButton(transport);
    m_repeatButton->setFixedWidth(92);
    controls->addStretch();
    controls->addWidget(m_shuffleButton);
    controls->addSpacing(12);
    controls->addWidget(previousButton);
    controls->addWidget(m_playButton);
    controls->addWidget(stopButton);
    controls->addWidget(nextButton);
    controls->addSpacing(12);
    controls->addWidget(m_repeatButton);
    controls->addStretch();
    transportRoot->addLayout(controls);
    root->addWidget(transport);
    shell->addWidget(content, 1);
    m_centralStack = new QStackedWidget(this);
    m_centralStack->addWidget(central);
    m_wasabiWidget = new WasabiPlayerWidget(m_player, m_playlistModel, m_mediaLibrary,
                                            m_centralStack);
    m_centralStack->addWidget(m_wasabiWidget);
    setCentralWidget(m_centralStack);
    connect(m_wasabiWidget, &WasabiPlayerWidget::previousRequested,
            this, &MainWindow::previousTrack);
    connect(m_wasabiWidget, &WasabiPlayerWidget::nextRequested,
            this, [this] { nextTrack(false); });
    connect(m_wasabiWidget, &WasabiPlayerWidget::openRequested,
            this, &MainWindow::openFiles);
    connect(m_wasabiWidget, &WasabiPlayerWidget::trackActivated,
            this, &MainWindow::playSourceRow);
    connect(m_wasabiWidget, &WasabiPlayerWidget::libraryTrackActivated,
            this, [this](const QUrl &url) {
                int row = m_playlistModel->findUrl(url);
                if (row < 0) {
                    m_playlistModel->addUrl(url);
                    row = m_playlistModel->findUrl(url);
                }
                playSourceRow(row);
            });
    connect(m_wasabiWidget, &WasabiPlayerWidget::shuffleToggled,
            m_shuffleButton, &QPushButton::setChecked);
    connect(m_wasabiWidget, &WasabiPlayerWidget::repeatRequested,
            this, &MainWindow::cycleRepeatMode);
    connect(m_wasabiWidget, &WasabiPlayerWidget::volumeRequested,
            m_volumeSlider, &QSlider::setValue);
    connect(m_wasabiWidget, &WasabiPlayerWidget::toolbarMenuRequested,
            this, [this](int menuIndex, const QPoint &position) {
                const QList<QAction *> menus = menuBar()->actions();
                if (menuIndex >= 0 && menuIndex < menus.size() && menus.at(menuIndex)->menu())
                    menus.at(menuIndex)->menu()->popup(position);
            });
    connect(m_wasabiWidget, &WasabiPlayerWidget::nativeInterfaceRequested,
            this, [this] { m_skinCombo->setCurrentIndex(0); });
    statusBar()->showMessage(tr("Ready"));

    connect(openFilesButton, &QPushButton::clicked, this, &MainWindow::openFiles);
    connect(openFolderButton, &QPushButton::clicked, this, &MainWindow::openFolder);
    connect(saveButton, &QPushButton::clicked, this, &MainWindow::savePlaylist);
    connect(removeButton, &QPushButton::clicked, this, &MainWindow::removeSelected);
    connect(clearButton, &QPushButton::clicked, this, &MainWindow::clearPlaylist);
    connect(previousButton, &QPushButton::clicked, this, &MainWindow::previousTrack);
    connect(m_playButton, &QPushButton::clicked, this, &MainWindow::togglePlayback);
    connect(stopButton, &QPushButton::clicked, m_player, &PlayerController::stop);
    connect(nextButton, &QPushButton::clicked, this, [this] { nextTrack(false); });
    connect(m_repeatButton, &QPushButton::clicked, this, &MainWindow::cycleRepeatMode);
    connect(m_shuffleButton, &QPushButton::toggled, this, [this] { resetShuffleCycle(); });
    connect(m_shuffleButton, &QPushButton::toggled,
            m_wasabiWidget, &WasabiPlayerWidget::setShuffleEnabled);
    connect(m_volumeSlider, &QSlider::valueChanged,
            m_wasabiWidget, &WasabiPlayerWidget::setVolume);
}

void MainWindow::connectSignals()
{
    connect(m_playlistView, &QListView::doubleClicked, this, &MainWindow::playViewIndex);
    connect(m_searchEdit, &QLineEdit::textChanged, this, [this](const QString &text) {
        const QRegularExpression expression(QRegularExpression::escape(text),
                                            QRegularExpression::CaseInsensitiveOption);
        m_filterModel->setFilterRegularExpression(expression);
        m_libraryFilterModel->setFilterRegularExpression(expression);
        updateQueueSummary();
    });
    connect(m_contentTabs, &QTabWidget::currentChanged, this, [this](int index) {
        m_searchEdit->setPlaceholderText(index == 0 ? tr("Search queue…") : tr("Search library…"));
        updateQueueSummary();
    });
    connect(m_libraryView, &QTableView::doubleClicked, this, [this](const QModelIndex &viewIndex) {
        const QModelIndex sourceIndex = m_libraryFilterModel->mapToSource(viewIndex);
        const QUrl url = m_mediaLibrary->urlAt(sourceIndex.row());
        if (!url.isEmpty()) {
            int row = m_playlistModel->findUrl(url);
            if (row < 0) {
                m_playlistModel->addUrl(url);
                row = m_playlistModel->rowCount() - 1;
            }
            playSourceRow(row);
        }
    });
    connect(m_playlistModel, &QAbstractItemModel::rowsInserted, this, &MainWindow::updateQueueSummary);
    connect(m_playlistModel, &QAbstractItemModel::rowsRemoved, this, &MainWindow::updateQueueSummary);
    connect(m_playlistModel, &QAbstractItemModel::modelReset, this, &MainWindow::updateQueueSummary);
    connect(m_playlistModel, &QAbstractItemModel::rowsInserted, this,
            [this] { resetShuffleCycle(); });
    connect(m_playlistModel, &QAbstractItemModel::rowsRemoved, this,
            [this] { resetShuffleCycle(); });
    connect(m_playlistModel, &QAbstractItemModel::modelReset, this,
            [this] { resetShuffleCycle(); });
    if (m_mediaLibrary->model()) {
        connect(m_mediaLibrary->model(), &QAbstractItemModel::modelReset,
                this, &MainWindow::updateQueueSummary);
        connect(m_mediaLibrary->model(), &QAbstractItemModel::rowsInserted,
                this, &MainWindow::updateQueueSummary);
        connect(m_mediaLibrary->model(), &QAbstractItemModel::rowsRemoved,
                this, &MainWindow::updateQueueSummary);
    }
    connect(m_player, &PlayerController::positionChanged, this, &MainWindow::updatePosition);
    connect(m_player, &PlayerController::durationChanged, this, &MainWindow::updateDuration);
    connect(m_player, &PlayerController::playbackStateChanged,
            this, &MainWindow::updatePlaybackState);
    connect(m_player, &PlayerController::metadataChanged, this, &MainWindow::updateNowPlaying);
    connect(m_player, &PlayerController::sourceChanged, this, [this](const QUrl &source) {
        m_playlistModel->setCurrentRow(m_playlistModel->findUrl(source));
        updateNowPlaying();
    });
    connect(m_player, &PlayerController::endOfMedia, this, [this] { nextTrack(true); });
    connect(m_player, &PlayerController::errorOccurred, this, [this](const QString &message) {
        statusBar()->showMessage(tr("Playback error: %1").arg(message), 8000);
    });
    connect(m_player, &PlayerController::audioOutputsChanged, this, &MainWindow::refreshAudioOutputs);
    connect(m_player, &PlayerController::hasVideoChanged, this, [this](bool available) {
        m_videoWidget->setVisible(available);
        statusBar()->showMessage(available ? tr("Video stream detected") : tr("Audio playback"), 2500);
    });
    connect(m_videoWidget, &QVideoWidget::fullScreenChanged, this, [this](bool fullScreen) {
        if (!fullScreen)
            m_videoWidget->show();
    });
    connect(m_seekSlider, &QSlider::sliderPressed, this, [this] { m_userSeeking = true; });
    connect(m_seekSlider, &QSlider::sliderReleased, this, [this] {
        m_userSeeking = false;
        m_player->seek(m_seekSlider->value());
    });
    connect(m_seekSlider, &QSlider::sliderMoved, this,
            [this](int value) { m_elapsedLabel->setText(formatTime(value)); });
    connect(m_volumeSlider, &QSlider::valueChanged, m_player, &PlayerController::setVolume);
    connect(m_audioDeviceCombo, &QComboBox::currentIndexChanged,
            this, &MainWindow::selectAudioOutput);
    connect(m_skinCombo, &QComboBox::currentTextChanged, this, [this] { applyStyle(); });
    connect(m_playlistView, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        QMenu menu(this);
        menu.addAction(tr("Play"), this, &MainWindow::playCurrent);
        menu.addAction(tr("Remove selected"), this, &MainWindow::removeSelected);
        const QModelIndex viewIndex = m_playlistView->indexAt(point);
        if (viewIndex.isValid()) {
            const QUrl url = m_playlistModel->urlAt(m_filterModel->mapToSource(viewIndex).row());
            if (url.isLocalFile()) {
                menu.addAction(tr("Show in file manager"), this, [url] {
                    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(url.toLocalFile()).absolutePath()));
                });
            }
        }
        menu.exec(m_playlistView->viewport()->mapToGlobal(point));
    });
    connect(m_libraryView, &QWidget::customContextMenuRequested, this, [this](const QPoint &point) {
        QMenu menu(this);
        const QModelIndex viewIndex = m_libraryView->indexAt(point);
        if (viewIndex.isValid()) {
            const QModelIndex sourceIndex = m_libraryFilterModel->mapToSource(viewIndex);
            const QUrl url = m_mediaLibrary->urlAt(sourceIndex.row());
            menu.addAction(tr("Add to queue"), this, [this, url] { m_playlistModel->addUrl(url); });
            menu.addAction(tr("Show in file manager"), this, [url] {
                QDesktopServices::openUrl(
                    QUrl::fromLocalFile(QFileInfo(url.toLocalFile()).absolutePath()));
            });
            menu.addSeparator();
        }
        menu.addAction(tr("Remove selected from library"), this, &MainWindow::removeSelected);
        menu.exec(m_libraryView->viewport()->mapToGlobal(point));
    });
}

void MainWindow::addPaths(const QStringList &paths, bool playFirst)
{
    const int firstAdded = m_playlistModel->rowCount();
    QList<QUrl> directUrls;
    for (const QString &path : paths) {
        const QUrl candidate(path);
        if (candidate.isValid() && !candidate.scheme().isEmpty() && !candidate.isLocalFile()) {
            directUrls.append(candidate);
            continue;
        }
        const QFileInfo info(path);
        if (info.isDir()) {
            addDirectory(info.absoluteFilePath());
        } else if (info.isFile() && PlaylistModel::isPlaylistFile(info.absoluteFilePath())) {
            QString error;
            if (!m_playlistModel->loadM3u(info.absoluteFilePath(), &error))
                statusBar()->showMessage(tr("Could not open playlist: %1").arg(error), 6000);
        } else if (info.isFile()) {
            directUrls.append(QUrl::fromLocalFile(info.absoluteFilePath()));
        }
    }
    m_playlistModel->addUrls(directUrls);
    m_mediaLibrary->addUrls(directUrls);
    m_mediaLibrary->addUrls(m_playlistModel->urls());
    if (playFirst) {
        if (firstAdded < m_playlistModel->rowCount())
            playSourceRow(firstAdded);
        else if (!directUrls.isEmpty())
            playSourceRow(m_playlistModel->findUrl(directUrls.first()));
    }
}

void MainWindow::addUrls(const QList<QUrl> &urls, bool playFirst)
{
    const int first = m_playlistModel->rowCount();
    m_playlistModel->addUrls(urls);
    m_mediaLibrary->addUrls(urls);
    if (playFirst) {
        if (first < m_playlistModel->rowCount())
            playSourceRow(first);
        else if (!urls.isEmpty())
            playSourceRow(m_playlistModel->findUrl(urls.first()));
    }
}

void MainWindow::openFiles()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Open audio"), QStandardPaths::writableLocation(QStandardPaths::MusicLocation),
        tr(kAudioFilter));
    addPaths(files, m_player->source().isEmpty());
}

void MainWindow::openFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(
        this, tr("Open music folder"), QStandardPaths::writableLocation(QStandardPaths::MusicLocation));
    if (!folder.isEmpty()) {
        const int first = m_playlistModel->rowCount();
        addDirectory(folder);
        if (m_player->source().isEmpty() && first < m_playlistModel->rowCount())
            playSourceRow(first);
    }
}

void MainWindow::addDirectory(const QString &path)
{
    QList<QUrl> urls;
    QDirIterator iterator(path, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        const QString file = iterator.next();
        if (kAudioExtensions.contains(QFileInfo(file).suffix().toLower()))
            urls.append(QUrl::fromLocalFile(file));
    }
    std::sort(urls.begin(), urls.end(), [](const QUrl &left, const QUrl &right) {
        return left.toLocalFile().localeAwareCompare(right.toLocalFile()) < 0;
    });
    addUrls(urls);
}

void MainWindow::savePlaylist()
{
    if (m_playlistModel->rowCount() == 0)
        return;
    QString path = QFileDialog::getSaveFileName(this, tr("Save playlist"),
                                                 QStringLiteral("playlist.m3u8"),
                                                 tr("M3U playlist (*.m3u8 *.m3u)"));
    if (path.isEmpty())
        return;
    if (QFileInfo(path).suffix().isEmpty())
        path += QStringLiteral(".m3u8");
    QString error;
    if (!m_playlistModel->saveM3u(path, &error))
        statusBar()->showMessage(tr("Could not save playlist: %1").arg(error), 6000);
    else
        statusBar()->showMessage(tr("Playlist saved"), 3000);
}

QList<int> MainWindow::selectedSourceRows() const
{
    QList<int> rows;
    for (const QModelIndex &viewIndex : m_playlistView->selectionModel()->selectedRows())
        rows.append(m_filterModel->mapToSource(viewIndex).row());
    std::sort(rows.begin(), rows.end(), std::greater<>());
    return rows;
}

void MainWindow::removeSelected()
{
    if (m_contentTabs->currentIndex() == 1) {
        QList<int> rows;
        for (const QModelIndex &viewIndex : m_libraryView->selectionModel()->selectedRows())
            rows.append(m_libraryFilterModel->mapToSource(viewIndex).row());
        const int removed = m_mediaLibrary->removeRows(rows);
        statusBar()->showMessage(tr("Removed %n item(s) from the library", nullptr, removed), 3000);
        return;
    }
    for (int row : selectedSourceRows()) {
        if (m_playlistModel->urlAt(row) == m_player->source())
            m_player->clear();
        m_playlistModel->removeRow(row);
    }
}

void MainWindow::clearPlaylist()
{
    if (m_contentTabs->currentIndex() == 1) {
        if (QMessageBox::question(this, tr("Clear Media Library"),
                                  tr("Remove every entry from the media library?\n"
                                     "Your media files will not be deleted."))
            == QMessageBox::Yes) {
            m_mediaLibrary->clear();
            statusBar()->showMessage(tr("Media library cleared"), 3000);
        }
        return;
    }
    m_player->clear();
    m_playlistModel->clear();
    updateNowPlaying();
}

void MainWindow::playCurrent()
{
    QModelIndex current = m_playlistView->currentIndex();
    if (!current.isValid() && m_filterModel->rowCount() > 0)
        current = m_filterModel->index(0, 0);
    playViewIndex(current);
}

void MainWindow::playViewIndex(const QModelIndex &index)
{
    if (index.isValid())
        playSourceRow(m_filterModel->mapToSource(index).row());
}

void MainWindow::playSourceRow(int row)
{
    const QUrl url = m_playlistModel->urlAt(row);
    if (url.isEmpty())
        return;
    m_playlistModel->setCurrentRow(row);
    const QModelIndex viewIndex = m_filterModel->mapFromSource(m_playlistModel->index(row));
    if (viewIndex.isValid()) {
        m_playlistView->setCurrentIndex(viewIndex);
        m_playlistView->scrollTo(viewIndex);
    }
    m_player->play(url);
}

void MainWindow::togglePlayback()
{
    if (m_player->source().isEmpty())
        playCurrent();
    else
        m_player->toggle();
}

void MainWindow::previousTrack()
{
    if (m_player->position() > 3000) {
        m_player->seek(0);
        return;
    }
    const int count = m_playlistModel->rowCount();
    if (count == 0)
        return;
    int row = m_playlistModel->currentRow() - 1;
    if (row < 0)
        row = count - 1;
    playSourceRow(row);
}

void MainWindow::nextTrack(bool automatic)
{
    const int row = nextRow(automatic);
    if (row >= 0)
        playSourceRow(row);
    else if (automatic)
        m_player->stop();
}

int MainWindow::nextRow(bool automatic)
{
    const int count = m_playlistModel->rowCount();
    if (count == 0)
        return -1;
    const int current = qMax(0, m_playlistModel->currentRow());
    if (automatic && m_repeatMode == RepeatMode::One)
        return current;
    if (m_shuffleButton->isChecked() && count > 1) {
        auto fillShuffleCycle = [this, count, current] {
            m_shuffleRemaining.clear();
            for (int row = 0; row < count; ++row) {
                if (row != current)
                    m_shuffleRemaining.append(row);
            }
            m_shuffleCycleActive = true;
        };
        if (!m_shuffleCycleActive)
            fillShuffleCycle();
        else if (m_shuffleRemaining.isEmpty()) {
            if (automatic && m_repeatMode != RepeatMode::All)
                return -1;
            fillShuffleCycle();
        }
        const int choice = QRandomGenerator::global()->bounded(m_shuffleRemaining.size());
        return m_shuffleRemaining.takeAt(choice);
    }
    if (current + 1 < count)
        return current + 1;
    if (!automatic || m_repeatMode == RepeatMode::All)
        return 0;
    return -1;
}

void MainWindow::resetShuffleCycle()
{
    m_shuffleRemaining.clear();
    m_shuffleCycleActive = false;
}

void MainWindow::cycleRepeatMode()
{
    switch (m_repeatMode) {
    case RepeatMode::Off: m_repeatMode = RepeatMode::All; break;
    case RepeatMode::All: m_repeatMode = RepeatMode::One; break;
    case RepeatMode::One: m_repeatMode = RepeatMode::Off; break;
    }
    updateRepeatButton();
}

void MainWindow::updateRepeatButton()
{
    switch (m_repeatMode) {
    case RepeatMode::Off: m_repeatButton->setText(tr("Repeat off")); break;
    case RepeatMode::All: m_repeatButton->setText(tr("Repeat all")); break;
    case RepeatMode::One: m_repeatButton->setText(tr("Repeat one")); break;
    }
    if (m_wasabiWidget)
        m_wasabiWidget->setRepeatMode(static_cast<int>(m_repeatMode));
}

void MainWindow::refreshAudioOutputs()
{
    const QByteArray selected = m_player->audioOutputId();
    const QByteArray saved = QSettings().value(QStringLiteral("audio/device")).toByteArray();
    const QSignalBlocker blocker(m_audioDeviceCombo);
    m_audioDeviceCombo->clear();
    int selectedIndex = -1;
    for (const QAudioDevice &device : m_player->audioOutputs()) {
        m_audioDeviceCombo->addItem(device.description(), device.id());
        const int index = m_audioDeviceCombo->count() - 1;
        if ((!saved.isEmpty() && device.id() == saved)
            || (saved.isEmpty() && device.id() == selected))
            selectedIndex = index;
    }
    if (selectedIndex < 0 && m_audioDeviceCombo->count() > 0)
        selectedIndex = 0;
    m_audioDeviceCombo->setCurrentIndex(selectedIndex);
    if (selectedIndex >= 0)
        selectAudioOutput(selectedIndex);
}

void MainWindow::selectAudioOutput(int index)
{
    if (index >= 0 && m_player->setAudioOutput(m_audioDeviceCombo->itemData(index).toByteArray()))
        statusBar()->showMessage(tr("Audio output: %1").arg(m_audioDeviceCombo->itemText(index)), 3000);
}

bool MainWindow::setAudioOutput(const QString &deviceName)
{
    const bool selected = m_player->setAudioOutput(deviceName);
    if (selected) {
        const QSignalBlocker blocker(m_audioDeviceCombo);
        const int index = m_audioDeviceCombo->findData(m_player->audioOutputId());
        if (index >= 0)
            m_audioDeviceCombo->setCurrentIndex(index);
    }
    return selected;
}

void MainWindow::setDspEnabled(bool enabled)
{
    m_player->setDspEnabled(enabled);
}

void MainWindow::setEqualizerPreset(const QString &preset)
{
    m_player->equalizer()->applyPreset(preset);
}

void MainWindow::updateNowPlaying()
{
    if (m_player->source().isEmpty()) {
        m_titleLabel->setText(tr("Nothing playing"));
        m_detailsLabel->setText(tr("Add music to begin"));
        m_locationLabel->setText(tr("Drop files or folders anywhere in this window"));
        setWindowTitle(QStringLiteral("LlamaAmp Qt"));
        updateArtwork();
        return;
    }
    const QMediaMetaData metadata = m_player->metaData();
    QString title = metadata.stringValue(QMediaMetaData::Title);
    if (title.isEmpty())
        title = PlaylistModel::displayName(m_player->source());
    QStringList details;
    QString artist = metadata.stringValue(QMediaMetaData::AlbumArtist);
    if (artist.isEmpty())
        artist = metadata.stringValue(QMediaMetaData::ContributingArtist);
    const QString album = metadata.stringValue(QMediaMetaData::AlbumTitle);
    if (!artist.isEmpty()) details.append(artist);
    if (!album.isEmpty()) details.append(album);
    m_titleLabel->setText(title);
    m_playlistModel->setTitleForUrl(m_player->source(), title);
    m_mediaLibrary->updateMetadata(m_player->source(), title, artist, album, m_player->duration());
    m_detailsLabel->setText(details.isEmpty() ? tr("Unknown artist")
                                              : details.join(QStringLiteral("  •  ")));
    m_locationLabel->setText(m_player->source().isLocalFile()
                                 ? m_player->source().toLocalFile()
                                 : m_player->source().toDisplayString());
    setWindowTitle(tr("%1 — LlamaAmp Qt").arg(title));
    updateArtwork();
}

void MainWindow::updateArtwork()
{
    const QImage artwork = m_player->metaData().value(QMediaMetaData::CoverArtImage).value<QImage>();
    if (artwork.isNull()) {
        m_artworkLabel->setPixmap(QIcon(QStringLiteral(":/icons/llamaamp.svg")).pixmap(76, 76));
        return;
    }
    m_artworkLabel->setPixmap(QPixmap::fromImage(artwork).scaled(
        m_artworkLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}

void MainWindow::updatePosition(qint64 position)
{
    if (!m_userSeeking)
        m_seekSlider->setValue(static_cast<int>(qMin(position, qint64(INT_MAX))));
    m_elapsedLabel->setText(formatTime(position));
}

void MainWindow::updateDuration(qint64 duration)
{
    m_seekSlider->setRange(0, static_cast<int>(qMin(duration, qint64(INT_MAX))));
    m_durationLabel->setText(formatTime(duration));
    if (!m_player->source().isEmpty())
        updateNowPlaying();
}

void MainWindow::updatePlaybackState(QMediaPlayer::PlaybackState state)
{
    m_playButton->setIcon(style()->standardIcon(state == QMediaPlayer::PlayingState
                                                    ? QStyle::SP_MediaPause
                                                    : QStyle::SP_MediaPlay));
}

void MainWindow::updateQueueSummary()
{
    const int total = m_playlistModel->rowCount();
    const bool libraryVisible = m_contentTabs && m_contentTabs->currentIndex() == 1;
    const int visible = libraryVisible ? m_libraryFilterModel->rowCount() : m_filterModel->rowCount();
    const int available = libraryVisible && m_mediaLibrary->model()
        ? m_mediaLibrary->model()->rowCount() : total;
    m_queueSummaryLabel->setText(tr("%n track(s) ready", nullptr, total));
    m_resultCountLabel->setText(m_searchEdit && !m_searchEdit->text().isEmpty()
                                    ? tr("%1 of %2").arg(visible).arg(available)
                                    : tr("%n track(s)", nullptr, available));
}

void MainWindow::showEqualizer()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("DSP Equalizer"));
    dialog.setMinimumWidth(620);
    auto *root = new QVBoxLayout(&dialog);
    auto *topRow = new QHBoxLayout;
    auto *enabled = new QCheckBox(tr("Enable processed audio output"), &dialog);
    enabled->setChecked(m_player->dspEnabled());
    auto *preset = new QComboBox(&dialog);
    preset->addItems(m_player->equalizer()->presetNames());
    topRow->addWidget(enabled);
    topRow->addStretch();
    topRow->addWidget(new QLabel(tr("Preset:"), &dialog));
    topRow->addWidget(preset);
    root->addLayout(topRow);

    auto *bands = new QHBoxLayout;
    QVector<QSlider *> bandSliders;
    for (int band = 0; band < Equalizer::BandCount; ++band) {
        auto *column = new QVBoxLayout;
        auto *gain = new QLabel(QStringLiteral("%1 dB").arg(m_player->equalizer()->bandGain(band)),
                                &dialog);
        gain->setAlignment(Qt::AlignCenter);
        auto *slider = new QSlider(Qt::Vertical, &dialog);
        slider->setRange(-12, 12);
        slider->setValue(qRound(m_player->equalizer()->bandGain(band)));
        slider->setTickPosition(QSlider::TicksBothSides);
        bandSliders.append(slider);
        auto *frequency = new QLabel(Equalizer::frequency(band) >= 1000
                                         ? QStringLiteral("%1k").arg(Equalizer::frequency(band) / 1000)
                                         : QString::number(Equalizer::frequency(band)), &dialog);
        frequency->setAlignment(Qt::AlignCenter);
        column->addWidget(gain);
        column->addWidget(slider, 1, Qt::AlignHCenter);
        column->addWidget(frequency);
        bands->addLayout(column);
        connect(slider, &QSlider::valueChanged, &dialog, [this, band, gain](int value) {
            m_player->equalizer()->setBandGain(band, float(value));
            gain->setText(QStringLiteral("%1 dB").arg(value));
        });
    }
    root->addLayout(bands, 1);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dialog);
    root->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(enabled, &QCheckBox::toggled, m_player, &PlayerController::setDspEnabled);
    connect(preset, &QComboBox::currentTextChanged, &dialog,
            [this, bandSliders](const QString &name) {
                m_player->equalizer()->applyPreset(name);
                for (int band = 0; band < bandSliders.size(); ++band)
                    bandSliders.at(band)->setValue(qRound(m_player->equalizer()->bandGain(band)));
            });
    dialog.exec();
}

void MainWindow::showSkinBrowser()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("XML Skin Browser"));
    dialog.resize(760, 480);
    auto *root = new QVBoxLayout(&dialog);
    auto *splitter = new QSplitter(&dialog);
    auto *skins = new QListWidget(splitter);
    auto *details = new QWidget(splitter);
    auto *detailsLayout = new QVBoxLayout(details);
    auto *preview = new QLabel(details);
    preview->setMinimumSize(420, 240);
    preview->setAlignment(Qt::AlignCenter);
    preview->setObjectName(QStringLiteral("artwork"));
    auto *metadata = new QLabel(details);
    metadata->setWordWrap(true);
    metadata->setTextInteractionFlags(Qt::TextSelectableByMouse);
    detailsLayout->addWidget(preview, 1);
    detailsLayout->addWidget(metadata);
    splitter->addWidget(skins);
    splitter->addWidget(details);
    splitter->setStretchFactor(1, 1);
    root->addWidget(splitter, 1);
    auto *note = new QLabel(tr("User-supplied Bento-family skins can use the partial XML "
                               "compatibility player. No third-party skins are bundled."),
                            &dialog);
    note->setWordWrap(true);
    root->addWidget(note);
    const QList<LegacySkinInfo> catalog = SkinManager::legacySkinCatalog();
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Apply | QDialogButtonBox::Close, &dialog);
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    connect(buttons->button(QDialogButtonBox::Apply), &QPushButton::clicked, &dialog,
            [this, skins, catalog, &dialog] {
                const int row = skins->currentRow();
                if (row < 0 || row >= catalog.size())
                    return;
                if (!SkinManager::canRenderWasabiSkin(catalog.at(row))) {
                    QMessageBox::information(this, tr("Skin not renderable"),
                        tr("This XML layout is catalogued but does not yet have a renderer."));
                    return;
                }
                const int comboIndex = m_skinCombo->findData(catalog.at(row).directory);
                if (comboIndex >= 0) {
                    m_skinCombo->setCurrentIndex(comboIndex);
                    QSettings settings;
                    settings.setValue(QStringLiteral("appearance/skin"),
                                      m_skinCombo->currentText());
                    dialog.accept();
                }
            });
    root->addWidget(buttons);

    for (const LegacySkinInfo &skin : catalog)
        skins->addItem(skin.name);
    connect(skins, &QListWidget::currentRowChanged, &dialog,
            [catalog, preview, metadata](int row) {
                if (row < 0 || row >= catalog.size())
                    return;
                const LegacySkinInfo &skin = catalog.at(row);
                const QPixmap screenshot(skin.screenshot);
                preview->setPixmap(screenshot.isNull() ? QPixmap{} : screenshot.scaled(
                    preview->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
                metadata->setText(QObject::tr(
                    "%1 %2\nAuthor: %3\nFormat: %4\nIncludes: %5  Scripts: %6  Accelerators: %7\nRenderer: %8\n\n%9%10")
                    .arg(skin.name, skin.version, skin.author, skin.rootElement)
                    .arg(skin.includeCount).arg(skin.scriptCount).arg(skin.acceleratorCount)
                    .arg(SkinManager::canRenderWasabiSkin(skin)
                             ? QObject::tr("partial Bento-family compatibility surface")
                             : QObject::tr("catalog only"))
                    .arg(skin.comment, skin.error.isEmpty()
                                           ? QString{} : QObject::tr("\nParse error: %1").arg(skin.error)));
            });
    if (skins->count() > 0)
        skins->setCurrentRow(0);
    dialog.exec();
}

QString MainWindow::formatTime(qint64 milliseconds)
{
    const qint64 totalSeconds = qMax<qint64>(0, milliseconds / 1000);
    const qint64 hours = totalSeconds / 3600;
    const qint64 minutes = (totalSeconds % 3600) / 60;
    const qint64 seconds = totalSeconds % 60;
    if (hours > 0)
        return QStringLiteral("%1:%2:%3").arg(hours).arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(minutes).arg(seconds, 2, 10, QLatin1Char('0'));
}

void MainWindow::restoreState()
{
    QSettings settings;
    restoreGeometry(settings.value(QStringLiteral("window/geometry")).toByteArray());
    m_volumeSlider->setValue(settings.value(QStringLiteral("audio/volume"), 80).toInt());
    m_shuffleButton->setChecked(settings.value(QStringLiteral("playback/shuffle"), false).toBool());
    m_repeatMode = static_cast<RepeatMode>(
        qBound(0, settings.value(QStringLiteral("playback/repeat"), 0).toInt(), 2));
    updateRepeatButton();
    m_skinCombo->setCurrentText(settings.value(QStringLiteral("appearance/skin"),
                                                QStringLiteral("Llama Green")).toString());
    QList<float> equalizerGains;
    for (const QVariant &value : settings.value(QStringLiteral("dsp/equalizerGains")).toList())
        equalizerGains.append(value.toFloat());
    if (!equalizerGains.isEmpty())
        m_player->equalizer()->setGains(equalizerGains);
    m_player->setDspEnabled(settings.value(QStringLiteral("dsp/enabled"), false).toBool());
    refreshAudioOutputs();
    QList<QUrl> urls;
    for (const QString &value : settings.value(QStringLiteral("playlist/urls")).toStringList())
        urls.append(QUrl(value));
    m_playlistModel->addUrls(urls);
    const int row = qBound(-1, settings.value(QStringLiteral("playlist/current"), -1).toInt(),
                           m_playlistModel->rowCount() - 1);
    m_playlistModel->setCurrentRow(row);
}

void MainWindow::saveState() const
{
    QSettings settings;
    settings.setValue(QStringLiteral("window/geometry"), saveGeometry());
    settings.setValue(QStringLiteral("audio/volume"), m_volumeSlider->value());
    settings.setValue(QStringLiteral("audio/device"), m_player->audioOutputId());
    settings.setValue(QStringLiteral("playback/shuffle"), m_shuffleButton->isChecked());
    settings.setValue(QStringLiteral("playback/repeat"), static_cast<int>(m_repeatMode));
    settings.setValue(QStringLiteral("appearance/skin"), m_skinCombo->currentText());
    QVariantList equalizerGains;
    for (float gain : m_player->equalizer()->gains())
        equalizerGains.append(gain);
    settings.setValue(QStringLiteral("dsp/equalizerGains"), equalizerGains);
    settings.setValue(QStringLiteral("dsp/enabled"), m_player->dspEnabled());
    settings.setValue(QStringLiteral("playlist/current"), m_playlistModel->currentRow());
    QStringList urls;
    for (const QUrl &url : m_playlistModel->urls())
        urls.append(url.toString(QUrl::FullyEncoded));
    settings.setValue(QStringLiteral("playlist/urls"), urls);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveState();
    QMainWindow::closeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *event)
{
    QStringList localPaths;
    QList<QUrl> remoteUrls;
    for (const QUrl &url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            localPaths.append(url.toLocalFile());
        else
            remoteUrls.append(url);
    }
    const bool playFirst = m_player->source().isEmpty();
    addPaths(localPaths, playFirst);
    addUrls(remoteUrls, playFirst && m_player->source().isEmpty());
    event->acceptProposedAction();
}

void MainWindow::applyStyle()
{
    const QString baseStyle = QStringLiteral(R"(
        * { font-family: "Inter", "Noto Sans", sans-serif; }
        QMainWindow, QWidget { background: #121513; color: #e7eee8; }
        QMenuBar, QMenu, QStatusBar { background: #0d100e; color: #d7dfd8; }
        QMenuBar::item:selected, QMenu::item:selected { background: #315c39; }
        QFrame#sidebar { background: #0d100e; border-right: 1px solid #29302b; }
        QLabel#brand { color: #a1ffae; font-size: 18px; font-weight: 800; letter-spacing: 2px; }
        QLabel#sideHeading, QLabel#eyebrow { color: #718077; font-size: 10px; font-weight: 700; letter-spacing: 1.5px; }
        QLabel#queueSummary { color: #9caaa0; padding: 8px 2px; }
        QPushButton#sideButton { text-align: left; background: transparent; border: none; padding: 9px; }
        QPushButton#sideButton:hover { background: #1c241e; color: #92f6a0; }
        QFrame#nowPlaying { background: #18201a; border: 1px solid #304035; border-radius: 10px; }
        QLabel#artwork { background: #0d120e; border: 1px solid #334238; border-radius: 7px; }
        QLabel#trackTitle { color: #a1ffae; font-size: 24px; font-weight: 700; }
        QLabel#trackDetails { color: #c4cec6; font-size: 14px; }
        QLabel#trackLocation { color: #748079; font-size: 11px; }
        QLabel#sectionTitle { color: #eaf4eb; font-size: 15px; font-weight: 800; letter-spacing: 1px; }
        QFrame#transport { background: #181c19; border: 1px solid #303732; border-radius: 8px; }
        QVideoWidget#videoOutput { background: black; border: 1px solid #334238; border-radius: 7px; }
        QPushButton { background: #292e2a; border: 1px solid #3e4740; border-radius: 5px; padding: 7px 11px; }
        QPushButton:hover { background: #354139; border-color: #64c874; }
        QPushButton:pressed, QPushButton:checked { background: #315c39; border-color: #79e789; }
        QPushButton#primaryPlay { background: #4d9d5a; border-color: #72db81; }
        QLineEdit, QComboBox { background: #0d110e; border: 1px solid #333c36; border-radius: 5px; padding: 7px; }
        QLineEdit:focus, QComboBox:focus { border-color: #68d678; }
        QListView { background: #0d100e; alternate-background-color: #121713; border: 1px solid #2e3730; border-radius: 7px; outline: none; }
        QTableView { background: #0d100e; alternate-background-color: #121713; border: 1px solid #2e3730; gridline-color: #242b26; selection-background-color: #2d5134; }
        QHeaderView::section { background: #181e1a; color: #a8b4aa; padding: 8px; border: none; border-right: 1px solid #29312b; }
        QTabWidget::pane { border: none; }
        QTabBar::tab { background: #171c18; color: #9ca89e; padding: 8px 18px; border-top-left-radius: 5px; border-top-right-radius: 5px; }
        QTabBar::tab:selected { background: #2d5134; color: white; }
        QListView::item { padding: 10px 12px; border-bottom: 1px solid #202621; }
        QListView::item:selected { background: #2d5134; color: white; }
        QSlider::groove:horizontal { background: #343a35; height: 5px; border-radius: 2px; }
        QSlider::sub-page:horizontal { background: #67ce76; border-radius: 2px; }
        QSlider::handle:horizontal { background: #a3ffaf; width: 13px; margin: -5px 0; border-radius: 6px; }
        QScrollBar:vertical { background: #101411; width: 10px; }
        QScrollBar::handle:vertical { background: #39443b; border-radius: 5px; min-height: 30px; }
    )");
    if (m_skinCombo && !m_skinCombo->currentData().toString().isEmpty()) {
        const QString directory = m_skinCombo->currentData().toString();
        const QList<LegacySkinInfo> catalog = SkinManager::legacySkinCatalog();
        const auto selected = std::find_if(catalog.cbegin(), catalog.cend(),
                                           [&directory](const LegacySkinInfo &skin) {
            return skin.directory == directory;
        });
        if (selected != catalog.cend()) {
            m_wasabiWidget->setSkin(*selected);
            m_player->setVideoOutput(m_wasabiWidget->videoOutput());
            m_centralStack->setCurrentWidget(m_wasabiWidget);
            menuBar()->hide();
            statusBar()->hide();
            setMinimumSize(633, 492);
            if (width() < 800 || height() < 600)
                resize(800, 600);
            qApp->setStyleSheet(QStringLiteral(R"(
                QWidget { background: #15191b; color: #cbd1d3; font-family: sans-serif; }
                QTabWidget::pane { border: 1px solid #384044; }
                QTabBar::tab { background: #242a2d; color: #aeb8bc; padding: 5px 14px; }
                QTabBar::tab:selected { background: #4b5357; color: white; }
                QListView { background: #0b1012; alternate-background-color: #111719;
                            border: 1px solid #323a3e; outline: none; }
                QListView::item { padding: 6px; border-bottom: 1px solid #252c2f; }
                QListView::item:selected { background: #566268; color: white; }
            )"));
            return;
        }
    }
    if (m_centralStack)
        m_centralStack->setCurrentIndex(0);
    m_player->setVideoOutput(m_videoWidget);
    menuBar()->show();
    statusBar()->show();
    setMinimumSize(760, 560);
    qApp->setStyleSheet(SkinManager::applyPalette(
        baseStyle, m_skinCombo ? m_skinCombo->currentText() : QStringLiteral("Llama Green")));
}
