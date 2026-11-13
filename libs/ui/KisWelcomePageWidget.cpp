/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2018 Scott Petrovic <scottpetrovic@gmail.com>
 * SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 * SPDX-FileCopyrightText: 2025 Brocreate Tablet Edition
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KisWelcomePageWidget.h"
#include "KisRecentDocumentsModelWrapper.h"
#include <QDesktopServices>
#include <QMimeData>
#include <QPixmap>
#include <QMessageBox>
#include <QTemporaryFile>
#include <QBuffer>
#include <QNetworkAccessManager>
#include <QEventLoop>
#include <QDomDocument>

#include "KisRemoteFileFetcher.h"
#include "kactioncollection.h"
#include "kis_action.h"
#include "kis_action_manager.h"
#include "dialogs/KisDlgCreateNewDocument.h"
#include <KisMimeDatabase.h>
#include <KisApplication.h>

#include "KConfigGroup"
#include "KSharedConfig"

#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QScrollBar>
#include <QScrollArea>
#include <QLineEdit>
#include <QSlider>
#include <QToolButton>
#include <QButtonGroup>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QContextMenuEvent>
#include <QResizeEvent>
#include <QFrame>

#include "kis_clipboard.h"
#include "kis_icon_utils.h"
#include <kis_painting_tweaks.h>
#include "KoStore.h"
#include "kis_config.h"
#include "KisDocument.h"
#include <kis_image.h>
#include <kis_paint_device.h>
#include <KisPart.h>
#include <KisKineticScroller.h>
#include "KisMainWindow.h"

#include <utils/KisUpdaterBase.h>

#include <QCoreApplication>
#include <kis_debug.h>
#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <array>
#include <cmath>

#include "config-updaters.h"

#ifdef ENABLE_UPDATERS
#ifdef Q_OS_LINUX
#include <utils/KisAppimageUpdater.h>
#endif

#include <utils/KisManualUpdater.h>
#endif

#include <klocalizedstring.h>
#include <KritaVersionWrapper.h>

#include <KisUsageLogger.h>
#include <QSysInfo>
#include <kis_image_config.h>
#include "opengl/kis_opengl.h"

#ifdef Q_OS_WIN
#include <KisWindowsPackageUtils.h>
#endif

#ifdef Q_OS_MACOS
#include "libs/macosutils/KisMacosEntitlements.h"
#endif

#ifdef Q_OS_ANDROID
#include "KisAndroidDonations.h"
#include <QFontMetrics>
#include <QPaintDevice>
#include <QRandomGenerator>
#endif

// Used for triggering a QAction::setChecked signal from a QLabel::linkActivated signal
void ShowNewsAction::enableFromLink(QString unused_url)
{
    Q_UNUSED(unused_url);
    Q_EMIT setChecked(true);
}

// class to override item height for Breeze since qss seems to not work
class RecentItemDelegate : public QStyledItemDelegate
{
    int itemHeight = 0;
public:
    explicit RecentItemDelegate(QObject *parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void setItemHeight(int height)
    {
        this->itemHeight = height;
    }

    QSize sizeHint(const QStyleOptionViewItem &option, const QModelIndex &/*index*/) const override
    {
        return QSize(option.rect.width(), itemHeight);
    }
};

// ============================================================================
// BrocreateBentoCard
// ============================================================================

BrocreateBentoCard::BrocreateBentoCard(CardType type, QWidget *parent)
    : QWidget(parent)
    , m_cardType(type)
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
}

void BrocreateBentoCard::setDocumentData(const QString &title, const QString &filePath, const QPixmap &thumbnail)
{
    m_title = title;
    m_filePath = filePath;
    m_thumbnail = thumbnail;
    setToolTip(filePath);
    update();
}

void BrocreateBentoCard::setListMode(bool listMode)
{
    m_listMode = listMode;
    update();
}

void BrocreateBentoCard::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal radius = m_listMode ? 12.0 : 16.0;

    QPainterPath clipPath;
    clipPath.addRoundedRect(r, radius, radius);

    if (m_cardType == NewDocumentCard) {
        QColor bg("#232327");
        if (m_pressed) {
            bg = QColor("#1C1C20");
        } else if (m_hovered) {
            bg = QColor("#2B2B31");
        }
        p.fillPath(clipPath, bg);

        p.setPen(QPen(m_hovered ? QColor("#0066FF") : QColor("#2E2E34"), 1.2));
        p.drawPath(clipPath);

        // Centered circular '+' icon and "Make Something new" label
        const QPointF center = m_listMode
            ? QPointF(36.0, r.center().y())
            : QPointF(r.center().x(), r.center().y() - 14.0);
        const qreal circleRadius = m_listMode ? 14.0 : 20.0;

        p.setPen(QPen(QColor("#8E8E98"), 1.8));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(center, circleRadius, circleRadius);

        const qreal arm = circleRadius * 0.45;
        p.drawLine(QPointF(center.x() - arm, center.y()), QPointF(center.x() + arm, center.y()));
        p.drawLine(QPointF(center.x(), center.y() - arm), QPointF(center.x(), center.y() + arm));

        QFont labelFont = font();
        labelFont.setPixelSize(m_listMode ? 14 : 13);
        labelFont.setWeight(QFont::Medium);
        p.setFont(labelFont);
        p.setPen(QColor("#9E9EA8"));

        if (m_listMode) {
            QRectF textRect(70.0, r.top(), r.width() - 86.0, r.height());
            p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, i18n("Make Something new"));
        } else {
            QRectF textRect(r.left() + 12.0, center.y() + circleRadius + 10.0, r.width() - 24.0, 26.0);
            p.drawText(textRect, Qt::AlignHCenter | Qt::AlignTop, i18n("Make Something new"));
        }
        return;
    }

    // ArtworkCard
    p.save();
    p.setClipPath(clipPath);
    p.fillRect(r, QColor("#1E1E22"));

    if (m_listMode) {
        // Horizontal row: rounded thumbnail on left, title & path on right
        const qreal thumbSize = qMax(36.0, r.height() - 16.0);
        const QRectF thumbRect(r.left() + 10.0, r.top() + 8.0, thumbSize * 1.45, thumbSize);
        QPainterPath thumbClip;
        thumbClip.addRoundedRect(thumbRect, 8.0, 8.0);
        p.save();
        p.setClipPath(thumbClip);
        if (!m_thumbnail.isNull()) {
            QPixmap scaled = m_thumbnail.scaled(thumbRect.size().toSize(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            const qreal sx = (scaled.width() - thumbRect.width()) * 0.5;
            const qreal sy = (scaled.height() - thumbRect.height()) * 0.5;
            p.drawPixmap(thumbRect.topLeft(), scaled, QRectF(sx, sy, thumbRect.width(), thumbRect.height()));
        } else {
            p.fillRect(thumbRect, QColor("#27272C"));
        }
        p.restore();

        QFont titleFont = font();
        titleFont.setPixelSize(14);
        titleFont.setWeight(QFont::DemiBold);
        p.setFont(titleFont);
        p.setPen(QColor("#F4F4F6"));
        const qreal textX = thumbRect.right() + 16.0;
        const QRectF titleRect(textX, r.top() + 12.0, r.width() - textX - 16.0, 22.0);
        p.drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(m_title, Qt::ElideRight, int(titleRect.width())));

        QFont pathFont = font();
        pathFont.setPixelSize(11);
        p.setFont(pathFont);
        p.setPen(QColor("#8E8E98"));
        const QRectF pathRect(textX, r.top() + 36.0, r.width() - textX - 16.0, 18.0);
        p.drawText(pathRect, Qt::AlignLeft | Qt::AlignVCenter,
                   p.fontMetrics().elidedText(m_filePath, Qt::ElideMiddle, int(pathRect.width())));
    } else {
        // Bento / Grid artwork card: full-bleed rounded artwork
        if (!m_thumbnail.isNull()) {
            QPixmap scaled = m_thumbnail.scaled(r.size().toSize(), Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
            const qreal sx = (scaled.width() - r.width()) * 0.5;
            const qreal sy = (scaled.height() - r.height()) * 0.5;
            p.drawPixmap(r.topLeft(), scaled, QRectF(sx, sy, r.width(), r.height()));
        } else {
            p.fillRect(r, QColor("#25252A"));
            p.setPen(QColor("#8E8E98"));
            p.drawText(r, Qt::AlignCenter, m_title);
        }

        if (m_hovered || m_pressed) {
            QLinearGradient grad(r.left(), r.bottom() - 56.0, r.left(), r.bottom());
            grad.setColorAt(0.0, QColor(12, 12, 14, 0));
            grad.setColorAt(1.0, QColor(12, 12, 14, 215));
            p.fillRect(QRectF(r.left(), r.bottom() - 56.0, r.width(), 56.0), grad);

            QFont titleFont = font();
            titleFont.setPixelSize(12);
            titleFont.setWeight(QFont::DemiBold);
            p.setFont(titleFont);
            p.setPen(QColor("#FFFFFF"));
            QRectF labelRect(r.left() + 14.0, r.bottom() - 32.0, r.width() - 28.0, 22.0);
            p.drawText(labelRect, Qt::AlignLeft | Qt::AlignVCenter,
                       p.fontMetrics().elidedText(m_title, Qt::ElideRight, int(labelRect.width())));
        }
    }
    p.restore();

    if (m_hovered) {
        p.setPen(QPen(QColor("#0066FF"), 2.0));
        p.drawPath(clipPath);
    } else {
        p.setPen(QPen(QColor("#27272A"), 1.0));
        p.drawPath(clipPath);
    }
}

void BrocreateBentoCard::enterEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_hovered = true;
    update();
}

