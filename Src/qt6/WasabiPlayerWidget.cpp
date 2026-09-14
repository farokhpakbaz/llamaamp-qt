/*
 * SPDX-FileCopyrightText: 2026 LlamaAmp contributors
 * SPDX-License-Identifier: Apache-2.0
 */

#include "WasabiPlayerWidget.h"

#include "AudioVisualizationWidget.h"
#include "Equalizer.h"
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
#include <QTabBar>
#include <QTabWidget>
#include <QTableView>
#include <QVideoWidget>

#include <functional>

class ClassicEqualizerWidget final : public QWidget
{
public:
    explicit ClassicEqualizerWidget(PlayerController *player, QWidget *parent = nullptr)
        : QWidget(parent), m_player(player)
    {
        setObjectName(QStringLiteral("classicEqualizer"));
        setAccessibleName(tr("Classic ten-band equalizer"));
        setFixedHeight(172);
        connect(m_player->equalizer(), &Equalizer::changed, this,
                qOverload<>(&ClassicEqualizerWidget::update));
        connect(m_player, &PlayerController::dspEnabledChanged, this,
                qOverload<>(&ClassicEqualizerWidget::update));
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter painter(this);
        painter.fillRect(rect(), QColor(24, 25, 47));
        painter.setPen(QColor(100, 104, 130));
        painter.drawRect(rect().adjusted(1, 1, -2, -2));
        painter.fillRect(QRect(4, 4, width() - 8, 17), QColor(48, 50, 75));
        painter.setPen(QColor(215, 217, 225));
        painter.setFont(QFont(QStringLiteral("Sans Serif"), 7, QFont::Bold));
        painter.drawText(QRect(0, 4, width(), 17), Qt::AlignCenter,
                         QStringLiteral("LLAMAAMP EQUALIZER"));

        drawButton(painter, QRect(10, 27, 39, 17), QStringLiteral("ON"),
                   m_player->dspEnabled());
        drawButton(painter, QRect(54, 27, 48, 17), QStringLiteral("AUTO"), false);
        drawButton(painter, QRect(width() - 89, 27, 77, 17), QStringLiteral("PRESETS"), false);
        painter.setPen(QColor(182, 169, 63));
        painter.drawLine(125, 38, width() - 103, 38);

        painter.setFont(QFont(QStringLiteral("Monospace"), 7));
        painter.setPen(QColor(246, 220, 51));
        painter.drawText(QRect(55, 50, 48, 13), Qt::AlignCenter, QStringLiteral("+12 dB"));
        painter.drawText(QRect(55, 94, 48, 13), Qt::AlignCenter, QStringLiteral("0 dB"));
        painter.drawText(QRect(55, 139, 48, 13), Qt::AlignCenter, QStringLiteral("-12 dB"));
        drawSlider(painter, 42, 0.0F);
        painter.setPen(QColor(221, 224, 228));
        painter.drawText(QRect(10, 151, 64, 14), Qt::AlignCenter, QStringLiteral("PREAMP"));

        for (int band = 0; band < Equalizer::BandCount; ++band) {
            const int x = sliderX(band);
            drawSlider(painter, x, m_player->equalizer()->bandGain(band));
            const int frequency = Equalizer::frequency(band);
            const QString label = frequency >= 1000
                ? QStringLiteral("%1K").arg(frequency / 1000)
                : QString::number(frequency);
            painter.setPen(QColor(221, 224, 228));
            painter.drawText(QRect(x - 15, 151, 30, 14), Qt::AlignCenter, label);
        }
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        const QPoint point = event->position().toPoint();
        if (QRect(10, 27, 39, 17).contains(point)) {
            m_player->setDspEnabled(!m_player->dspEnabled());
            return;
        }
        if (QRect(width() - 89, 27, 77, 17).contains(point)) {
            const QStringList presets = m_player->equalizer()->presetNames();
            m_preset = (m_preset + 1) % presets.size();
            m_player->equalizer()->applyPreset(presets.at(m_preset));
            setToolTip(tr("Equalizer preset: %1").arg(presets.at(m_preset)));
            return;
        }
        if (point.y() < 54 || point.y() > 146)
            return;
        for (int band = 0; band < Equalizer::BandCount; ++band) {
            if (qAbs(point.x() - sliderX(band)) <= 12) {
                const float gain = 12.0F - 24.0F * float(point.y() - 56) / 88.0F;
                m_player->equalizer()->setBandGain(band, gain);
                return;
            }
        }
    }

private:
    static void drawButton(QPainter &painter, const QRect &rect, const QString &text, bool active)
    {
        painter.fillRect(rect, QColor(205, 208, 211));
        painter.setPen(QColor(248, 248, 248));
        painter.drawLine(rect.topLeft(), rect.topRight());
        painter.drawLine(rect.topLeft(), rect.bottomLeft());
        painter.setPen(QColor(38, 40, 50));
        painter.drawLine(rect.bottomLeft(), rect.bottomRight());
        painter.drawLine(rect.topRight(), rect.bottomRight());
        painter.setPen(active ? QColor(0, 110, 35) : QColor(35, 38, 47));
        painter.drawText(rect, Qt::AlignCenter, (active ? QStringLiteral("■ ") : QString{}) + text);
    }

