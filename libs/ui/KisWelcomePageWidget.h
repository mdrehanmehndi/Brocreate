/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2018 Scott Petrovic <scottpetrovic@gmail.com>
 * SPDX-FileCopyrightText: 2025 Brocreate Tablet Edition
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISWELCOMEPAGEWIDGET_H
#define KISWELCOMEPAGEWIDGET_H

#include "kritaui_export.h"
#include "KisViewManager.h"
#include <KisUpdaterBase.h>
#include <KisKineticScroller.h>

#include <QAction>
#include <QWidget>
#include "ui_KisWelcomePage.h"
#include <QStandardItemModel>
#include <QScopedPointer>
#include <QFont>
#include <QVector>

#include "config-updaters.h"

class RecentItemDelegate;
class KisMainWindow;
class QScrollArea;
class QLineEdit;
class QSlider;
class QPushButton;
class QToolButton;
class QButtonGroup;
class QFrame;

// Custom QAction to bridge a QLabel::linkActivated signal to a QAction::setChecked signal
class ShowNewsAction : public QAction
{
    Q_OBJECT
public:
    using QAction::QAction;
private Q_SLOTS:
    void enableFromLink(QString unused_url);
};

/// Rounded artwork / "Make Something new" Bento card widget for the Brocreate Gallery
class KRITAUI_EXPORT BrocreateBentoCard : public QWidget
{
    Q_OBJECT
public:
    enum CardType {
        NewDocumentCard,
        ArtworkCard
    };

    explicit BrocreateBentoCard(CardType type, QWidget *parent = nullptr);

    void setDocumentData(const QString &title, const QString &filePath, const QPixmap &thumbnail);
    void setListMode(bool listMode);
    QString filePath() const { return m_filePath; }
    QString title() const { return m_title; }
    CardType cardType() const { return m_cardType; }

Q_SIGNALS:
    void clicked(const QString &filePath);
    void contextMenuRequested(const QPoint &globalPos, const QString &filePath, const QString &title);

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;

private:
    CardType m_cardType {ArtworkCard};
    QString m_title;
    QString m_filePath;
    QPixmap m_thumbnail;
    bool m_hovered {false};
    bool m_pressed {false};
    bool m_listMode {false};
};

/// Masonry / Grid / List viewport container for Brocreate Welcome Bento Gallery
class KRITAUI_EXPORT BrocreateMasonryContainer : public QWidget
{
    Q_OBJECT
public:
    enum ViewMode {
        BentoMasonry = 0,
        ListMode = 1,
        UniformGrid = 2
    };

    explicit BrocreateMasonryContainer(QWidget *parent = nullptr);

    void setViewMode(ViewMode mode);
    ViewMode viewMode() const { return m_viewMode; }
    void setCardScalePercent(int percent);
    void setCards(const QVector<BrocreateBentoCard *> &cards);
    void relayoutCards();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    ViewMode m_viewMode {BentoMasonry};
    int m_cardScalePercent {100};
    QVector<BrocreateBentoCard *> m_cards;
};

/// A widget for displaying the Brocreate Welcome Bento Gallery when no documents are open.
class KRITAUI_EXPORT KisWelcomePageWidget : public QWidget, public Ui::KisWelcomePage
{
    Q_OBJECT

public:
    explicit KisWelcomePageWidget(QWidget *parent);
    ~KisWelcomePageWidget() override;

    void setMainWindow(KisMainWindow *m_mainWindow);

public Q_SLOTS:
    /// if a document is placed over this area, a dotted line will appear as an indicator
    /// that it is a droppable area. KisMainWindow is what triggers this
    void showDropAreaIndicator(bool show);

    void slotUpdateThemeColors();

#ifdef ENABLE_UPDATERS
    void slotSetUpdateStatus(KisUpdaterStatus updateStatus);
    void slotShowUpdaterErrorDetails();
#endif

private Q_SLOTS:
    void slotNewFileClicked();
    void slotOpenFileClicked();
    void slotImportFileClicked();
    void slotSettingsClicked();
    void slotPaste();

    void newsStateChanged(bool hasNews);

    void recentDocumentClicked(QModelIndex index);
    void slotRecentDocContextMenuRequest(const QPoint &pos);

    /**
     * Once all files in the recent documents model are checked, cleanup the UI if the model is empty
     */
    void slotRecentFilesModelIsUpToDate();

    void recentFilesResized();
    void rebuildBentoGallery();
    void slotViewModeChanged(int id);
    void slotScaleSliderChanged(int value);
    void slotSearchTextChanged(const QString &text);
    void slotBentoCardClicked(const QString &filePath);
    void slotBentoCardContextMenu(const QPoint &globalPos, const QString &filePath, const QString &title);

    void slotScrollerStateChanged(QScroller::State state) { KisKineticScroller::updateCursor(this, state); }

#ifdef ENABLE_UPDATERS
    void slotRunVersionUpdate();
    void slotToggleUpdateChecks(bool state);
#endif

#ifdef Q_OS_ANDROID
    void slotUpdateDonationState();
#endif

    bool isDevelopmentBuild();

    QFont largerFont();

protected:
    // QWidget overrides
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void changeEvent(QEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupBentoGalleryUi();
    void setupNewsLangSelection(QMenu *newsOptionMenu);
    void showDevVersionHighlight();

    static void updateShortcutLink(QToolButton *button, QLabel *label, QAction *action);

#ifdef ENABLE_UPDATERS
    void updateVersionUpdaterFrame();
#endif

#ifdef Q_OS_ANDROID
    void initDonations();
#endif

    KisMainWindow *m_mainWindow {nullptr};

    const QString analyticsString = "pk_campaign=startup-sceen&pk_kwd=";

    // Keeping track of link colors with theme change
    QColor textColor;
    QColor backgroundColor;
    QColor blendedColor;
    QString blendedStyle;

#ifdef ENABLE_UPDATERS
    QScopedPointer<KisUpdaterBase> m_versionUpdater;
    KisUpdaterStatus m_updaterStatus;
#endif
    bool m_networkIsAllowed {false};

    QScopedPointer<RecentItemDelegate> recentItemDelegate;

    // Brocreate Welcome Bento Gallery widgets
    QWidget *m_bentoRoot {nullptr};
    QFrame *m_leftFloatingPill {nullptr};
    QToolButton *m_pillBtnImport {nullptr};
    QToolButton *m_pillBtnOpen {nullptr};
    QToolButton *m_pillBtnSettings {nullptr};
    QToolButton *m_pillBtnPaste {nullptr};

    QFrame *m_viewSwitcherPill {nullptr};
    QButtonGroup *m_viewModeGroup {nullptr};
    QToolButton *m_btnBentoMode {nullptr};
    QToolButton *m_btnListMode {nullptr};
    QToolButton *m_btnGridMode {nullptr};
    QLineEdit *m_searchEdit {nullptr};
    QSlider *m_scaleSlider {nullptr};
    QScrollArea *m_bentoScrollArea {nullptr};
    BrocreateMasonryContainer *m_masonryContainer {nullptr};
    QFrame *m_bentoDropOverlay {nullptr};
    QVector<BrocreateBentoCard *> m_bentoCards;
    QString m_searchFilter;
    bool m_dropIndicatorVisible {false};
};

#endif // KISWELCOMEPAGEWIDGET_H