void BrocreateBentoCard::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    m_hovered = false;
    m_pressed = false;
    update();
}

void BrocreateBentoCard::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_pressed = true;
        update();
    }
    QWidget::mousePressEvent(event);
}

void BrocreateBentoCard::mouseReleaseEvent(QMouseEvent *event)
{
    const bool wasPressed = m_pressed;
    m_pressed = false;
    update();
    if (wasPressed && event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        Q_EMIT clicked(m_filePath);
    }
    QWidget::mouseReleaseEvent(event);
}

void BrocreateBentoCard::contextMenuEvent(QContextMenuEvent *event)
{
    if (m_cardType == ArtworkCard) {
        Q_EMIT contextMenuRequested(event->globalPos(), m_filePath, m_title);
    }
}

// ============================================================================
// BrocreateMasonryContainer
// ============================================================================

BrocreateMasonryContainer::BrocreateMasonryContainer(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
}

void BrocreateMasonryContainer::setViewMode(ViewMode mode)
{
    m_viewMode = mode;
    for (BrocreateBentoCard *card : qAsConst(m_cards)) {
        card->setListMode(m_viewMode == ListMode);
    }
    relayoutCards();
}

void BrocreateMasonryContainer::setCardScalePercent(int percent)
{
    m_cardScalePercent = qBound(60, percent, 150);
    relayoutCards();
}

void BrocreateMasonryContainer::setCards(const QVector<BrocreateBentoCard *> &cards)
{
    m_cards = cards;
    for (BrocreateBentoCard *card : qAsConst(m_cards)) {
        card->setListMode(m_viewMode == ListMode);
        card->show();
    }
    relayoutCards();
}

void BrocreateMasonryContainer::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    relayoutCards();
}

void BrocreateMasonryContainer::relayoutCards()
{
    const int availW = qMax(260, width());
    const int gap = 14;

    if (m_cards.isEmpty()) {
        setMinimumHeight(240);
        return;
    }

    if (m_viewMode == ListMode) {
        int y = 0;
        const int rowH = 74;
        for (BrocreateBentoCard *card : qAsConst(m_cards)) {
            card->setGeometry(0, y, availW, rowH);
            y += rowH + 10;
        }
        setMinimumHeight(qMax(240, y + 16));
        return;
    }

    const int targetColWidth = qRound(260.0 * (m_cardScalePercent / 100.0));
    const int colCount = qBound(2, availW / qMax(180, targetColWidth), 5);
    const int colWidth = (availW - gap * (colCount - 1)) / colCount;

    if (m_viewMode == UniformGrid) {
        const int cardH = qRound(colWidth * 0.68);
        int maxY = 0;
        for (int i = 0; i < m_cards.size(); ++i) {
            const int col = i % colCount;
            const int row = i / colCount;
            const int x = col * (colWidth + gap);
            const int y = row * (cardH + gap);
            m_cards[i]->setGeometry(x, y, colWidth, cardH);
            maxY = qMax(maxY, y + cardH);
        }
        setMinimumHeight(qMax(260, maxY + 24));
        return;
    }

    // BentoMasonry: shortest-column placement with organic Bento card proportions
    static const qreal bentoAspectRatios[] = { 0.62, 0.72, 0.96, 0.66, 0.88, 1.05, 0.74 };
    QVector<int> colHeights(colCount, 0);

    for (int i = 0; i < m_cards.size(); ++i) {
        int targetCol = 0;
        for (int c = 1; c < colCount; ++c) {
            if (colHeights[c] < colHeights[targetCol]) {
                targetCol = c;
            }
        }

        int cardH = 180;
        if (m_cards[i]->cardType() == BrocreateBentoCard::NewDocumentCard) {
            cardH = qRound(colWidth * 0.62);
        } else {
            const qreal ratio = bentoAspectRatios[i % (sizeof(bentoAspectRatios) / sizeof(bentoAspectRatios[0]))];
            cardH = qRound(colWidth * ratio);
        }

        const int x = targetCol * (colWidth + gap);
        const int y = colHeights[targetCol];
        m_cards[i]->setGeometry(x, y, colWidth, cardH);
        colHeights[targetCol] = y + cardH + gap;
    }

    int maxH = 0;
    for (int h : qAsConst(colHeights)) {
        maxH = qMax(maxH, h);
    }
    setMinimumHeight(qMax(280, maxH + 24));
}

// ============================================================================
// KisWelcomePageWidget
// ============================================================================