    static void drawSlider(QPainter &painter, int x, float gain)
    {
        painter.fillRect(QRect(x - 2, 56, 4, 90), QColor(8, 9, 19));
        painter.fillRect(QRect(x - 1, 57, 2, 88), QColor(246, 220, 51));
        const int y = 56 + qRound((12.0F - qBound(-12.0F, gain, 12.0F)) * 88.0F / 24.0F);
        drawButton(painter, QRect(x - 7, y - 6, 14, 12), QString{}, false);
    }

    int sliderX(int band) const
    {
        return 126 + band * qMax(24, (width() - 148) / (Equalizer::BandCount - 1));
    }

    PlayerController *m_player = nullptr;
    int m_preset = -1;
};

class WasabiDeck final : public QWidget
{
public:
    enum class Action {
        Previous, Play, Pause, Stop, Next, Open, Mute, Shuffle, Repeat,
        FileMenu, PlayMenu, OptionsMenu, ViewMenu, HelpMenu, WindowShade,
        ToggleEqualizer, ShowPlaylist
    };

    explicit WasabiDeck(QWidget *parent = nullptr) : QWidget(parent)
    {
        setObjectName(QStringLiteral("wasabiDeck"));
        setAccessibleName(tr("Classic player controls"));
        setAccessibleDescription(
            tr("Playback, seeking, volume, menus, and window controls"));
        setFixedHeight(188);
        setMouseTracking(true);
    }