KisWelcomePageWidget::KisWelcomePageWidget(QWidget *parent)
    : QWidget(parent)
{
    setupUi(this);

    // Keep legacy Ui::KisWelcomePage widgets intact in memory but hidden
    // so we preserve API compatibility while displaying the Brocreate Bento Gallery.
    if (layout()) {
        for (int i = 0; i < layout()->count(); ++i) {
            if (QWidget *w = layout()->itemAt(i)->widget()) {
                w->hide();
            }
        }
    }

    devBuildIcon->setIcon(KisIconUtils::loadIcon("warning"));
    devBuildLabel->setVisible(false);
    updaterFrame->setVisible(false);
    versionNotificationLabel->setVisible(false);
    bnVersionUpdate->setVisible(false);
    bnErrorDetails->setVisible(false);

    // Recent docs legacy list configuration (kept synchronized)
    recentDocumentsListView->setDragEnabled(false);
    recentDocumentsListView->viewport()->setAutoFillBackground(false);
    recentDocumentsListView->setSpacing(2);
    recentDocumentsListView->installEventFilter(this);
    recentDocumentsListView->setViewMode(QListView::IconMode);
    recentDocumentsListView->setSelectionMode(QAbstractItemView::NoSelection);
    recentDocumentsListView->setVerticalScrollMode(QListView::ScrollPerPixel);
    recentDocumentsListView->verticalScrollBar()->setSingleStep(50);

    // News widget...
    QMenu *newsOptionsMenu = new QMenu(this);
    newsOptionsMenu->setToolTipsVisible(true);
    ShowNewsAction *showNewsAction = new ShowNewsAction(i18n("Enable news and check for new releases"), newsOptionsMenu);
    newsOptionsMenu->addAction(showNewsAction);
    showNewsAction->setToolTip(i18n("Show news about Brocreate: this needs internet to retrieve information from the krita.org website"));
    showNewsAction->setCheckable(true);

    newsOptionsMenu->addSection(i18n("Language"));
    QAction *newsInfoAction = newsOptionsMenu->addAction(i18n("English news is always up to date."));
    newsInfoAction->setEnabled(false);

    setupNewsLangSelection(newsOptionsMenu);
    btnNewsOptions->setMenu(newsOptionsMenu);

    labelSupportText->setFont(largerFont());

    connect(btnDismisAll, SIGNAL(clicked()), newsWidget, SLOT(dismissAll()));
    connect(newsWidget, SIGNAL(newsStateChanged(bool)), this, SLOT(newsStateChanged(bool)));
    newsStateChanged(false);

    connect(showNewsAction, SIGNAL(toggled(bool)), newsWidget, SLOT(setVisible(bool)));
    connect(showNewsAction, SIGNAL(toggled(bool)), labelNoFeed, SLOT(setHidden(bool)));
    connect(showNewsAction, SIGNAL(toggled(bool)), newsWidget, SLOT(toggleNews(bool)));
    connect(labelNoFeed, SIGNAL(linkActivated(QString)), showNewsAction, SLOT(enableFromLink(QString)));

#ifdef ENABLE_UPDATERS
    connect(showNewsAction, SIGNAL(toggled(bool)), this, SLOT(slotToggleUpdateChecks(bool)));
#endif

    supporterBadge->hide();
    wdgAndroidSupportBanner->hide();
#ifdef Q_OS_ANDROID
    initDonations();
#endif

    KisConfig cfg(true);
    m_networkIsAllowed = cfg.readEntry<bool>("FetchNews", false);

#ifdef ENABLE_UPDATERS
#ifndef Q_OS_ANDROID
#if defined Q_OS_LINUX
    if (!qEnvironmentVariableIsSet("SteamAppId")) {
        if (qEnvironmentVariableIsSet("APPIMAGE")) {
            m_versionUpdater.reset(new KisAppimageUpdater());
        } else {
            m_versionUpdater.reset(new KisManualUpdater());
        }
    }
#elif defined Q_OS_WIN
    if (!KisWindowsPackageUtils::isRunningInPackage() && !qEnvironmentVariableIsSet("STEAMAPPID")) {
        m_versionUpdater.reset(new KisManualUpdater());
        KisUsageLogger::log("Non-store package - creating updater");
    } else {
        KisUsageLogger::log("detected appx or steam package - not creating the updater");
    }
#else
    m_versionUpdater.reset(new KisManualUpdater());
#endif
    if (!m_versionUpdater.isNull()) {
        connect(bnVersionUpdate, SIGNAL(clicked()), this, SLOT(slotRunVersionUpdate()));
        connect(bnErrorDetails, SIGNAL(clicked()), this, SLOT(slotShowUpdaterErrorDetails()));
        connect(m_versionUpdater.data(), SIGNAL(sigUpdateCheckStateChange(KisUpdaterStatus)),
                this, SLOT(slotSetUpdateStatus(const KisUpdaterStatus&)));

        if (m_networkIsAllowed) {
            m_versionUpdater->checkForUpdate();
        }
    }
#endif
#endif

    showNewsAction->setChecked(m_networkIsAllowed);
    newsWidget->setVisible(m_networkIsAllowed);
    versionNotificationLabel->setEnabled(m_networkIsAllowed);

    setupBentoGalleryUi();
    setAcceptDrops(true);
}

KisWelcomePageWidget::~KisWelcomePageWidget()
{
}

void KisWelcomePageWidget::setupBentoGalleryUi()
{
    setObjectName(QStringLiteral("BrocreateWelcomePage"));
    setAutoFillBackground(false);

    m_bentoRoot = new QWidget(this);
    m_bentoRoot->setObjectName(QStringLiteral("bentoRoot"));
    m_bentoRoot->setStyleSheet(QStringLiteral(
        "QWidget#bentoRoot { background-color: #121214; }"
        "QFrame#leftFloatingPill { background-color: #1E1E22; border: 1px solid #27272A; border-radius: 14px; }"
        "QFrame#leftFloatingPill QToolButton { background: transparent; border: none; border-radius: 10px; padding: 8px; }"
        "QFrame#leftFloatingPill QToolButton:hover { background-color: #2A2A30; }"
        "QFrame#viewSwitcherPill { background-color: #1E1E22; border: 1px solid #27272A; border-radius: 10px; }"
        "QFrame#viewSwitcherPill QToolButton { background: transparent; border: none; border-radius: 7px; padding: 5px 8px; }"
        "QFrame#viewSwitcherPill QToolButton:hover { background-color: #2A2A30; }"
        "QFrame#viewSwitcherPill QToolButton:checked { background-color: #0066FF; }"
        "QLineEdit#bentoSearchEdit { background-color: #1A1A1E; color: #F4F4F6; border: 1px solid #27272A; border-radius: 10px; padding: 6px 12px; font-size: 13px; }"
        "QLineEdit#bentoSearchEdit:focus { border: 1px solid #0066FF; }"
        "QSlider#bentoScaleSlider::groove:horizontal { background: #1E1E22; border: 1px solid #27272A; height: 8px; border-radius: 4px; }"
        "QSlider#bentoScaleSlider::handle:horizontal { background: #121214; border: 2px solid #D4D4D8; width: 12px; height: 12px; margin: -4px 0; border-radius: 7px; }"
        "QScrollArea#bentoScrollArea { background: transparent; border: none; }"
        "QScrollBar:vertical { background: transparent; width: 6px; margin: 4px 0; }"
        "QScrollBar::handle:vertical { background: #6E6E77; border-radius: 3px; min-height: 40px; }"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }"
    ));

    QHBoxLayout *outerLayout = new QHBoxLayout(m_bentoRoot);
    outerLayout->setContentsMargins(18, 28, 36, 20);
    outerLayout->setSpacing(28);

    // Left vertical 4-icon floating pill (matching refs/m4.png)
    QVBoxLayout *leftRailLayout = new QVBoxLayout();
    leftRailLayout->addStretch(1);

    m_leftFloatingPill = new QFrame(m_bentoRoot);
    m_leftFloatingPill->setObjectName(QStringLiteral("leftFloatingPill"));
    m_leftFloatingPill->setFixedWidth(52);
    QVBoxLayout *pillLayout = new QVBoxLayout(m_leftFloatingPill);
    pillLayout->setContentsMargins(6, 10, 6, 10);
    pillLayout->setSpacing(10);

    auto makePillBtn = [this](const QString &iconName, const QString &tip) {
        QToolButton *btn = new QToolButton(m_leftFloatingPill);
        btn->setIcon(KisIconUtils::loadIcon(iconName));
        btn->setIconSize(QSize(22, 22));
        btn->setFixedSize(40, 40);
        btn->setToolTip(tip);
        return btn;
    };

    m_pillBtnImport = makePillBtn(QStringLiteral("document-import"), i18n("Import Artwork"));
    m_pillBtnOpen = makePillBtn(QStringLiteral("folder"), i18n("Open File"));
    m_pillBtnSettings = makePillBtn(QStringLiteral("configure"), i18n("Configure Brocreate"));
    m_pillBtnPaste = makePillBtn(QStringLiteral("edit-paste"), i18n("New from Clipboard"));

    pillLayout->addWidget(m_pillBtnImport);
    pillLayout->addWidget(m_pillBtnOpen);
    pillLayout->addWidget(m_pillBtnSettings);
    pillLayout->addWidget(m_pillBtnPaste);

    connect(m_pillBtnImport, &QToolButton::clicked, this, &KisWelcomePageWidget::slotImportFileClicked);
    connect(m_pillBtnOpen, &QToolButton::clicked, this, &KisWelcomePageWidget::slotOpenFileClicked);
    connect(m_pillBtnSettings, &QToolButton::clicked, this, &KisWelcomePageWidget::slotSettingsClicked);
    connect(m_pillBtnPaste, &QToolButton::clicked, this, &KisWelcomePageWidget::slotPaste);

    leftRailLayout->addWidget(m_leftFloatingPill);
    leftRailLayout->addStretch(1);
    outerLayout->addLayout(leftRailLayout);

    // Right Bento Gallery workspace
    QVBoxLayout *galleryColumn = new QVBoxLayout();
    galleryColumn->setContentsMargins(0, 0, 0, 0);
    galleryColumn->setSpacing(12);

    // Top row: [Bento | List | Grid] switcher pill + Search bar
    QHBoxLayout *topBarLayout = new QHBoxLayout();
    topBarLayout->setSpacing(12);

    m_viewSwitcherPill = new QFrame(m_bentoRoot);
    m_viewSwitcherPill->setObjectName(QStringLiteral("viewSwitcherPill"));
    QHBoxLayout *switcherLayout = new QHBoxLayout(m_viewSwitcherPill);
    switcherLayout->setContentsMargins(4, 3, 4, 3);
    switcherLayout->setSpacing(3);

    m_btnBentoMode = new QToolButton(m_viewSwitcherPill);
    m_btnBentoMode->setIcon(KisIconUtils::loadIcon(QStringLiteral("view-grid")));
    m_btnBentoMode->setIconSize(QSize(18, 18));
    m_btnBentoMode->setCheckable(true);
    m_btnBentoMode->setChecked(true);
    m_btnBentoMode->setToolTip(i18n("Bento Masonry View"));

    m_btnListMode = new QToolButton(m_viewSwitcherPill);
    m_btnListMode->setIcon(KisIconUtils::loadIcon(QStringLiteral("view-list-details")));
    m_btnListMode->setIconSize(QSize(18, 18));
    m_btnListMode->setCheckable(true);
    m_btnListMode->setToolTip(i18n("List View"));

    m_btnGridMode = new QToolButton(m_viewSwitcherPill);
    m_btnGridMode->setIcon(KisIconUtils::loadIcon(QStringLiteral("view-preview")));
    m_btnGridMode->setIconSize(QSize(18, 18));
    m_btnGridMode->setCheckable(true);
    m_btnGridMode->setToolTip(i18n("Uniform Grid View"));

    m_viewModeGroup = new QButtonGroup(this);
    m_viewModeGroup->setExclusive(true);
    m_viewModeGroup->addButton(m_btnBentoMode, BrocreateMasonryContainer::BentoMasonry);
    m_viewModeGroup->addButton(m_btnListMode, BrocreateMasonryContainer::ListMode);
    m_viewModeGroup->addButton(m_btnGridMode, BrocreateMasonryContainer::UniformGrid);

    switcherLayout->addWidget(m_btnBentoMode);
    switcherLayout->addWidget(m_btnListMode);
    switcherLayout->addWidget(m_btnGridMode);

    m_searchEdit = new QLineEdit(m_bentoRoot);
    m_searchEdit->setObjectName(QStringLiteral("bentoSearchEdit"));
    m_searchEdit->setPlaceholderText(i18n("Search artworks in Brocreate..."));
    m_searchEdit->setClearButtonEnabled(true);
    m_searchEdit->setMaximumWidth(360);

    topBarLayout->addWidget(m_viewSwitcherPill);
    topBarLayout->addWidget(m_searchEdit);
    topBarLayout->addStretch(1);
    galleryColumn->addLayout(topBarLayout);

    // Sleek full-width card scale slider track matching refs/m4.png
    m_scaleSlider = new QSlider(Qt::Horizontal, m_bentoRoot);
    m_scaleSlider->setObjectName(QStringLiteral("bentoScaleSlider"));
    m_scaleSlider->setRange(65, 145);
    m_scaleSlider->setValue(100);
    m_scaleSlider->setToolTip(i18n("Adjust gallery card size"));
    galleryColumn->addWidget(m_scaleSlider);

    // Scrollable Bento Masonry viewport
    m_bentoScrollArea = new QScrollArea(m_bentoRoot);
    m_bentoScrollArea->setObjectName(QStringLiteral("bentoScrollArea"));
    m_bentoScrollArea->setWidgetResizable(true);
    m_bentoScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_bentoScrollArea->setFrameShape(QFrame::NoFrame);
    m_bentoScrollArea->viewport()->setStyleSheet(QStringLiteral("background: transparent;"));

    m_masonryContainer = new BrocreateMasonryContainer(m_bentoScrollArea);
    m_bentoScrollArea->setWidget(m_masonryContainer);

    if (QScroller *scroller = KisKineticScroller::createPreconfiguredScroller(m_bentoScrollArea)) {
        connect(scroller, SIGNAL(stateChanged(QScroller::State)), this, SLOT(slotScrollerStateChanged(QScroller::State)));
    }

    galleryColumn->addWidget(m_bentoScrollArea, 1);
    outerLayout->addLayout(galleryColumn, 1);

    // Drag-and-drop visual overlay
    m_bentoDropOverlay = new QFrame(m_bentoRoot);
    m_bentoDropOverlay->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    m_bentoDropOverlay->setStyleSheet(QStringLiteral(
        "QFrame { border: 3px dashed #0066FF; border-radius: 18px; background: rgba(0, 102, 255, 22); }"
    ));
    m_bentoDropOverlay->hide();

    // Place m_bentoRoot into the main widget layout
    if (!layout()) {
        QVBoxLayout *rootLayout = new QVBoxLayout(this);
        rootLayout->setContentsMargins(0, 0, 0, 0);
        rootLayout->addWidget(m_bentoRoot);
    } else {
        layout()->setContentsMargins(0, 0, 0, 0);
        layout()->addWidget(m_bentoRoot);
    }

    connect(m_viewModeGroup, SIGNAL(buttonClicked(int)), this, SLOT(slotViewModeChanged(int)));
    connect(m_scaleSlider, &QSlider::valueChanged, this, &KisWelcomePageWidget::slotScaleSliderChanged);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &KisWelcomePageWidget::slotSearchTextChanged);

    rebuildBentoGallery();
}

void KisWelcomePageWidget::setMainWindow(KisMainWindow *mainWin)
{
    if (mainWin) {
        m_mainWindow = mainWin;

        KisActionManager *actionManager = mainWin->viewManager()->actionManager();
        updateShortcutLink(newFileLink, newFileLinkShortcut, actionManager->actionByName(QStringLiteral("file_new")));
        updateShortcutLink(openFileLink, openFileShortcut, actionManager->actionByName(QStringLiteral("file_open")));
        connect(recentDocumentsListView, SIGNAL(clicked(QModelIndex)), this, SLOT(recentDocumentClicked(QModelIndex)));

        connect(newFileLink, SIGNAL(clicked(bool)), this, SLOT(slotNewFileClicked()));
        connect(openFileLink, SIGNAL(clicked(bool)), this, SLOT(slotOpenFileClicked()));
        connect(clearRecentFilesLink, SIGNAL(clicked(bool)), mainWin, SLOT(clearRecentFiles()));

        KisAction *pasteAction = mainWin->viewManager()->actionManager()->actionByName("edit_paste");
        if (pasteAction) {
            connect(pasteAction, SIGNAL(triggered()), this, SLOT(slotPaste()));
        }

        slotUpdateThemeColors();

        Q_EMIT newsWidget->setAnalyticsTracking("?" + analyticsString);

        KisRecentDocumentsModelWrapper *recentFilesModel = KisRecentDocumentsModelWrapper::instance();
        connect(recentFilesModel, SIGNAL(sigModelIsUpToDate()), this, SLOT(slotRecentFilesModelIsUpToDate()));
        connect(&recentFilesModel->model(), &QAbstractItemModel::rowsInserted, this, &KisWelcomePageWidget::rebuildBentoGallery);
        connect(&recentFilesModel->model(), &QAbstractItemModel::rowsRemoved, this, &KisWelcomePageWidget::rebuildBentoGallery);
        connect(&recentFilesModel->model(), &QAbstractItemModel::modelReset, this, &KisWelcomePageWidget::rebuildBentoGallery);
        connect(&recentFilesModel->model(), &QAbstractItemModel::dataChanged, this, &KisWelcomePageWidget::rebuildBentoGallery);

        recentDocumentsListView->setModel(&recentFilesModel->model());
        slotRecentFilesModelIsUpToDate();
    }
}