    void setRuntime(const WasabiRuntime *runtime)
    {
        m_runtime = runtime;
        m_runtimeImages.clear();
        if (!m_windowShaded)
            setFixedHeight(runtime ? 119 : 188);
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

    void setWindowShaded(bool shaded)
    {
        m_windowShaded = shaded;
        setFixedHeight(shaded ? 36 : (m_runtime ? 119 : 188));
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
        if (!m_runtime) {
            paintBuiltIn(painter);
            return;
        }
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
        if (!drawRuntimeBitmap(painter,
                               QStringLiteral("window.titlebar.button.sysmenu.normal"),
                               QRect(5, 2, 15, 13)))
            drawFallbackButton(painter, QRect(5, 2, 15, 13), QStringLiteral("L"));
        if (!drawRuntimeBitmap(painter,
                               QStringLiteral("window.titlebar.button.minimize.normal"),
                               QRect(width() - 79, 2, 17, 13)))
            drawFallbackButton(painter, QRect(width() - 79, 2, 17, 13), QStringLiteral("−"));
        if (!drawRuntimeBitmap(painter,
                               QStringLiteral("window.titlebar.button.maximize.normal"),
                               QRect(width() - 60, 2, 17, 13)))
            drawFallbackButton(painter, QRect(width() - 60, 2, 17, 13), QStringLiteral("□"));
        if (!drawRuntimeBitmap(painter,
                               QStringLiteral("window.titlebar.button.shade.normal"),
                               QRect(width() - 41, 2, 17, 13)))
            drawFallbackButton(painter, QRect(width() - 41, 2, 17, 13),
                               m_windowShaded ? QStringLiteral("∨") : QStringLiteral("∧"));
        if (!drawRuntimeBitmap(painter,
                               QStringLiteral("window.titlebar.button.close.normal"),
                               QRect(width() - 22, 2, 17, 13)))
            drawFallbackButton(painter, QRect(width() - 22, 2, 17, 13), QStringLiteral("×"));

        if (m_windowShaded) {
            painter.fillRect(QRect(3, 19, width() - 6, 14), QColor(8, 16, 19));
            painter.setPen(QColor(156, 195, 207));
            painter.setFont(QFont(QStringLiteral("Monospace"), 7, QFont::Bold));
            painter.drawText(QRect(8, 19, 44, 14), Qt::AlignVCenter, formatTime(m_position));
            painter.setFont(QFont(QStringLiteral("Sans Serif"), 7, QFont::Bold));
            painter.drawText(QRect(58, 19, qMax(0, width() - 180), 14),
                             Qt::AlignVCenter, m_title);
            const int controlsX = width() - 112;
            drawFallbackButton(painter, QRect(controlsX, 20, 18, 12), QStringLiteral("|<"));
            drawFallbackButton(painter, QRect(controlsX + 19, 20, 18, 12),
                               m_playing ? QStringLiteral("||") : QStringLiteral(">"));
            drawFallbackButton(painter, QRect(controlsX + 38, 20, 18, 12), QStringLiteral("[]"));
            drawFallbackButton(painter, QRect(controlsX + 57, 20, 18, 12), QStringLiteral(">|"));
            painter.setPen(QColor(49, 57, 60));
            painter.drawLine(width() - 34, 26, width() - 7, 26);
            painter.setPen(QColor(159, 186, 196));
            painter.drawLine(width() - 34, 26, width() - 34 + 27 * m_volume / 100, 26);
            return;
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
        if (!drawRuntimeBitmap(painter, QStringLiteral("player.button.previous.normal"),
                               QRect(4, 91, 26, 24)))
            drawFallbackButton(painter, QRect(4, 91, 26, 24), QStringLiteral("|<"));
        if (!drawRuntimeBitmap(painter, QStringLiteral("player.button.play.normal"),
                               QRect(29, 91, 22, 24)))
            drawFallbackButton(painter, QRect(29, 91, 22, 24), QStringLiteral(">"));
        if (!drawRuntimeBitmap(painter, QStringLiteral("player.button.pause.normal"),
                               QRect(50, 91, 22, 24)))
            drawFallbackButton(painter, QRect(50, 91, 22, 24), QStringLiteral("||"));
        if (!drawRuntimeBitmap(painter, QStringLiteral("player.button.stop.normal"),
                               QRect(71, 91, 22, 24)))
            drawFallbackButton(painter, QRect(71, 91, 22, 24), QStringLiteral("[]"));
        if (!drawRuntimeBitmap(painter, QStringLiteral("player.button.next.normal"),
                               QRect(92, 91, 26, 24)))
            drawFallbackButton(painter, QRect(92, 91, 26, 24), QStringLiteral(">|"));
        if (!drawRuntimeBitmap(painter, QStringLiteral("player.button.eject.normal"),
                               QRect(118, 94, 25, 17)))
            drawFallbackButton(painter, QRect(118, 94, 25, 17), QStringLiteral("^"));
        if (!drawRuntimeBitmap(painter, QStringLiteral("player.button.shuffle.normal%1")
                                            .arg(m_shuffle ? 1 : 0), QRect(147, 94, 32, 17)))
            drawFallbackButton(painter, QRect(147, 94, 32, 17), QStringLiteral("SHF"), m_shuffle);
        if (!drawRuntimeBitmap(painter, QStringLiteral("player.button.repeat.normal%1")
                                            .arg(m_repeatMode), QRect(179, 94, 32, 17)))
            drawFallbackButton(painter, QRect(179, 94, 32, 17),
                               m_repeatMode == 2 ? QStringLiteral("R1") : QStringLiteral("REP"),
                               m_repeatMode != 0);
        if (!drawRuntimeBitmap(painter,
                               m_volume == 0 ? QStringLiteral("player.button.demute.normal")
                                             : QStringLiteral("player.button.mute.normal"),
                               QRect(119, 61, 25, 14)))
            drawFallbackButton(painter, QRect(119, 61, 25, 14),
                               m_volume == 0 ? QStringLiteral("X") : QStringLiteral("VOL"));

        painter.setPen(QColor(49, 57, 60));
        painter.drawLine(150, 68, 226, 68);
        painter.setPen(QColor(159, 186, 196));
        painter.drawLine(150, 68, 150 + 76 * m_volume / 100, 68);
        const int volumeThumbX = 150 + (63 * m_volume / 100);
        if (!drawRuntimeBitmap(painter, QStringLiteral("player.volume.thumb.normal"),
                               QRect(volumeThumbX, 64, 17, 10)))
            drawFallbackButton(painter, QRect(volumeThumbX, 63, 10, 11), QString{});

        painter.setPen(QColor(49, 57, 60));
        painter.drawLine(8, 83, 227, 83);
        int seekThumbX = 7;
        if (m_duration > 0)
            seekThumbX += int(188 * m_position / m_duration);
        painter.setPen(QColor(159, 186, 196));
        painter.drawLine(8, 83, seekThumbX + 15, 83);
        if (!drawRuntimeBitmap(painter, QStringLiteral("player.posbar.thumb.normal"),
                               QRect(seekThumbX, 79, 31, 10)))
            drawFallbackButton(painter, QRect(seekThumbX, 78, 16, 11), QString{});

        painter.setPen(m_playing ? QColor(160, 205, 216) : QColor(73, 88, 94));
        painter.drawText(QRect(211, 48, 12, 12), Qt::AlignCenter,
                         m_playing ? QStringLiteral("▶") : QStringLiteral("■"));
    }

    void mousePressEvent(QMouseEvent *event) override
    {
        const QPoint point = event->position().toPoint();
        if (!m_runtime) {
            handleBuiltInMouse(point);
            return;
        }
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
            else if (point.x() >= width() - 41 && point.x() < width() - 24)
                trigger(Action::WindowShade);
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
        if (m_windowShaded && point.y() >= 19 && point.y() <= 35) {
            const int controlsX = width() - 112;
            if (point.x() >= controlsX && point.x() < controlsX + 19)
                trigger(Action::Previous);
            else if (point.x() < controlsX + 38 && point.x() >= controlsX + 19)
                trigger(m_playing ? Action::Pause : Action::Play);
            else if (point.x() < controlsX + 57 && point.x() >= controlsX + 38)
                trigger(Action::Stop);
            else if (point.x() < controlsX + 76 && point.x() >= controlsX + 57)
                trigger(Action::Next);
            else if (point.x() >= width() - 34) {
                const int volume = qBound(0, (point.x() - (width() - 34)) * 100 / 27, 100);
                setVolume(volume);
                if (volumeChanged) volumeChanged(volume);
            }
            return;
        }
        if (m_duration > 0 && point.y() >= 76 && point.y() <= 91
            && point.x() >= 7 && point.x() <= 229) {
            if (seekRequested)
                seekRequested(m_duration * (point.x() - 7) / 222);
        }
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->position().y() <= 18) {
            trigger(Action::WindowShade);
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
    }

private:
    void handleBuiltInMouse(const QPoint &point)
    {
        if (point.y() <= 21) {
            if (QRect(width() - 70, 6, 16, 13).contains(point))
                window()->showMinimized();
            else if (QRect(width() - 52, 6, 16, 13).contains(point))
                trigger(Action::WindowShade);
            else if (QRect(width() - 34, 6, 16, 13).contains(point))
                window()->close();
            return;
        }
        if (m_windowShaded) {
            const int x = width() - 117;
            if (QRect(x, 22, 20, 12).contains(point)) trigger(Action::Previous);
            else if (QRect(x + 21, 22, 20, 12).contains(point))
                trigger(m_playing ? Action::Pause : Action::Play);
            else if (QRect(x + 42, 22, 20, 12).contains(point)) trigger(Action::Stop);
            else if (QRect(x + 63, 22, 20, 12).contains(point)) trigger(Action::Next);
            return;
        }
        if (point.y() >= 143 && point.y() <= 172) {
            if (point.x() >= 24 && point.x() < 62) trigger(Action::Previous);
            else if (point.x() < 102 && point.x() >= 64) trigger(Action::Play);
            else if (point.x() < 142 && point.x() >= 104) trigger(Action::Pause);
            else if (point.x() < 182 && point.x() >= 144) trigger(Action::Stop);
            else if (point.x() < 222 && point.x() >= 184) trigger(Action::Next);
            else if (point.x() < 263 && point.x() >= 225) trigger(Action::Open);
            else if (point.x() < 347 && point.x() >= 274) trigger(Action::Shuffle);
            else if (point.x() < 393 && point.x() >= 350) trigger(Action::Repeat);
            return;
        }
        if (point.y() >= 119 && point.y() <= 137 && point.x() >= 20
            && point.x() <= width() - 20 && m_duration > 0) {
            if (seekRequested)
                seekRequested(m_duration * (point.x() - 20) / qMax(1, width() - 40));
            return;
        }
        if (point.y() >= 88 && point.y() <= 108 && point.x() >= 174
            && point.x() <= 266) {
            const int volume = qBound(0, (point.x() - 174) * 100 / 92, 100);
            setVolume(volume);
            if (volumeChanged) volumeChanged(volume);
            return;
        }
        if (QRect(width() - 86, 91, 35, 19).contains(point))
            trigger(Action::ToggleEqualizer);
        else if (QRect(width() - 47, 91, 35, 19).contains(point))
            trigger(Action::ShowPlaylist);
    }

    void paintBuiltIn(QPainter &painter)
    {
        painter.fillRect(rect(), QColor(27, 28, 49));
        painter.setPen(QColor(112, 116, 143));
        painter.drawRect(rect().adjusted(1, 1, -2, -2));
        painter.fillRect(QRect(4, 4, width() - 8, 18), QColor(52, 54, 82));
        painter.setPen(QColor(112, 116, 143));
        for (int x = 29; x < width() - 115; x += 3)
            painter.drawLine(x, 8, x + 1, 17);
        painter.setPen(QColor(224, 226, 232));
        painter.setFont(QFont(QStringLiteral("Sans Serif"), 8, QFont::Bold));
        painter.drawText(QRect(0, 4, width(), 18), Qt::AlignCenter, QStringLiteral("LLAMAAMP"));
        drawFallbackButton(painter, QRect(width() - 70, 6, 16, 13), QStringLiteral("−"));
        drawFallbackButton(painter, QRect(width() - 52, 6, 16, 13),
                           m_windowShaded ? QStringLiteral("∨") : QStringLiteral("∧"));
        drawFallbackButton(painter, QRect(width() - 34, 6, 16, 13), QStringLiteral("×"));

        if (m_windowShaded) {
            painter.fillRect(QRect(5, 23, width() - 10, 10), QColor(3, 5, 12));
            painter.setPen(QColor(52, 255, 55));
            painter.setFont(QFont(QStringLiteral("Monospace"), 7, QFont::Bold));
            painter.drawText(QRect(10, 20, 48, 16), Qt::AlignVCenter, formatTime(m_position));
            painter.drawText(QRect(62, 20, width() - 190, 16),
                             Qt::AlignVCenter, m_title);
            const int x = width() - 117;
            drawFallbackButton(painter, QRect(x, 22, 20, 12), QStringLiteral("|<"));
            drawFallbackButton(painter, QRect(x + 21, 22, 20, 12),
                               m_playing ? QStringLiteral("||") : QStringLiteral(">"));
            drawFallbackButton(painter, QRect(x + 42, 22, 20, 12), QStringLiteral("[]"));
            drawFallbackButton(painter, QRect(x + 63, 22, 20, 12), QStringLiteral(">|"));
            return;
        }

        const QRect analyzer(13, 31, 151, 74);
        painter.fillRect(analyzer, QColor(2, 5, 13));
        painter.setPen(QColor(66, 72, 96));
        painter.drawRect(analyzer);
        painter.setPen(QColor(28, 74, 45));
        for (int x = analyzer.left() + 8; x < analyzer.right(); x += 9)
            painter.drawLine(x, analyzer.top() + 28, x, analyzer.bottom() - 5);
        for (int y = analyzer.top() + 34; y < analyzer.bottom(); y += 8)
            painter.drawLine(analyzer.left() + 4, y, analyzer.right() - 4, y);
        for (int band = 0; band < 15; ++band) {
            const int barHeight = 8 + int((band * 17 + m_position / 80) % 43);
            const int x = analyzer.left() + 10 + band * 9;
            painter.fillRect(QRect(x, analyzer.bottom() - 5 - barHeight, 5, barHeight),
                             QColor(30, 238, 45));
            if (barHeight > 34)
                painter.fillRect(QRect(x, analyzer.bottom() - 5 - barHeight, 5, 4),
                                 QColor(246, 222, 43));
        }
        painter.setPen(QColor(50, 255, 61));
        painter.setFont(QFont(QStringLiteral("Monospace"), 7, QFont::Bold));
        painter.drawText(QRect(20, 38, 32, 16), Qt::AlignCenter,
                         m_playing ? QStringLiteral("▶") : QStringLiteral("■"));
        painter.setFont(QFont(QStringLiteral("Monospace"), 17, QFont::Bold));
        painter.drawText(QRect(52, 34, 103, 30), Qt::AlignCenter, formatTime(m_position));

        const QRect titleDisplay(174, 32, width() - 187, 27);
        painter.fillRect(titleDisplay, QColor(2, 5, 12));
        painter.setPen(QColor(54, 255, 58));
        painter.setFont(QFont(QStringLiteral("Monospace"), 10, QFont::Bold));
        painter.drawText(titleDisplay.adjusted(8, 0, -5, 0),
                         Qt::AlignVCenter, m_title);
        painter.setFont(QFont(QStringLiteral("Monospace"), 9, QFont::Bold));
        painter.drawText(QRect(181, 66, 55, 20), Qt::AlignCenter, QStringLiteral("192"));
        painter.drawText(QRect(245, 66, 47, 20), Qt::AlignCenter, QStringLiteral("44"));
        painter.setPen(QColor(215, 217, 224));
        painter.setFont(QFont(QStringLiteral("Sans Serif"), 7));
        painter.drawText(QRect(230, 66, 30, 20), Qt::AlignCenter, QStringLiteral("kbps"));
        painter.drawText(QRect(284, 66, 30, 20), Qt::AlignCenter, QStringLiteral("kHz"));
        painter.drawText(QRect(width() - 91, 66, 38, 20), Qt::AlignCenter,
                         QStringLiteral("mono"));
        painter.setPen(QColor(45, 255, 55));
        painter.drawText(QRect(width() - 51, 66, 38, 20), Qt::AlignCenter,
                         QStringLiteral("stereo"));
        painter.setPen(QColor(232, 213, 45));
        painter.drawText(QRect(15, 108, 90, 15), Qt::AlignVCenter, m_details);

        painter.setPen(QColor(4, 7, 15));
        painter.drawLine(20, 128, width() - 20, 128);
        painter.setPen(QColor(201, 185, 57));
        int seekX = 20;
        if (m_duration > 0)
            seekX += int((width() - 40) * m_position / m_duration);
        painter.drawLine(20, 128, seekX, 128);
        drawFallbackButton(painter, QRect(seekX - 5, 123, 12, 11), QString{});

        const int controlsY = 143;
        drawFallbackButton(painter, QRect(24, controlsY, 37, 29), QStringLiteral("|<"));
        drawFallbackButton(painter, QRect(64, controlsY, 37, 29), QStringLiteral(">"));
        drawFallbackButton(painter, QRect(104, controlsY, 37, 29), QStringLiteral("||"));
        drawFallbackButton(painter, QRect(144, controlsY, 37, 29), QStringLiteral("[]"));
        drawFallbackButton(painter, QRect(184, controlsY, 37, 29), QStringLiteral(">|"));
        drawFallbackButton(painter, QRect(225, controlsY, 37, 29), QStringLiteral("^"));
        drawFallbackButton(painter, QRect(274, controlsY + 3, 72, 23),
                           QStringLiteral("SHUFFLE"), m_shuffle);
        drawFallbackButton(painter, QRect(350, controlsY + 3, 42, 23),
                           m_repeatMode == 2 ? QStringLiteral("R1") : QStringLiteral("REP"),
                           m_repeatMode != 0);
        painter.setPen(QColor(16, 18, 29));
        painter.drawLine(178, 98, 266, 98);
        painter.setPen(QColor(53, 255, 62));
        painter.drawLine(178, 98, 178 + 88 * m_volume / 100, 98);
        drawFallbackButton(painter, QRect(174 + 78 * m_volume / 100, 92, 15, 13), QString{});
        drawFallbackButton(painter, QRect(width() - 86, 91, 35, 19), QStringLiteral("EQ"));
        drawFallbackButton(painter, QRect(width() - 47, 91, 35, 19), QStringLiteral("PL"));
    }

    bool drawRuntimeBitmap(QPainter &painter, const QString &id, const QRect &destination)
    {
        if (!m_runtime)
            return false;
        const WasabiBitmapDefinition *definition = m_runtime->bitmap(id);
        if (!definition || definition->file.isEmpty() || definition->file.startsWith(QLatin1Char('$')))
            return false;
        auto found = m_runtimeImages.find(definition->file);
        if (found == m_runtimeImages.end())
            found = m_runtimeImages.insert(definition->file, QImage(definition->file));
        if (found->isNull())
            return false;
        QRect source = definition->sourceRect;
        if (source.width() <= 0) source.setWidth(found->width() - source.x());
        if (source.height() <= 0) source.setHeight(found->height() - source.y());
        painter.drawImage(destination, *found, source);
        return true;
    }

    static void drawFallbackButton(QPainter &painter, const QRect &rect, const QString &label,
                                   bool active = false)
    {
        painter.fillRect(rect, active ? QColor(61, 91, 100) : QColor(53, 59, 62));
        painter.setPen(QColor(112, 122, 126));
        painter.drawLine(rect.topLeft(), rect.topRight());
        painter.drawLine(rect.topLeft(), rect.bottomLeft());
        painter.setPen(QColor(20, 24, 26));
        painter.drawLine(rect.bottomLeft(), rect.bottomRight());
        painter.drawLine(rect.topRight(), rect.bottomRight());
        painter.setPen(active ? QColor(191, 231, 241) : QColor(211, 217, 219));
        painter.setFont(QFont(QStringLiteral("Sans Serif"), 6, QFont::Bold));
        painter.drawText(rect, Qt::AlignCenter, label);
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
    bool m_windowShaded = false;
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
    m_pageTitle = new QLabel(QStringLiteral("LLAMAAMP PLAYLIST"), this);
    m_pageTitle->setObjectName(QStringLiteral("classicPanelTitle"));
    m_pageTitle->setAlignment(Qt::AlignCenter);
    m_pageTitle->setFixedHeight(18);
    root->addWidget(m_pageTitle);

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
    m_tabs->tabBar()->hide();
    root->addWidget(m_tabs, 1);
    m_equalizer = new ClassicEqualizerWidget(m_player, this);
    root->addWidget(m_equalizer);

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
        case WasabiDeck::Action::WindowShade:
            setWindowShaded(!m_windowShaded); break;
        case WasabiDeck::Action::ToggleEqualizer:
            m_equalizer->setVisible(!m_equalizer->isVisible()); break;
        case WasabiDeck::Action::ShowPlaylist:
            setPage(0); break;
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
    m_pageTitle->hide();
    m_tabs->tabBar()->show();
    m_equalizer->hide();
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
    if (m_tabs && page >= 0 && page < m_tabs->count()) {
        if (m_windowShaded)
            setWindowShaded(false);
        m_tabs->setCurrentIndex(page);
        const QStringList titles = {
            QStringLiteral("LLAMAAMP PLAYLIST"), QStringLiteral("LLAMAAMP MEDIA LIBRARY"),
            QStringLiteral("LLAMAAMP VIDEO"), QStringLiteral("LLAMAAMP VISUALIZATION"),
            QStringLiteral("LLAMAAMP BROWSER")};
        m_pageTitle->setText(titles.at(page));
    }
}

void WasabiPlayerWidget::useBuiltInSkin()
{
    m_runtime = WasabiRuntime{};
    setToolTip(tr("Built-in classic interface"));
    m_deck->setRuntime(nullptr);
    m_tabs->tabBar()->hide();
    if (!m_windowShaded) {
        m_pageTitle->show();
        m_equalizer->show();
    }
}

void WasabiPlayerWidget::setWindowShaded(bool shaded)
{
    if (m_windowShaded == shaded)
        return;
    m_windowShaded = shaded;
    m_tabs->setVisible(!shaded);
    m_pageTitle->setVisible(!shaded && !m_tabs->tabBar()->isVisible());
    m_equalizer->setVisible(!shaded && !m_tabs->tabBar()->isVisible());
    m_deck->setWindowShaded(shaded);
    emit windowShadeChanged(shaded);
}

bool WasabiPlayerWidget::isWindowShaded() const { return m_windowShaded; }

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