void KisWelcomePageWidget::rebuildBentoGallery()
{
    if (!m_masonryContainer) {
        return;
    }

    qDeleteAll(m_bentoCards);
    m_bentoCards.clear();

    // First card is always the large "Make Something new" card
    BrocreateBentoCard *newCard = new BrocreateBentoCard(BrocreateBentoCard::NewDocumentCard, m_masonryContainer);
    connect(newCard, &BrocreateBentoCard::clicked, this, [this](const QString &) {
        slotNewFileClicked();
    });
    m_bentoCards.append(newCard);

    KisRecentDocumentsModelWrapper *recentFilesModel = KisRecentDocumentsModelWrapper::instance();
    const QAbstractItemModel &model = recentFilesModel->model();
    const int rows = model.rowCount();

    for (int i = 0; i < rows; ++i) {
        const QModelIndex idx = model.index(i, 0);
        const QString title = idx.data(Qt::DisplayRole).toString();
        const QString fileUrl = idx.data(Qt::ToolTipRole).toString();

        if (!m_searchFilter.isEmpty()) {
            if (!title.contains(m_searchFilter, Qt::CaseInsensitive) &&
                !fileUrl.contains(m_searchFilter, Qt::CaseInsensitive)) {
                continue;
            }
        }

        QPixmap thumb;
        const QVariant dec = idx.data(Qt::DecorationRole);
        if (dec.canConvert<QIcon>()) {
            thumb = dec.value<QIcon>().pixmap(QSize(512, 512));
        } else if (dec.canConvert<QPixmap>()) {
            thumb = dec.value<QPixmap>();
        }

        BrocreateBentoCard *artCard = new BrocreateBentoCard(BrocreateBentoCard::ArtworkCard, m_masonryContainer);
        artCard->setDocumentData(title.isEmpty() ? QFileInfo(fileUrl).baseName() : title, fileUrl, thumb);
        connect(artCard, &BrocreateBentoCard::clicked, this, &KisWelcomePageWidget::slotBentoCardClicked);
        connect(artCard, &BrocreateBentoCard::contextMenuRequested, this, &KisWelcomePageWidget::slotBentoCardContextMenu);
        m_bentoCards.append(artCard);
    }

    m_masonryContainer->setCards(m_bentoCards);
}

void KisWelcomePageWidget::slotViewModeChanged(int id)
{
    if (m_masonryContainer) {
        m_masonryContainer->setViewMode(static_cast<BrocreateMasonryContainer::ViewMode>(id));
    }
}

void KisWelcomePageWidget::slotScaleSliderChanged(int value)
{
    if (m_masonryContainer) {
        m_masonryContainer->setCardScalePercent(value);
    }
}

void KisWelcomePageWidget::slotSearchTextChanged(const QString &text)
{
    m_searchFilter = text.trimmed();
    rebuildBentoGallery();
}

void KisWelcomePageWidget::slotBentoCardClicked(const QString &filePath)
{
    if (m_mainWindow && !filePath.isEmpty()) {
        m_mainWindow->openDocument(filePath, KisMainWindow::None);
    }
}

void KisWelcomePageWidget::slotBentoCardContextMenu(const QPoint &globalPos, const QString &filePath, const QString &title)
{
    QMenu contextMenu(this);
    QAction *actionOpen = contextMenu.addAction(i18n("Open \"%1\"", title));
    QAction *actionForget = contextMenu.addAction(i18n("Forget \"%1\"", title));
    QAction *triggered = contextMenu.exec(globalPos);
    if (triggered == actionOpen && m_mainWindow) {
        m_mainWindow->openDocument(filePath, KisMainWindow::None);
    } else if (triggered == actionForget && m_mainWindow) {
        m_mainWindow->removeRecentFile(filePath);
    }
}

void KisWelcomePageWidget::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter p(this);
    p.fillRect(rect(), QColor("#121214"));
}

void KisWelcomePageWidget::showDropAreaIndicator(bool show)
{
    m_dropIndicatorVisible = show;
    if (m_bentoDropOverlay && m_bentoRoot) {
        m_bentoDropOverlay->setGeometry(m_bentoRoot->rect().adjusted(14, 14, -14, -14));
        m_bentoDropOverlay->setVisible(show);
        if (show) {
            m_bentoDropOverlay->raise();
        }
    }

    if (!show) {
        QString dropFrameStyle = QStringLiteral("QFrame#dropAreaIndicator { border: 2px solid transparent }");
        dropFrameBorder->setStyleSheet(dropFrameStyle);
    } else {
        QString dropFrameStyle = QStringLiteral("QFrame#dropAreaIndicator { border: 2px dotted #0066FF }");
        dropFrameBorder->setStyleSheet(dropFrameStyle);
    }
}

void KisWelcomePageWidget::slotUpdateThemeColors()
{
    textColor = qApp->palette().color(QPalette::Text);
    backgroundColor = qApp->palette().color(QPalette::Window);

    blendedColor = KisPaintingTweaks::blendColors(textColor, backgroundColor, 0.8);
    blendedStyle = "QWidget{color: " + blendedColor.name() + "}";

    startTitleLabel->setStyleSheet(blendedStyle);
    recentDocumentsLabel->setStyleSheet(blendedStyle);
    helpTitleLabel->setStyleSheet(blendedStyle);
    newsTitleLabel->setStyleSheet(blendedStyle);
    newFileLinkShortcut->setStyleSheet(blendedStyle);
    openFileShortcut->setStyleSheet(blendedStyle);
    clearRecentFilesLink->setStyleSheet(blendedStyle);
    recentDocumentsListView->setStyleSheet(blendedStyle);
    newsWidget->setStyleSheet(blendedStyle);

#ifdef Q_OS_ANDROID
    blendedStyle = blendedStyle + "\nQPushButton { padding: 10px }";
#endif

    newFileLink->setStyleSheet(blendedStyle);
    openFileLink->setStyleSheet(blendedStyle);

    dropFrameBorder->setObjectName("dropAreaIndicator");
    showDropAreaIndicator(false);

    openFileLink->setIconSize(QSize(48, 48));
    newFileLink->setIconSize(QSize(48, 48));
    openFileLink->setIcon(KisIconUtils::loadIcon("document-open"));
    newFileLink->setIcon(KisIconUtils::loadIcon("document-new"));

    if (m_pillBtnImport) {
        m_pillBtnImport->setIcon(KisIconUtils::loadIcon(QStringLiteral("document-import")));
        m_pillBtnOpen->setIcon(KisIconUtils::loadIcon(QStringLiteral("folder")));
        m_pillBtnSettings->setIcon(KisIconUtils::loadIcon(QStringLiteral("configure")));
        m_pillBtnPaste->setIcon(KisIconUtils::loadIcon(QStringLiteral("edit-paste")));
    }
    if (m_btnBentoMode) {
        m_btnBentoMode->setIcon(KisIconUtils::loadIcon(QStringLiteral("view-grid")));
        m_btnListMode->setIcon(KisIconUtils::loadIcon(QStringLiteral("view-list-details")));
        m_btnGridMode->setIcon(KisIconUtils::loadIcon(QStringLiteral("view-preview")));
    }

    btnNewsOptions->setIcon(KisIconUtils::loadIcon("view-choose"));
    btnNewsOptions->setFlat(true);
    notifyIcon->setIcon(KisIconUtils::loadIcon("notification-dot"));

    supportKritaIcon->setIcon(KisIconUtils::loadIcon(QStringLiteral("support-krita")));
    userManualIcon->setIcon(KisIconUtils::loadIcon(QStringLiteral("help-contents")));
    gettingStartedIcon->setIcon(KisIconUtils::loadIcon(QStringLiteral("get_started")));
    userCommunityIcon->setIcon(KisIconUtils::loadIcon(QStringLiteral("comunity")));
    kritaWebsiteIcon->setIcon(KisIconUtils::loadIcon(QStringLiteral("website")));
    sourceCodeIcon->setIcon(KisIconUtils::loadIcon(QStringLiteral("code")));
    kdeIcon->setIcon(KisIconUtils::loadIcon(QStringLiteral("kde")));

    userCommunityLink->setText(QString("<a style=\"color: " + blendedColor.name() + " \" href=\"https://krita-artists.org\">")
                               .append(i18n("User Community")).append("</a>"));

    gettingStartedLink->setText(QString("<a style=\"color: " + blendedColor.name() + " \" href=\"https://docs.krita.org/user_manual/getting_started.html\">")
                                .append(i18n("Getting Started")).append("</a>"));

    manualLink->setText(QString("<a style=\"color: " + blendedColor.name() + " \" href=\"https://docs.krita.org\">")
                        .append(i18n("User Manual")).append("</a>"));

    supportKritaLink->setText(QString("<a style=\"color: " + blendedColor.name() + " \" href=\"https://krita.org/support-us/donations?" + analyticsString + "donations" + "\">")
                              .append(i18n("Support Brocreate")).append("</a>"));

    kritaWebsiteLink->setText(QString("<a style=\"color: " + blendedColor.name() + " \" href=\"https://www.krita.org?" + analyticsString + "marketing-site" + "\">")
                              .append(i18n("Brocreate Website")).append("</a>"));

    sourceCodeLink->setText(QString("<a style=\"color: " + blendedColor.name() + " \" href=\"https://invent.kde.org/graphics/krita\">")
                            .append(i18n("Source Code")).append("</a>"));

    poweredByKDELink->setText(QString("<a style=\"color: " + blendedColor.name() + " \" href=\"https://userbase.kde.org/What_is_KDE\">")
                              .append(i18n("Powered by KDE")).append("</a>"));

    QString translationNoFeed = i18n("You can <a href=\"ignored\" style=\"color: COLOR_PLACEHOLDER; text-decoration: underline;\">enable news</a> in various languages with the menu above");
    labelNoFeed->setText(translationNoFeed.replace("COLOR_PLACEHOLDER", blendedColor.name()));

    const QColor faintTextColor = KisPaintingTweaks::blendColors(textColor, backgroundColor, 0.4);
    const QString &faintTextStyle = "QWidget{color: " + faintTextColor.name() + "}";
    labelNoRecentDocs->setStyleSheet(faintTextStyle);
    labelNoFeed->setStyleSheet(faintTextStyle);

    const QColor frameColor = KisPaintingTweaks::blendColors(textColor, backgroundColor, 0.1);
    const QString &frameQss = "{border: 1px solid " + frameColor.name() + "}";
    recentDocsStackedWidget->setStyleSheet("QStackedWidget#recentDocsStackedWidget" + frameQss);
    newsFrame->setStyleSheet("QFrame#newsFrame" + frameQss);

    showDevVersionHighlight();

#ifdef ENABLE_UPDATERS
    updateVersionUpdaterFrame();
#endif

#ifdef Q_OS_MACOS
    if (KisMacosEntitlements().sandbox()) {
        supportKritaLink->hide();
        supportKritaIcon->hide();
        labelSupportText->hide();
        kritaWebsiteLink->hide();
        kritaWebsiteIcon->hide();
    }
#endif
}

void KisWelcomePageWidget::dragEnterEvent(QDragEnterEvent *event)
{
    showDropAreaIndicator(true);
    if (event->mimeData()->hasUrls() ||
        event->mimeData()->hasFormat("application/x-krita-node-internal-pointer") ||
        event->mimeData()->hasFormat("application/x-qt-image")) {
        return event->accept();
    }

    return event->ignore();
}

void KisWelcomePageWidget::dropEvent(QDropEvent *event)
{
    showDropAreaIndicator(false);

    if (event->mimeData()->hasUrls() && !event->mimeData()->urls().empty()) {
        Q_FOREACH (const QUrl &url, event->mimeData()->urls()) {
            if (url.toLocalFile().endsWith(".bundle", Qt::CaseInsensitive)) {
                bool r = m_mainWindow->installBundle(url.toLocalFile());
                if (!r) {
                    qWarning() << "Could not install bundle" << url.toLocalFile();
                }
            } else if (!url.isLocalFile()) {
                QScopedPointer<QTemporaryFile> tmp(new QTemporaryFile());
                tmp->setFileName(url.fileName());

                KisRemoteFileFetcher fetcher;

                if (!fetcher.fetchFile(url, tmp.data())) {
                    qWarning() << "Fetching" << url << "failed";
                    continue;
                }
                const auto localUrl = QUrl::fromLocalFile(tmp->fileName());

                m_mainWindow->openDocument(localUrl.toLocalFile(), KisMainWindow::None);
            } else {
                m_mainWindow->openDocument(url.toLocalFile(), KisMainWindow::None);
            }
        }
    }
}

void KisWelcomePageWidget::dragMoveEvent(QDragMoveEvent *event)
{
    m_mainWindow->dragMoveEvent(event);

    if (event->mimeData()->hasUrls() ||
        event->mimeData()->hasFormat("application/x-krita-node-internal-pointer") ||
        event->mimeData()->hasFormat("application/x-qt-image")) {
        return event->accept();
    }

    return event->ignore();
}

void KisWelcomePageWidget::dragLeaveEvent(QDragLeaveEvent */*event*/)
{
    showDropAreaIndicator(false);
    m_mainWindow->dragLeave();
}

void KisWelcomePageWidget::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::FontChange) {
        labelSupportText->setFont(largerFont());
    }
}

bool KisWelcomePageWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == recentDocumentsListView && event->type() == QEvent::Leave) {
        recentDocumentsListView->clearSelection();
    } else if (watched == recentDocumentsListView && event->type() == QEvent::Resize) {
        recentFilesResized();
    }
    return QWidget::eventFilter(watched, event);
}

namespace {

QString getAutoNewsLang()
{
    const QStringList uiLangs = KLocalizedString::languages();
    QString autoNewsLang = uiLangs.first();
    if (autoNewsLang.isEmpty()) {
        autoNewsLang = QString("en");
    } else if (autoNewsLang == "ja") {
        return QString("jp");
    } else if (autoNewsLang == "zh_CN") {
        return QString("zh");
    } else if (autoNewsLang == "zh_TW") {
        return QString("zh-tw");
    } else if (autoNewsLang == "zh_HK") {
        return QString("zh-hk");
    } else if (autoNewsLang == "en" || autoNewsLang == "en_US" || autoNewsLang == "en_GB") {
        return QString("en");
    }

    return autoNewsLang;
}

} /* namespace */

void KisWelcomePageWidget::setupNewsLangSelection(QMenu *newsOptionsMenu)
{
    struct Lang {
        const QString siteCode;
        const QString name;
    };
    static const std::array<Lang, 22> newsLangs = {{
        {QString("en"), QStringLiteral("English")},
        {QString("jp"), QStringLiteral("日本語")},
        {QString("zh"), QStringLiteral("中文 (简体)")},
        {QString("zh-tw"), QStringLiteral("中文 (台灣正體)")},
        {QString("zh-hk"), QStringLiteral("廣東話 (香港)")},
        {QString("ca"), QStringLiteral("Català")},
        {QString("ca@valencia"), QStringLiteral("Català de Valencia")},
        {QString("cs"), QStringLiteral("Čeština")},
        {QString("de"), QStringLiteral("Deutsch")},
        {QString("eo"), QStringLiteral("Esperanto")},
        {QString("es"), QStringLiteral("Español")},
        {QString("eu"), QStringLiteral("Euskara")},
        {QString("fr"), QStringLiteral("Français")},
        {QString("it"), QStringLiteral("Italiano")},
        {QString("lt"), QStringLiteral("lietuvių")},
        {QString("nl"), QStringLiteral("Nederlands")},
        {QString("pt"), QStringLiteral("Português")},
        {QString("sk"), QStringLiteral("Slovenský")},
        {QString("sl"), QStringLiteral("Slovenski")},
        {QString("sv"), QStringLiteral("Svenska")},
        {QString("tr"), QStringLiteral("Türkçe")},
        {QString("uk"), QStringLiteral("Українська")}
    }};

    static const QString newsLangConfigName = QStringLiteral("FetchNewsLanguages");

    QSharedPointer<QSet<QString>> enabledNewsLangs = QSharedPointer<QSet<QString>>::create();
    {
        KisConfig cfg(true);
        auto languagesList = cfg.readList<QString>(newsLangConfigName);
        *enabledNewsLangs = QSet(languagesList.begin(), languagesList.end());
    }

    if (enabledNewsLangs->isEmpty()) {
        enabledNewsLangs->insert(QString(getAutoNewsLang()));
    }

    for (const auto &lang : newsLangs) {
        QAction *langItem = newsOptionsMenu->addAction(lang.name);
        langItem->setCheckable(true);
        const QString code = lang.siteCode;
        connect(langItem, &QAction::toggled, newsWidget, [=](bool checked) {
            newsWidget->toggleNewsLanguage(code, checked);
        });

        if (enabledNewsLangs->contains(code)) {
            langItem->setChecked(true);
        }

        connect(langItem, &QAction::toggled, qApp, [=](bool checked) {
            KisConfig cfg(false);
            if (checked) {
                enabledNewsLangs->insert(QString(code));
            } else {
                enabledNewsLangs->remove(QString(code));
            }
            cfg.writeList(newsLangConfigName, enabledNewsLangs->values());
        });
    }
}

void KisWelcomePageWidget::showDevVersionHighlight()
{
    if (isDevelopmentBuild()) {
        QString devBuildLabelText = QString("<a style=\"color: " +
                                           blendedColor.name() +
                                           " \" href=\"https://docs.krita.org/en/untranslatable_pages/triaging_bugs.html?"
                                           + analyticsString + "dev-build" + "\">")
                                  .append(i18n("DEV BUILD")).append("</a>");

        devBuildLabel->setText(devBuildLabelText);
        devBuildIcon->setVisible(true);
        devBuildLabel->setVisible(true);
    } else {
        devBuildIcon->setVisible(false);
        devBuildLabel->setVisible(false);
    }
}

void KisWelcomePageWidget::updateShortcutLink(QToolButton *button, QLabel *label, QAction *action)
{
    if (action) {
        QString shortcutText = action->shortcut().toString(QKeySequence::NativeText);
        if (shortcutText.isEmpty()) {
            label->setText(QString());
        } else {
            label->setText(QStringLiteral("(%1)").arg(shortcutText));
        }
        button->show();
        label->show();
    } else {
        button->hide();
        label->hide();
    }
}

void KisWelcomePageWidget::recentDocumentClicked(QModelIndex index)
{
    QString fileUrl = index.data(Qt::ToolTipRole).toString();
    m_mainWindow->openDocument(fileUrl, KisMainWindow::None);
}

void KisWelcomePageWidget::slotRecentDocContextMenuRequest(const QPoint &pos)
{
    QMenu contextMenu;
    QModelIndex index = recentDocumentsListView->indexAt(pos);
    QAction *actionForget = nullptr;
    if (index.isValid()) {
        actionForget = new QAction(i18n("Forget \"%1\"", index.data(Qt::DisplayRole).toString()), &contextMenu);
        contextMenu.addAction(actionForget);
    }
    QAction *triggered = contextMenu.exec(recentDocumentsListView->mapToGlobal(pos));

    if (index.isValid() && triggered == actionForget) {
        m_mainWindow->removeRecentFile(index.data(Qt::ToolTipRole).toString());
    }
}

bool KisWelcomePageWidget::isDevelopmentBuild()
{
    return KritaVersionWrapper::isDevelopersBuild();
}

void KisWelcomePageWidget::slotNewFileClicked()
{
    if (m_mainWindow) {
        m_mainWindow->slotFileNew();
    }
}

void KisWelcomePageWidget::slotOpenFileClicked()
{
    if (m_mainWindow) {
        m_mainWindow->slotFileOpen();
    }
}

void KisWelcomePageWidget::slotImportFileClicked()
{
    if (!m_mainWindow) {
        return;
    }
    if (QAction *act = m_mainWindow->viewManager()->actionManager()->actionByName(QStringLiteral("file_import_file"))) {
        act->trigger();
    } else {
        m_mainWindow->slotFileOpen();
    }
}

void KisWelcomePageWidget::slotSettingsClicked()
{
    if (!m_mainWindow) {
        return;
    }
    if (QAction *act = m_mainWindow->viewManager()->actionManager()->actionByName(QStringLiteral("options_configure"))) {
        act->trigger();
    }
}

void KisWelcomePageWidget::slotPaste()
{
    if (!this->isVisible())
        return;

    if (!KisClipboard::instance()->hasImage())
        return;

    KisDlgCreateNewDocument *dlg = new KisDlgCreateNewDocument(this);
    dlg->SelectPage(KisDlgCreateNewDocument::Page::CreateFromClipboard);
    dlg->exec();
    dlg->deleteLater();
}

void KisWelcomePageWidget::slotRecentFilesModelIsUpToDate()
{
    KisRecentDocumentsModelWrapper *recentFilesModel = KisRecentDocumentsModelWrapper::instance();
    const bool modelIsEmpty = recentFilesModel->model().rowCount() == 0;

    if (modelIsEmpty) {
        recentDocsStackedWidget->setCurrentWidget(labelNoRecentDocs);
    } else {
        recentDocsStackedWidget->setCurrentWidget(recentDocumentsListView);
    }
    clearRecentFilesLink->setVisible(!modelIsEmpty);

    rebuildBentoGallery();
}

#ifdef ENABLE_UPDATERS
void KisWelcomePageWidget::slotToggleUpdateChecks(bool state)
{
    if (m_versionUpdater.isNull()) {
        return;
    }

    m_networkIsAllowed = state;

    if (m_networkIsAllowed) {
        m_versionUpdater->checkForUpdate();
    }

    updateVersionUpdaterFrame();
}

void KisWelcomePageWidget::slotRunVersionUpdate()
{
    if (m_versionUpdater.isNull()) {
        return;
    }

    if (m_networkIsAllowed) {
        m_versionUpdater->doUpdate();
    }
}

void KisWelcomePageWidget::slotSetUpdateStatus(KisUpdaterStatus updateStatus)
{
    m_updaterStatus = updateStatus;
    updateVersionUpdaterFrame();
}

void KisWelcomePageWidget::slotShowUpdaterErrorDetails()
{
    QMessageBox::warning(qApp->activeWindow(), i18nc("@title:window", "Brocreate"), m_updaterStatus.updaterOutput());
}

void KisWelcomePageWidget::updateVersionUpdaterFrame()
{
    updaterFrame->setVisible(false);
    versionNotificationLabel->setVisible(false);
    bnVersionUpdate->setVisible(false);
    bnErrorDetails->setVisible(false);

    if (!m_networkIsAllowed || m_versionUpdater.isNull()) {
        return;
    }

    QString versionLabelText;

    if (m_updaterStatus.status() == UpdaterStatus::StatusID::UPDATE_AVAILABLE) {
        updaterFrame->setVisible(true);
        updaterFrame->setEnabled(true);
        versionLabelText = i18n("New version of Brocreate is available.");
        versionNotificationLabel->setVisible(true);
        updateIcon->setIcon(KisIconUtils::loadIcon("update-medium"));

        if (m_versionUpdater->hasUpdateCapability()) {
            bnVersionUpdate->setVisible(true);
        } else {
            QString downloadLink = QString(" <a style=\"color: %1; text-decoration: underline\" href=\"%2?%3\">Download Brocreate %4</a>")
                    .arg(blendedColor.name())
                    .arg(m_updaterStatus.downloadLink())
                    .arg(analyticsString + "version-update")
                    .arg(m_updaterStatus.availableVersion());

            versionLabelText.append(downloadLink);
        }

    } else if ((m_updaterStatus.status() == UpdaterStatus::StatusID::UPTODATE)
               || (m_updaterStatus.status() == UpdaterStatus::StatusID::CHECK_ERROR)
               || (m_updaterStatus.status() == UpdaterStatus::StatusID::IN_PROGRESS)) {
        updaterFrame->setVisible(false);
    } else if (m_updaterStatus.status() == UpdaterStatus::StatusID::UPDATE_ERROR) {
        updaterFrame->setVisible(true);
        versionLabelText = i18n("An error occurred during the update");
        versionNotificationLabel->setVisible(true);
        bnErrorDetails->setVisible(true);
        updateIcon->setIcon(KisIconUtils::loadIcon("warning"));
    } else if (m_updaterStatus.status() == UpdaterStatus::StatusID::RESTART_REQUIRED) {
        updaterFrame->setVisible(true);
        versionLabelText = QString("<b>%1</b> %2").arg(i18n("Restart is required.")).arg(m_updaterStatus.details());
        versionNotificationLabel->setVisible(true);
        updateIcon->setIcon(KisIconUtils::loadIcon("view-refresh"));
    }

    versionNotificationLabel->setText(versionLabelText);
    if (!blendedStyle.isNull()) {
        versionNotificationLabel->setStyleSheet(blendedStyle);
    }
}
#endif

#ifdef Q_OS_ANDROID
void KisWelcomePageWidget::initDonations()
{
    KisAndroidDonations *androidDonations = KisAndroidDonations::instance();
    if (!androidDonations) {
        qWarning("KisWelcomePage::initDonations: androidDonations is null");
        return;
    }

    int bannerIndex = QRandomGenerator::global()->bounded(1, 5);
    qreal ry = 0.02;
    qreal rh = 1.0 - ry * 2.0;
    qreal rwpad = 0.02;
    qreal rx, rw;
    switch (bannerIndex) {
    case 1:
    case 3:
        rx = 0.4;
        rw = 1.0 - rx - rwpad;
        break;
    case 2:
    case 4:
        rx = rwpad;
        rw = 0.6 - rx;
        break;
    default:
        qWarning("Unhandled banner index %d", bannerIndex);
        rx = rwpad;
        rw = 1.0 - rwpad * 2.0;
        break;
    }

    QString welcomeBannerPath =
        QStandardPaths::locate(QStandardPaths::AppDataLocation,
                               QStringLiteral("share/krita/donation/welcomebanner%1.jpg").arg(bannerIndex));
    QPixmap welcomeBannerPixmap(welcomeBannerPath);

    if (!welcomeBannerPixmap.isNull()) {
        QVector<QString> headlines = {
            i18n("Become a Supporter!"),
            i18n("Support Brocreate!"),
        };
        QVector<QString> subtitles = {
            i18n("Supporters get brush packs and more."),
            i18n("Contributions keep development going."),
        };
        QString headline = headlines[QRandomGenerator::global()->bounded(headlines.size())];
        QString subtitle = subtitles[QRandomGenerator::global()->bounded(subtitles.size())];

        QPainter painter(&welcomeBannerPixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);

        QPen pen(Qt::black, 12.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        QBrush brush(Qt::white);

        qreal verticalSeparation = 40.0;

        QFont currentFont = font();
        currentFont.setPointSize(100);
        currentFont.setBold(false);
        QPainterPath topPath;
        topPath.addText(0, 0, currentFont, headline);
        QRectF topBounds = topPath.boundingRect();

        currentFont.setPointSize(60);
        currentFont.setBold(true);
        QPainterPath bottomPath;
        bottomPath.addText(0, 0, currentFont, subtitle);
        QRectF bottomBounds = bottomPath.boundingRect();

        qreal maxWidth = qMax(topBounds.width(), bottomBounds.width());
        qreal topOffsetX = (maxWidth - topBounds.width()) / 2.0;
        qreal bottomOffsetX = (maxWidth - bottomBounds.width()) / 2.0;

        QTransform topTransform;
        topTransform.translate(topOffsetX - topBounds.left(), -topBounds.top());
        topPath = topTransform.map(topPath);
        topPath.setFillRule(Qt::WindingFill);

        QTransform bottomTransform;
        bottomTransform.translate(bottomOffsetX - bottomBounds.left(),
                                  topBounds.height() + verticalSeparation - bottomBounds.top());
        bottomPath = bottomTransform.map(bottomPath);
        bottomPath.setFillRule(Qt::WindingFill);

        QPainterPath combinedPath;
        combinedPath.addPath(topPath);
        combinedPath.addPath(bottomPath);

        qreal w = painter.device()->width();
        qreal h = painter.device()->height();
        QRect rect(w * rx, h * ry, w * rw, h * rh);

        QRectF combinedBounds = combinedPath.boundingRect();
        qreal scaleX = rect.width() / combinedBounds.width();
        qreal scaleY = rect.height() / combinedBounds.height();
        qreal scale = qMin(scaleX, scaleY);
        qreal combinedOffsetX = (rect.width() - (combinedBounds.width() * scale)) / 2.0;
        qreal combinedOffsetY = (rect.height() - (combinedBounds.height() * scale)) / 2.0;

        QTransform combinedTransform;
        combinedTransform.translate(rect.left() + combinedOffsetX, rect.top() + combinedOffsetY);
        combinedTransform.scale(scale, scale);
        combinedTransform.translate(-combinedBounds.left(), -combinedBounds.top());
        combinedPath = combinedTransform.map(combinedPath);
        combinedPath.setFillRule(Qt::WindingFill);

        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(combinedPath);

        painter.setPen(Qt::NoPen);
        painter.setBrush(brush);
        painter.drawPath(combinedPath);

        QSize welcomeBannerSize = welcomeBannerPixmap.size().scaled(500, 100, Qt::KeepAspectRatio);
        bnAndroidSupport->setFlat(false);
        bnAndroidSupport->setText(QString());
        bnAndroidSupport->setContentsMargins(0, 0, 0, 0);
        bnAndroidSupport->setIcon(QIcon(welcomeBannerPixmap));
        bnAndroidSupport->setIconSize(welcomeBannerSize);
        bnAndroidSupport->setFixedSize(welcomeBannerSize);
    }

    connect(bnAndroidSupport, &QPushButton::clicked, androidDonations, &KisAndroidDonations::slotStartDonationFlow);
    connect(bnAndroidSupporterManage,
            &QPushButton::clicked,
            androidDonations,
            &KisAndroidDonations::sigShowDonationManagementDialogRequested);

    QString badgePath = QStandardPaths::locate(QStandardPaths::AppDataLocation, "share/krita/donation/banner.png");
    QPixmap badgePixmap(badgePath);
    if (!badgePixmap.isNull()) {
        supporterBadge->setPixmap(badgePixmap);
    }

    connect(androidDonations, SIGNAL(sigStateChanged()), this, SLOT(slotUpdateDonationState()));
    slotUpdateDonationState();
}

void KisWelcomePageWidget::slotUpdateDonationState()
{
    bool badgeVisible = false;
    QWidget *pageVisible = pgSupportMessage;

    KisAndroidDonations *androidDonations = KisAndroidDonations::instance();
    if (androidDonations) {
        badgeVisible = androidDonations->shouldShowSupporterBadge();
        switch (androidDonations->state()) {
        case KisAndroidDonations::State::Supporter:
            pageVisible = pgAndroidSupporter;
            break;
        case KisAndroidDonations::State::NoSupport:
            pageVisible = nullptr;
            break;
        default:
            break;
        }
    } else {
        qWarning("KisWelcomePageWidget::slotUpdateDonationState: android donations is null");
    }

    if (pageVisible) {
        stkSupport->setCurrentWidget(pageVisible);
        wdgAndroidSupportBanner->hide();
        stkSupport->show();
    } else {
        stkSupport->hide();
        wdgAndroidSupportBanner->show();
    }

    supporterBadge->setVisible(badgeVisible);
}
#endif

QFont KisWelcomePageWidget::largerFont()
{
    QFont larger = font();
    qreal ratio = 1.1;
    if (larger.pixelSize() == -1) {
        larger.setPointSizeF(larger.pointSizeF() * ratio);
    } else {
        larger.setPixelSize(qRound(larger.pixelSize() * ratio));
    }
    return larger;
}

void KisWelcomePageWidget::recentFilesResized()
{
    int scrollSize = qApp->style()->pixelMetric(QStyle::PM_SliderThickness);
    int spacing = 8;

    int iconLength = KisRecentDocumentsModelWrapper::DEFALT_ICON_SIZE_LENGTH + spacing;

    int currentSize = recentDocumentsListView->size().width() - scrollSize - 2;
    int iconAmount = qMax(1, static_cast<int>(std::floor(currentSize / iconLength)));
    int length = qMax(32, static_cast<int>(std::floor(currentSize / iconAmount)) - spacing);

    recentDocumentsListView->setIconSize(QSize(length, length));
}

void KisWelcomePageWidget::newsStateChanged(bool hasNews)
{
    btnDismisAll->setVisible(hasNews);
    notifyIcon->setVisible(hasNews);

    QSizePolicy sizePolicy = notifyIcon->sizePolicy();
    if (hasNews) {
        sizePolicy.setHorizontalPolicy(QSizePolicy::Preferred);
    } else {
        sizePolicy.setHorizontalPolicy(QSizePolicy::Ignored);
    }
    notifyIcon->setSizePolicy(sizePolicy);
    btnDismisAll->setSizePolicy(sizePolicy);
}
