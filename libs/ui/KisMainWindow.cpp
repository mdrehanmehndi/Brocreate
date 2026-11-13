/* This file is part of the KDE project
   SPDX-FileCopyrightText: 1998, 1999 Torben Weis <weis@kde.org>
   SPDX-FileCopyrightText: 2000-2006 David Faure <faure@kde.org>
   SPDX-FileCopyrightText: 2007, 2009 Thomas zander <zander@kde.org>
   SPDX-FileCopyrightText: 2010 Benjamin Port <port.benjamin@gmail.com>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "KisMainWindow.h"

#include <KoConfig.h>

// qt includes
#include <QApplication>
#include <QByteArray>
#include <QCloseEvent>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QScreen>
#include <QDialog>
#include <QDockWidget>
#include <QIcon>
#include <QInputDialog>
#include <QLayout>
#include <QMdiArea>
#include <QMdiSubWindow>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <KisSignalMapper.h>
#include <QTabBar>
#include <QMoveEvent>
#include <QUrl>
#include <QMessageBox>
#include <QStatusBar>
#include <QStyleFactory>
#include <QMenu>
#include <QMenuBar>
#include <KisMimeDatabase.h>
#include <QMimeData>
#include <QStackedWidget>
#include <QProxyStyle>
#include <QScreen>
#include <QAction>
#include <QWindow>
#include <QTemporaryDir>
#include <QScrollArea>
#include <QActionGroup>
#include <QFrame>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QToolButton>
#include <QPushButton>
#include <QComboBox>
#include <QLabel>
#include <QElapsedTimer>
#include <QTimer>
#include <QTouchEvent>
#include <QTabletEvent>
#include <QPainter>
#include <QPainterPath>
#include <QButtonGroup>
#include <QLineF>
#include <QtMath>

#include <kactioncollection.h>
#include <kactionmenu.h>
#include <kis_debug.h>
#include <kedittoolbar.h>
#include <khelpmenu.h>
#include <klocalizedstring.h>
#include <kaboutdata.h>
#include <kis_workspace_resource.h>
#include <input/kis_input_manager.h>
#include "dialogs/KisDlgCreateNewDocument.h"
#include "kis_selection_manager.h"
#include "kis_icon_utils.h"
#include <krecentfilesaction.h>
#include "krita_utils.h"
#include <ktoggleaction.h>
#include <ktoolbar.h>
#include <kmainwindow.h>
#include <kxmlguiwindow.h>
#include <kxmlguifactory.h>
#include <kxmlguiclient.h>
#include <kguiitem.h>
#include <kwindowconfig.h>
#include <kacceleratormanager.h>

#include <KoResourcePaths.h>
#include <KoToolFactoryBase.h>
#include <KoToolRegistry.h>
#include <KoDockFactoryBase.h>
#include <KoDockWidgetTitleBar.h>
#include <kis_utility_title_bar.h>
#include <KoDocumentInfoDlg.h>
#include <KoDocumentInfo.h>
#include <KoFileDialog.h>
#include <kis_icon.h>
#include <KoToolManager.h>
#include "KoToolDocker.h"
#include "toolbox/KoToolBoxFactory.h"
#include <KoDockRegistry.h>
#include <KoPluginLoader.h>
#include <KoColorSpaceEngine.h>
#include <KoUpdater.h>
#include <KisResourceModel.h>
#include <KisResourceLoaderRegistry.h>
#include <KisResourceIterator.h>
#include <KisResourceTypes.h>
#include <KisResourceCacheDb.h>
#include <KisStorageModel.h>
#include <KisStorageFilterProxyModel.h>
#include <KisPlaybackEngine.h>

#ifdef Q_OS_ANDROID
#include "KisAndroidDonations.h"
#include "dialogs/KisDonationManagementDialog.h"
#include <QtAndroid>
#include <KisAndroidUtils.h>
#endif

#include <KisScopedPerformanceLogger.h>
#include <KisUsageLogger.h>
#include <brushengine/kis_paintop_settings.h>
#include "dialogs/kis_about_application.h"
#include "dialogs/kis_delayed_save_dialog.h"
#include "dialogs/kis_dlg_preferences.h"
#include "kis_action_manager.h"
#include "KisApplication.h"
#include "kis_canvas2.h"
#include "kis_canvas_controller.h"
#include "kis_canvas_resource_provider.h"
#include "kis_clipboard.h"
#include "kis_config.h"
#include "kis_config_notifier.h"
#include "kis_custom_image_widget.h"
#include "animation/KisAnimationRender.h"
#include "animation/KisDlgAnimationRenderer.h"
#include <KisDocument.h>
#include "kis_group_layer.h"
#include "kis_image_from_clipboard_widget.h"
#include "kis_image.h"
#include "kis_image_animation_interface.h"
#include <KisImportExportFilter.h>
#include "KisImportExportManager.h"
#include "kis_mainwindow_observer.h"
#include "kis_node.h"
#include "KisOpenPane.h"
#include "kis_paintop_box.h"
#include "KisPart.h"
#include "KisResourceServerProvider.h"
#include "kis_signal_compressor_with_param.h"
#include "kis_statusbar.h"
#include "KisView.h"
#include "KisViewManager.h"
#include "thememanager.h"
#include "kis_animation_importer.h"
#include "dialogs/kis_dlg_import_image_sequence.h"
#include "animation/KisDlgImportVideoAnimation.h"
#include <KisImageConfigNotifier.h>
#include "KisWindowLayoutManager.h"
#include <KisUndoActionsUpdateManager.h>
#include "KisWelcomePageWidget.h"
#include "KisRecentDocumentsModelWrapper.h"
#include <KritaVersionWrapper.h>
#include "KisCanvasWindow.h"
#include "kis_action.h"
#include <katecommandbar.h>
#include "KisNodeActivationActionCreatorVisitor.h"
#include "KisUiFont.h"
#include <KisResourceUserOperations.h>
#include "KisRecentFilesManager.h"
#include "KisWidgetConnectionUtils.h"
#include "KisToolBarStateModel.h"
#include <config-qmdiarea-always-show-subwindow-title.h>
#include <config-qt-patches-present.h>
#include <KisPlatformPluginInterfaceFactory.h>

#include <cmath>
#include <mutex>

#if defined(Q_OS_ANDROID) && KRITA_QT_HAS_ANDROID_QPLATFORMSCREEN_DENSITY_ADJUSTMENT
#include <KisAndroidScaling.h>
#endif

namespace {

enum class BrocreateGlyph {
    NewDoc,
    OpenDoc,
    SaveDoc,
    Settings,
    Undo,
    Redo,
    Animation,
    StudioDock,
    Brush,
    Eraser,
    Selection,
    Mirror,
    Fullscreen,
    Move,
    VectorLine,
    Fill,
    Lasso,
    RectSelect,
    Eyedropper,
    Symmetry,
    QuickAdd
};

static QIcon makeBrocreateIcon(BrocreateGlyph glyph, const QColor &strokeColor = QColor("#E4E4E7"))
{
    const int size = 48;
    QPixmap pix(size, size);
    pix.fill(Qt::transparent);

    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen(strokeColor, 3.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    switch (glyph) {
    case BrocreateGlyph::NewDoc:
        p.drawRoundedRect(QRectF(11, 8, 22, 30), 4, 4);
        p.drawLine(QPointF(29, 25), QPointF(39, 25));
        p.drawLine(QPointF(34, 20), QPointF(34, 30));
        break;
    case BrocreateGlyph::OpenDoc:
        p.drawRoundedRect(QRectF(8, 13, 32, 23), 4, 4);
        p.drawLine(QPointF(8, 19), QPointF(22, 19));
        p.drawLine(QPointF(16, 13), QPointF(20, 19));
        break;
    case BrocreateGlyph::SaveDoc:
        p.drawRoundedRect(QRectF(9, 9, 30, 30), 4, 4);
        p.drawRect(QRectF(16, 9, 14, 10));
        p.drawRoundedRect(QRectF(15, 25, 18, 14), 2, 2);
        break;
    case BrocreateGlyph::Settings: {
        p.drawEllipse(QPointF(24, 24), 6.5, 6.5);
        for (int i = 0; i < 8; ++i) {
            const qreal a = i * M_PI / 4.0;
            p.drawLine(QPointF(24 + 10.5 * std::cos(a), 24 + 10.5 * std::sin(a)),
                       QPointF(24 + 14.5 * std::cos(a), 24 + 14.5 * std::sin(a)));
        }
        break;
    }
    case BrocreateGlyph::Undo:
        p.drawArc(QRectF(12, 15, 24, 20), 0, 180 * 16);
        p.drawLine(QPointF(12, 25), QPointF(17, 19));
        p.drawLine(QPointF(12, 25), QPointF(19, 27));
        break;
    case BrocreateGlyph::Redo:
        p.drawArc(QRectF(12, 15, 24, 20), 0, 180 * 16);
        p.drawLine(QPointF(36, 25), QPointF(31, 19));
        p.drawLine(QPointF(36, 25), QPointF(29, 27));
        break;
    case BrocreateGlyph::Animation:
        p.drawRoundedRect(QRectF(9, 12, 30, 24), 4, 4);
        p.drawLine(QPointF(17, 12), QPointF(17, 36));
        p.drawLine(QPointF(31, 12), QPointF(31, 36));
        p.drawLine(QPointF(22, 19), QPointF(27, 24));
        p.drawLine(QPointF(27, 24), QPointF(22, 29));
        break;
    case BrocreateGlyph::StudioDock:
        p.drawRoundedRect(QRectF(9, 11, 30, 26), 4, 4);
        p.drawLine(QPointF(28, 11), QPointF(28, 37));
        break;
    case BrocreateGlyph::Brush:
        p.drawLine(QPointF(13, 35), QPointF(33, 12));
        p.drawLine(QPointF(18, 37), QPointF(37, 16));
        p.drawEllipse(QPointF(13, 35), 4.5, 4.5);
        break;
    case BrocreateGlyph::Eraser: {
        QPainterPath path;
        path.moveTo(15, 34);
        path.lineTo(28, 13);
        path.lineTo(37, 19);
        path.lineTo(24, 38);
        path.closeSubpath();
        p.drawPath(path);
        p.drawLine(QPointF(20, 26), QPointF(30, 31));
        break;
    }
    case BrocreateGlyph::Selection:
        p.drawEllipse(QRectF(11, 12, 26, 20));
        p.drawLine(QPointF(20, 32), QPointF(17, 38));
        break;
    case BrocreateGlyph::Mirror:
        p.drawLine(QPointF(24, 9), QPointF(24, 39));
        p.drawPolygon(QPolygonF({QPointF(10, 24), QPointF(19, 16), QPointF(19, 32)}));
        p.drawPolygon(QPolygonF({QPointF(38, 24), QPointF(29, 16), QPointF(29, 32)}));
        break;
    case BrocreateGlyph::Fullscreen:
        p.drawPolyline(QPolygonF({QPointF(10, 18), QPointF(10, 10), QPointF(18, 10)}));
        p.drawPolyline(QPolygonF({QPointF(30, 10), QPointF(38, 10), QPointF(38, 18)}));
        p.drawPolyline(QPolygonF({QPointF(38, 30), QPointF(38, 38), QPointF(30, 38)}));
        p.drawPolyline(QPolygonF({QPointF(18, 38), QPointF(10, 38), QPointF(10, 30)}));
        break;
    case BrocreateGlyph::Move:
        p.drawPolygon(QPolygonF({QPointF(15, 10), QPointF(15, 34), QPointF(22, 27), QPointF(31, 27)}));
        break;
    case BrocreateGlyph::VectorLine: {
        QPainterPath curve;
        curve.moveTo(11, 35);
        curve.cubicTo(14, 14, 34, 34, 37, 13);
        p.drawPath(curve);
        p.drawEllipse(QPointF(11, 35), 2.5, 2.5);
        p.drawEllipse(QPointF(37, 13), 2.5, 2.5);
        break;
    }
    case BrocreateGlyph::Fill:
        p.drawRoundedRect(QRectF(11, 15, 18, 18), 3, 3);
        p.drawLine(QPointF(27, 23), QPointF(36, 32));
        p.drawEllipse(QPointF(36, 35), 2.5, 3.5);
        break;
    case BrocreateGlyph::Lasso: {
        QPen dashed(strokeColor, 2.8, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(dashed);
        p.drawEllipse(QRectF(11, 11, 26, 22));
        p.setPen(pen);
        p.drawLine(QPointF(21, 33), QPointF(18, 39));
        break;
    }
    case BrocreateGlyph::RectSelect: {
        QPen dashed(strokeColor, 2.8, Qt::DashLine, Qt::RoundCap, Qt::RoundJoin);
        p.setPen(dashed);
        p.drawRoundedRect(QRectF(11, 11, 26, 26), 3, 3);
        break;
    }
    case BrocreateGlyph::Eyedropper:
        p.drawLine(QPointF(12, 36), QPointF(29, 19));
        p.drawLine(QPointF(26, 15), QPointF(33, 22));
        p.drawEllipse(QPointF(34, 14), 4.5, 4.5);
        break;
    case BrocreateGlyph::Symmetry:
        for (int i = 0; i < 6; ++i) {
            const qreal a = i * M_PI / 3.0;
            p.drawLine(QPointF(24, 24), QPointF(24 + 13 * std::cos(a), 24 + 13 * std::sin(a)));
        }
        p.drawEllipse(QPointF(24, 24), 4.0, 4.0);
        break;
    case BrocreateGlyph::QuickAdd:
        p.drawEllipse(QPointF(24, 24), 13.0, 13.0);
        p.drawLine(QPointF(17, 24), QPointF(31, 24));
        p.drawLine(QPointF(24, 17), QPointF(24, 31));
        break;
    }

    p.end();
    return QIcon(pix);
}

class BrocreatePillSlider : public QWidget
{
public:
    enum StyleMode {
        OpacityBluePill,
        SizeDarkPill
    };

    explicit BrocreatePillSlider(StyleMode mode, QWidget *parent = nullptr)
        : QWidget(parent)
        , m_mode(mode)
    {
        setFixedSize(mode == OpacityBluePill ? 124 : 128, 30);
        setCursor(Qt::PointingHandCursor);
        setAttribute(Qt::WA_AcceptTouchEvents, true);
    }

    void setNormalizedValue(qreal value)
    {
        const qreal clamped = qBound<qreal>(0.0, value, 1.0);
        if (!qFuzzyCompare(m_value + 1.0, clamped + 1.0)) {
            m_value = clamped;
            update();
        }
    }

    qreal normalizedValue() const { return m_value; }

    void setDisplayText(const QString &text)
    {
        if (m_displayText != text) {
            m_displayText = text;
            update();
        }
    }

    std::function<void(qreal)> onValueChanged;

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath clip;
        clip.addRoundedRect(r, 9.0, 9.0);
        p.setClipPath(clip);

        // Track background
        p.fillRect(r, QColor("#1E1E22"));

        // Value fill
        const qreal fillWidth = qMax<qreal>(0.0, r.width() * m_value);
        if (fillWidth > 0.5) {
            QRectF fillRect(r.left(), r.top(), fillWidth, r.height());
            if (m_mode == OpacityBluePill) {
                p.fillRect(fillRect, QColor("#0066FF"));
            } else {
                p.fillRect(fillRect, QColor("#262B36"));
                p.fillRect(QRectF(r.left(), r.top(), qMin<qreal>(fillWidth, 6.0), r.height()), QColor("#0066FF"));
            }
        }

        p.setClipping(false);
        p.setPen(QPen(QColor("#27272A"), 1.0));
        p.drawRoundedRect(r, 9.0, 9.0);

        // Icon
        p.setPen(QPen(QColor("#FFFFFF"), 1.5));
        if (m_mode == OpacityBluePill) {
            const QRectF iconRect(r.left() + 9.0, r.center().y() - 5.5, 11.0, 11.0);
            p.drawEllipse(iconRect);
            p.setBrush(QColor("#FFFFFF"));
            p.drawPie(iconRect, 90 * 16, 180 * 16);
            p.setBrush(Qt::NoBrush);
        } else {
            p.setBrush(QColor("#0066FF"));
            p.setPen(Qt::NoPen);
            p.drawEllipse(QPointF(r.left() + 14.0, r.center().y()), 3.5, 3.5);
        }

        // Value label
        p.setPen(QColor("#F4F4F5"));
        QFont f = font();
        f.setPointSizeF(9.5);
        f.setWeight(QFont::DemiBold);
        p.setFont(f);
        const QRectF textRect = r.adjusted(26.0, 0.0, -8.0, 0.0);
        p.drawText(textRect, Qt::AlignVCenter | Qt::AlignLeft, m_displayText);
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        m_dragging = true;
        updateFromPos(e->pos().x());
        e->accept();
    }

    void mouseMoveEvent(QMouseEvent *e) override
    {
        if (m_dragging) {
            updateFromPos(e->pos().x());
            e->accept();
        }
    }

    void mouseReleaseEvent(QMouseEvent *e) override
    {
        m_dragging = false;
        e->accept();
    }

    void wheelEvent(QWheelEvent *e) override
    {
        const qreal delta = e->angleDelta().y() > 0 ? 0.04 : -0.04;
        setNormalizedValue(m_value + delta);
        if (onValueChanged) {
            onValueChanged(m_value);
        }
        e->accept();
    }

private:
    void updateFromPos(int x)
    {
        const qreal ratio = qBound<qreal>(0.0, qreal(x) / qMax(1, width()), 1.0);
        setNormalizedValue(ratio);
        if (onValueChanged) {
            onValueChanged(m_value);
        }
    }

    StyleMode m_mode;
    qreal m_value {0.35};
    QString m_displayText;
    bool m_dragging {false};
};

} // namespace

class ToolDockerFactory : public KoDockFactoryBase
{
public:
    ToolDockerFactory() : KoDockFactoryBase() { }

    QString id() const override {
        return "sharedtooldocker";
    }

    QDockWidget* createDockWidget() override {
        KoToolDocker* dockWidget = new KoToolDocker();
        return dockWidget;
    }

    DockPosition defaultDockPosition() const override {
        return DockRight;
    }
};

class Q_DECL_HIDDEN KisMainWindow::Private
{
public:
    Private(KisMainWindow *parent, QUuid id)
        : q(parent)
        , id(id)
        , styleMenu(new KActionMenu(i18nc("@action:inmenu", "Styles"), parent))
        , dockWidgetMenu(new KActionMenu(i18nc("@action:inmenu", "&Dockers"), parent))
        , windowMenu(new KActionMenu(i18nc("@action:inmenu", "&Window"), parent))
        , documentMenu(new KActionMenu(i18nc("@action:inmenu", "New &View"), parent))
        , workspaceMenu(new KActionMenu(i18nc("@action:inmenu", "Wor&kspace"), parent))
        , welcomePage(new KisWelcomePageWidget(parent))
        , widgetStack(new QStackedWidget(parent))
        , mdiArea(new QMdiArea(parent))
        , windowMapper(new KisSignalMapper(parent))
        , documentMapper(new KisSignalMapper(parent))
    {
        if (id.isNull()) this->id = QUuid::createUuid();

        welcomeScroller = new QScrollArea();
        welcomeScroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        welcomeScroller->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        welcomeScroller->setWidget(welcomePage);
        welcomeScroller->setWidgetResizable(true);

        widgetStack->addWidget(welcomeScroller);
        widgetStack->addWidget(mdiArea);
        mdiArea->setTabsMovable(true);
        mdiArea->setActivationOrder(QMdiArea::ActivationHistoryOrder);
        mdiArea->setDocumentMode(true);
        mdiArea->setOption(QMdiArea::DontMaximizeSubWindowOnActivation);
#ifdef HAVE_QMDIAREA_ALWAYS_SHOW_SUBWINDOW_TITLE
        mdiArea->setOption(QMdiArea::AlwaysShowSubwindowNameInTitleBar);
#endif /* HAVE_QMDIAREA_ALWAYS_SHOW_SUBWINDOW_TITLE */

        commandBar = new KateCommandBar(parent);
    }

    ~Private() {
        qDeleteAll(toolbarList);
    }

    KisMainWindow *q {nullptr};
    QUuid id;

    KisViewManager *viewManager {nullptr};

    QPointer<KisView> activeView;
    KisSignalAutoConnectionsStore activeViewConnections;

    QList<QAction *> toolbarList;

    bool firstTime {true};
    bool windowSizeDirty {false};
#ifdef Q_OS_ANDROID
    bool flashWindowHackInProgress {false};
#endif

    KisAction *showDocumentInfo {nullptr};
    KisAction *saveAction {nullptr};
    KisAction *saveActionAs {nullptr};
    KisAction *importAnimation {nullptr};
    KisAction *importVideoAnimation {nullptr};
    KisAction *renderAnimation {nullptr};
    KisAction *renderAnimationAgain {nullptr};
    KisAction *closeAll {nullptr};
    KisAction *importFile {nullptr};
    KisAction *exportFile {nullptr};
    KisAction *exportFileAdvance {nullptr};
    KisAction *undo {nullptr};
    KisAction *redo {nullptr};
    KisAction *close {nullptr};
    KisAction *mdiCascade {nullptr};
    KisAction *mdiTile {nullptr};
    KisAction *mdiNextWindow {nullptr};
    KisAction *mdiPreviousWindow {nullptr};
    KisAction *toggleDockers {nullptr};
    KisAction *resetConfigurations {nullptr};
    KisAction *toggleDockerTitleBars {nullptr};
#ifdef Q_OS_ANDROID
    KisAction *showDonationManagementDialog {nullptr};
    KisAction *manageSubscriptions {nullptr};
#if KRITA_QT_HAS_ANDROID_QPLATFORMSCREEN_DENSITY_ADJUSTMENT
    KisAction *changeInterfaceScale {nullptr};
#endif
#else
    KisAction *toggleDetachCanvas {nullptr};
    KisAction *newWindow {nullptr};
#endif
    KisAction *fullScreenMode {nullptr};
    KisAction *showSessionManager {nullptr};
    KisAction *commandBarAction {nullptr};
    KisAction *expandingSpacers[2];

    KActionMenu *styleMenu {nullptr};
    QActionGroup* styleActions {nullptr};
    QMap<QString, QAction*> actionMap;

    KActionMenu *dockWidgetMenu {nullptr};
    KActionMenu *windowMenu {nullptr};
    KActionMenu *documentMenu {nullptr};
    KActionMenu *workspaceMenu {nullptr};

    KisKHelpMenu *helpMenu  {nullptr};

    KRecentFilesAction *recentFiles {nullptr};
    KisResourceModel *workspacemodel {nullptr};

    QScopedPointer<KisUndoActionsUpdateManager> undoActionsUpdateManager;

    QString lastExportLocation;

    QMap<QString, QDockWidget *> dockWidgetsMap;
    QByteArray dockerStateBeforeHiding;
    KoToolDocker *toolOptionsDocker {nullptr};

    QCloseEvent *deferredClosingEvent {nullptr};
#ifndef Q_OS_HAIKU
    Digikam::ThemeManager *themeManager {nullptr};
#endif
    QScrollArea *welcomeScroller {nullptr};
    KisWelcomePageWidget *welcomePage {nullptr};


    QStackedWidget *widgetStack {nullptr};

    QMdiArea *mdiArea {nullptr};
    QMdiSubWindow *activeSubWindow  {nullptr};
    KisSignalMapper *windowMapper {nullptr};
    KisSignalMapper *documentMapper {nullptr};
    KisCanvasWindow *canvasWindow {nullptr};

    QByteArray lastExportedFormat;
    QScopedPointer<KisSignalCompressorWithParam<int> > tabSwitchCompressor;
    QMutex savingEntryMutex;

    KConfigGroup windowStateConfig;

    QUuid workspaceBorrowedBy;

    KateCommandBar *commandBar {nullptr};

    // Brocreate Tablet Floating UI & Gestures
    QFrame *topLeftIsland {nullptr};
    QFrame *topCenterIsland {nullptr};
    QFrame *leftToolCapsule {nullptr};
    QPushButton *colorSwatchButton {nullptr};
    QComboBox *blendModeCombo {nullptr};
    BrocreatePillSlider *opacityPill {nullptr};
    BrocreatePillSlider *brushSizePill {nullptr};
    QToolButton *brushModeBtn {nullptr};
    QToolButton *eraserModeBtn {nullptr};
    QToolButton *selectModeBtn {nullptr};
    QToolButton *mirrorBtn {nullptr};
    QToolButton *fullscreenBtn {nullptr};
    QToolButton *animationModeBtn {nullptr};
    QToolButton *studioToggleBtn {nullptr};
    QButtonGroup *leftToolGroup {nullptr};
    QList<QToolButton *> leftToolButtons;

    bool brocreateUiInitialized {false};
    bool animationLayoutActive {false};
    bool canvasOnlyOverlayHidden {false};
    bool updatingFromResource {false};

    // Procreate Multi-Touch Gesture & Palm Rejection State
    QElapsedTimer lastStylusTimer;
    QElapsedTimer touchStartTimer;
    QTimer *longPressEyedropperTimer {nullptr};
    QPointF touchStartCentroid;
    QPointF lastTouchCentroid;
    qreal initialPinchSpan {0.0};
    int maxTouchPoints {0};
    bool touchGestureConsumed {false};
    bool pinchFitTriggered {false};

    KisActionManager * actionManager() {
        return viewManager->actionManager();
    }

    QTabBar* findTabBarHACK() {
        QObjectList objects = mdiArea->children();
        Q_FOREACH (QObject *object, objects) {
            QTabBar *bar = qobject_cast<QTabBar*>(object);
            if (bar) {
                return bar;
            }
        }
        return 0;
    }
};

class ScopedWidgetDisabler
{
    QWidget *widget;
public:
    ScopedWidgetDisabler(QWidget *widget_)
        : widget(widget_)
    {
        widget->setEnabled(false);
    }

    ~ScopedWidgetDisabler()
    {
        widget->setEnabled(true);
    }
};

KisMainWindow::KisMainWindow(QUuid uuid)
    : KXmlGuiWindow()
    , d(new Private(this, uuid))
{
    KisScopedPerformanceLogger perfLog(QStringLiteral("KisMainWindow::KisMainWindow"));

    KAcceleratorManager::setNoAccel(this);
    setWindowTitle(QStringLiteral("Brocreate"));

    d->workspacemodel = new KisResourceModel(ResourceType::Workspaces, this);
    connect(d->workspacemodel, SIGNAL(modelReset()), this, SLOT(updateWindowMenu()));
    connect(d->workspacemodel, SIGNAL(rowsInserted(QModelIndex,int,int)), this, SLOT(updateWindowMenu()));
    connect(d->workspacemodel, SIGNAL(rowsRemoved(QModelIndex,int,int)), this, SLOT(updateWindowMenu()));

    d->viewManager = new KisViewManager(this, actionCollection());
    KConfigGroup group( KSharedConfig::openConfig(), "theme");
#ifndef Q_OS_HAIKU
    d->themeManager = new Digikam::ThemeManager(group.readEntry("Theme", "Krita dark"), this);
#endif
    d->windowStateConfig = KSharedConfig::openConfig()->group("MainWindow");

    setStandardToolBarMenuEnabled(true);
    setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);
    setDockNestingEnabled(true);
#ifdef Q_OS_ANDROID
    // On Android, the animations for docks and menus are not only slow, but
    // also can cause crashes in some circumstances. Turning them off feels
    // better and avoids those.
    setAnimated(false);
#endif

    qApp->setStartDragDistance(25);     // 25 px is a distance that works well for Tablet and Mouse events

    connect(this, SIGNAL(restoringDone()), this, SLOT(forceDockTabFonts()));
    connect(this, SIGNAL(themeChanged()), d->viewManager, SLOT(updateIcons()));
    connect(this, SIGNAL(themeChanged()), d->viewManager->toolManager(), SLOT(themeChanged()));
    connect(KisPart::instance(), SIGNAL(documentClosed(QString)), SLOT(updateWindowMenu()));
    connect(KisPart::instance(), SIGNAL(documentOpened(QString)), SLOT(updateWindowMenu()));
    connect(KisConfigNotifier::instance(), SIGNAL(configChanged()), this, SLOT(configChanged()));

    actionCollection()->addAssociatedWidget(this);
    KoPluginLoader::instance()->load("Krita/ViewPlugin", KoPluginLoader::PluginsConfig(), d->viewManager, false);

    // Load the per-application plugins (Right now, only Python) We do this only once, when the first mainwindow is being created.
    KoPluginLoader::instance()->load("Krita/ApplicationPlugin", KoPluginLoader::PluginsConfig(), qApp, true);

    KoToolBoxFactory toolBoxFactory;
    QDockWidget *toolbox = createDockWidget(&toolBoxFactory);

    KisConfig cfg(true);
    if (cfg.toolOptionsInDocker()) {
        ToolDockerFactory toolDockerFactory;
        d->toolOptionsDocker = qobject_cast<KoToolDocker*>(createDockWidget(&toolDockerFactory));
        if (d->toolOptionsDocker) {
            d->toolOptionsDocker->toggleViewAction()->setEnabled(true);
        }
    }

    QMap<QString, QAction*> dockwidgetActions;

    if (toolbox) {
        dockwidgetActions[toolbox->toggleViewAction()->text()] = toolbox->toggleViewAction();
    }

    for (KoDockFactoryBase *factory : KoDockRegistry::instance()->sortedDockWidgetFactories()) {
        QDockWidget *dw = createDockWidget(factory);
        if (dw) {
            dockwidgetActions[dw->toggleViewAction()->text()] = dw->toggleViewAction();
        }
    }

    if (d->toolOptionsDocker) {
        dockwidgetActions[d->toolOptionsDocker->toggleViewAction()->text()] = d->toolOptionsDocker->toggleViewAction();
    }
    connect(d->viewManager->toolManager(), SIGNAL(toolOptionWidgetsChanged(KoCanvasController*,QList<QPointer<QWidget> >)), this, SLOT(newOptionWidgets(KoCanvasController*,QList<QPointer<QWidget> >)));

    Q_FOREACH (QString title, dockwidgetActions.keys()) {
        d->dockWidgetMenu->addAction(dockwidgetActions[title]);
    }


    // Style menu actions
    d->styleActions = new QActionGroup(this);
    QAction * action;

#ifndef Q_OS_ANDROID

    Q_FOREACH (QString styleName, QStyleFactory::keys()) {

        action = new QAction(styleName, d->styleActions);
        action->setCheckable(true);
        d->actionMap.insert(styleName, action);
        d->styleMenu->addAction(d->actionMap.value(styleName));
    }

    // select the config value, or the current style if that does not exist
    QString styleFromConfig = cfg.widgetStyle().toLower();
    QString styleToSelect = styleFromConfig.isEmpty() ? style()->objectName().toLower() : styleFromConfig;

    Q_FOREACH (auto key, d->actionMap.keys()) {
        if(key.toLower() == styleToSelect) { // does the key match selection
            d->actionMap.value(key)->setChecked(true);
        }
    }

    connect(d->styleActions, SIGNAL(triggered(QAction*)),
            this, SLOT(slotUpdateWidgetStyle()));
#endif

    // Load all the actions from the tool plugins
    // ToolBoxDocker needs them when at setViewManager()
    Q_FOREACH(KoToolFactoryBase *toolFactory, KoToolRegistry::instance()->values()) {
        toolFactory->createActions(actionCollection(), d->viewManager->toolManager());
    }


    Q_FOREACH (QDockWidget *wdg, dockWidgets()) {
        if ((wdg->features() & QDockWidget::DockWidgetClosable) == 0) {
            wdg->setVisible(true);
        }
    }

    Q_FOREACH (KoCanvasObserverBase* observer, canvasObservers()) {
        observer->setObservedCanvas(0);
        KisMainwindowObserver* mainwindowObserver = dynamic_cast<KisMainwindowObserver*>(observer);
        if (mainwindowObserver) {
            mainwindowObserver->setViewManager(d->viewManager);
        }
    }

    d->mdiArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    d->mdiArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    d->mdiArea->setTabPosition(QTabWidget::North);
    d->mdiArea->setTabsClosable(true);
    d->mdiArea->setAcceptDrops(true);

    themeChanged(); // updates icon styles

    setCentralWidget(d->widgetStack);
    d->widgetStack->setCurrentIndex(0);

    connect(d->mdiArea, SIGNAL(subWindowActivated(QMdiSubWindow*)), this, SLOT(subWindowActivated()));
    connect(d->windowMapper, SIGNAL(mapped(QWidget*)), this, SLOT(setActiveSubWindow(QWidget*)));
    connect(d->documentMapper, SIGNAL(mapped(QObject*)), this, SLOT(newView(QObject*)));

    d->canvasWindow = new KisCanvasWindow(this);
    actionCollection()->addAssociatedWidget(d->canvasWindow);

    createActions();

    // the welcome screen needs to grab actions...so make sure this line goes after the createAction() so they exist
    d->welcomePage->setMainWindow(this);

    d->recentFiles->setRecentFilesModel(&KisRecentDocumentsModelWrapper::instance()->model());

    subWindowActivated();
    updateWindowMenu();

    if (isHelpMenuEnabled() && !d->helpMenu) {
        d->helpMenu = new KisKHelpMenu(this, KAboutData::applicationData(), false);

        KisKActionCollection *actions = d->viewManager->actionCollection();
        QAction *helpContentsAction = d->helpMenu->action(KisKHelpMenu::menuHelpContents);
        QAction *whatsThisAction = d->helpMenu->action(KisKHelpMenu::menuWhatsThis);
        QAction *reportBugAction = d->helpMenu->action(KisKHelpMenu::menuReportBug);
        QAction *switchLanguageAction = d->helpMenu->action(KisKHelpMenu::menuSwitchLanguage);
        QAction *aboutAppAction = d->helpMenu->action(KisKHelpMenu::menuAboutApp);
        QAction *aboutKdeAction = d->helpMenu->action(KisKHelpMenu::menuAboutKDE);

        if (helpContentsAction) {
            actions->addAction(helpContentsAction->objectName(), helpContentsAction);
        }
        if (whatsThisAction) {
            actions->addAction(whatsThisAction->objectName(), whatsThisAction);
        }
        if (reportBugAction) {
            actions->addAction(reportBugAction->objectName(), reportBugAction);
        }
        if (switchLanguageAction) {
            actions->addAction(switchLanguageAction->objectName(), switchLanguageAction);
        }
        if (aboutAppAction) {
            actions->addAction(aboutAppAction->objectName(), aboutAppAction);
        }
        if (aboutKdeAction) {
            actions->addAction(aboutKdeAction->objectName(), aboutKdeAction);
        }

        connect(d->helpMenu, SIGNAL(showAboutApplication()), SLOT(showAboutApplication()));
    }

    // Custom handling of help action for manual URL outside docs.kde.org.
    QAction *helpAction = actionCollection()->action("help_contents");
    if (helpAction) {
        helpAction->disconnect(SIGNAL(triggered()));
        connect(helpAction, SIGNAL(triggered()), this, SLOT(showManual()));
    }

    configChanged();

    // Make sure the python plugins create their actions in time
    KisPart::instance()->notifyMainWindowIsBeingCreated(this);

    // If we have customized the toolbars, load that first
    setLocalXMLFile(KoResourcePaths::locateLocal("data", "krita5.xmlgui"));
    setXMLFile(":/kxmlgui5/krita5.xmlgui");

    guiFactory()->addClient(this);
    applyActionIconOverridesFromLocalXML();
    connect(guiFactory(), SIGNAL(makingChanges(bool)), SLOT(slotXmlGuiMakingChanges(bool)));

    // Create and plug toolbar list for Settings menu
    QList<QAction *> toolbarList;
    Q_FOREACH (QWidget* it, guiFactory()->containers("ToolBar")) {
        KisToolBar * toolBar = ::qobject_cast<KisToolBar *>(it);
        if (toolBar) {
            KToggleAction* act = new KToggleAction(i18n("Show %1 Toolbar", toolBar->windowTitle()), this);
            actionCollection()->addAction(toolBar->objectName().toUtf8(), act);
            act->setCheckedState(KGuiItem(i18n("Hide %1 Toolbar", toolBar->windowTitle())));
            connect(act, SIGNAL(toggled(bool)), this, SLOT(slotToolbarToggled(bool)));
            act->setChecked(!toolBar->isHidden());
            toolbarList.append(act);
        } else {
            warnUI << "Toolbar list contains a " << it->metaObject()->className() << " which is not a toolbar!";
        }
    }

    plugActionList("toolbarlist", toolbarList);
    d->toolbarList = toolbarList;

    applyToolBarLayout();

    d->viewManager->updateGUI();
    d->viewManager->updateIcons();

    QTimer::singleShot(1000, Qt::CoarseTimer, this, SLOT(checkSanity()));

    {
        using namespace std::placeholders; // For _1 placeholder
        std::function<void (int)> callback(
                    std::bind(&KisMainWindow::switchTab, this, _1));

        d->tabSwitchCompressor.reset(
                    new KisSignalCompressorWithParam<int>(500, callback, KisSignalCompressor::FIRST_INACTIVE));
    }


    if (cfg.readEntry("CanvasOnlyActive", false)) {
        QString currentWorkspace = cfg.readEntry<QString>("CurrentWorkspace", "Default");
        KoResourceServer<KisWorkspaceResource> * rserver = KisResourceServerProvider::instance()->workspaceServer();
        KisWorkspaceResourceSP workspace = rserver->resource("", "", currentWorkspace);
        if (workspace) {
            restoreWorkspace(workspace);
        }
        cfg.writeEntry("CanvasOnlyActive", false);
    }

    this->winId(); // Ensures the native window has been created.

#ifdef Q_OS_ANDROID
    // HACK: This prevents the mainWindow from going beyond the screen with no
    // way to bring it back. Apparently the size doesn't matter here as long as
    // it remains fixed?
    setFixedSize(KisApplication::primaryScreen()->availableGeometry().size());

    QScreen *s = QGuiApplication::primaryScreen();
    s->setOrientationUpdateMask(Qt::LandscapeOrientation|Qt::InvertedLandscapeOrientation|Qt::PortraitOrientation|Qt::InvertedPortraitOrientation);
    connect(s, SIGNAL(orientationChanged(Qt::ScreenOrientation)), this, SLOT(orientationChanged()));

#if KRITA_QT_HAS_ANDROID_QPLATFORMSCREEN_DENSITY_ADJUSTMENT
    // When the screen's UI scale changes, we have to hide and show it to get
    // its contents to update, otherwise it fails to re-render altogether.
    KisAndroidScaling *androidScaling = KisAndroidScaling::instance();
    if (androidScaling) {
        connect(androidScaling,
                &KisAndroidScaling::sigInterfaceScaleChanged,
                this,
                &KisMainWindow::slotFlashWindowHack,
                Qt::QueuedConnection);
    }
#endif

    // When Brocreate starts, Java side sends an event to set applicationState() to active. But, before
    // the event could reach KisApplication's platform integration, it is cleared by KisOpenGLModeProber::probeFormat.
    // So, we send it manually when MainWindow shows up.
    QAndroidJniObject::callStaticMethod<void>("org/qtproject/qt5/android/QtNative", "setApplicationState", "(I)V", Qt::ApplicationActive);
#endif

    setAcceptDrops(true);
    QTabBar *tabBar = d->findTabBarHACK();
    if (tabBar) {
        tabBar->setElideMode(Qt::ElideRight);
        // load customized tab style
        customizeTabBar();
        tabBar->setExpanding(true);
        tabBar->setAcceptDrops(true);
        tabBar->setChangeCurrentOnDrag(true);
    }

    applyMainWindowSettings(d->windowStateConfig);

    // Initialize Brocreate Tablet UI, QSS dark theme, floating islands, and gestures
    setupBrocreateUI();
    applyBrocreateDarkTheme();
}

KisMainWindow::~KisMainWindow()
{
    /**
     * Destructor of KisView accesses KisViewManager is multiple ways,
     * directly and via KoToolManager. Hence we should destroy all the
     * subwindows **before** we actually destroy the view manager.
     *
     * Please note that we cannot just `close()` the subwindows, becasue
     * they are deleted with deleteLater(), which may postpone their
     * destruction till after KisViewManager's death.
     */
    Q_FOREACH (QMdiSubWindow *subwin, d->mdiArea->subWindowList()) {
        if (subwin) {
            (void) subwin->close();
            delete subwin;
        }
    }

    // The doc and view might still exist (this is the case when closing the window)
    KisPart::instance()->removeMainWindow(this);

    delete d->viewManager;
    delete d;

}

QMenu *KisMainWindow::createPopupMenu()
{
    return 0;
}

QUuid KisMainWindow::id() const {
    return d->id;
}

void KisMainWindow::addView(KisView *view, QMdiSubWindow *subWindow)
{
    if (d->activeView == view && !subWindow) return;

    if (d->activeView) {
        d->activeView->disconnect(this);
    }

    // register the newly created view in the input manager
    viewManager()->inputManager()->addTrackedCanvas(view->canvasBase());

    showView(view, subWindow);
    Q_EMIT restoringDone();
}

void KisMainWindow::notifyChildViewDestroyed(KisView *view)
{
    /**
     * If we are the last view of the window, Qt will not activate another tab
     * before destroying tab/window. In this case we should clear all the dangling
     * pointers manually by setting the current view to null
     */
    viewManager()->inputManager()->removeTrackedCanvas(view->canvasBase());
    if (view->canvasBase() == viewManager()->canvasBase()) {
        viewManager()->setCurrentView(0);
    }
}


void KisMainWindow::showView(KisView *imageView, QMdiSubWindow *subwin)
{
    if (imageView && activeView() != imageView) {
        // XXX: find a better way to initialize this!
        imageView->setViewManager(d->viewManager);

        imageView->canvasBase()->setFavoriteResourceManager(d->viewManager->paintOpBox()->favoriteResourcesManager());
        imageView->slotLoadingFinished();

        if (!subwin) {
            QMdiSubWindow *currentSubWin = d->mdiArea->currentSubWindow();
            bool shouldMaximize = currentSubWin ? currentSubWin->isMaximized() : true;
            subwin = d->mdiArea->addSubWindow(imageView);

            const bool osManagedSurfacePresent = KisPlatformPluginInterfaceFactory::instance()->surfaceColorManagedByOS();
            const bool useNativeSurfaceForCanvas = KisConfig(true).effectiveShouldUseNativeSurfaceForCanvas(osManagedSurfacePresent);
            if (useNativeSurfaceForCanvas) {
                subwin->setAttribute(Qt::WA_DontCreateNativeAncestors);
                subwin->setAttribute(Qt::WA_NativeWindow);
            }

            if (shouldMaximize) {
                subwin->setWindowState(Qt::WindowMaximized);
            }
        } else {
            subwin->setWidget(imageView);
        }
        imageView->setSubWindow(subwin);
        subwin->setAttribute(Qt::WA_DeleteOnClose, true);
        connect(subwin, SIGNAL(destroyed()), SLOT(updateWindowMenu()));

        KisConfig cfg(true);
        subwin->setOption(QMdiSubWindow::RubberBandMove, cfg.readEntry<int>("mdi_rubberband", cfg.useOpenGL()));
        subwin->setOption(QMdiSubWindow::RubberBandResize, cfg.readEntry<int>("mdi_rubberband", cfg.useOpenGL()));
        subwin->setWindowIcon(qApp->windowIcon());

#ifdef Q_OS_MACOS
        connect(subwin, SIGNAL(destroyed()), SLOT(updateSubwindowFlags()));
        updateSubwindowFlags();
#endif

        if (d->mdiArea->subWindowList().size() == 1) {
            imageView->showMaximized();
        }
        else {
            imageView->show();
        }

        // Enable 144Hz touch/stylus gesture filtering & palm rejection on the canvas widget
        if (imageView->canvasBase() && imageView->canvasBase()->canvasWidget()) {
            QWidget *canvasWidget = imageView->canvasBase()->canvasWidget();
            canvasWidget->setAttribute(Qt::WA_AcceptTouchEvents, true);
            canvasWidget->installEventFilter(this);
        }

        d->viewManager->toolManager()->initializeCurrentToolForCanvas();

        setActiveView(imageView);

        updateWindowMenu();
        slotUpdateBrocreateOverlayGeometry();
    } else {
        unsetActiveView();
    }
}

void KisMainWindow::slotPreferences()
{
    std::optional<KisDlgPreferences::PageDesc>page = std::nullopt;

    KisAction* action = d->actionManager()->actionByName("options_configure");
    QVariant data  = action->data();
    if (data.userType() == QMetaType::QVariantList) {
        // Reset the data first
        action->setData(QVariant());
        QList<QVariant> list = data.toList();

        Q_ASSERT(list.length() == 2);
        Q_ASSERT(list[0].userType() == QMetaType::Int);
        Q_ASSERT(list[1].userType() == QMetaType::Int);

        KisDlgPreferences::Page p = (KisDlgPreferences::Page)list[0].toInt();
        int t = list[1].toInt();
        page = KisDlgPreferences::PageDesc{p, t};
    }

    QScopedPointer<KisDlgPreferences> dlgPreferences(new KisDlgPreferences(this));

    if (dlgPreferences->editPreferences(page)) {
        KisConfigNotifier::instance()->notifyConfigChanged();
        KisConfigNotifier::instance()->notifyPixelGridModeChanged();
        KisImageConfigNotifier::instance()->notifyConfigChanged();

        Q_FOREACH (QPointer<KisView> koview, KisPart::instance()->views()) {
            KisViewManager *view = qobject_cast<KisViewManager*>(koview);
            if (view) {
                KisNode* node = dynamic_cast<KisNode*>(view->image()->rootLayer().data());
                if (node) {
                    node->updateSettings();
                }
            }

        }
        updateWindowMenu();

        d->viewManager->showHideScrollbars();
    }
}

void KisMainWindow::updateTheme()
{
    // reload action icons!
    Q_FOREACH (QAction *action, actionCollection()->actions()) {
        KisIconUtils::updateIcon(action);
    }
    if (d->mdiArea) {
        d->mdiArea->setPalette(qApp->palette());
        for (int i=0; i<d->mdiArea->subWindowList().size(); i++) {
            QMdiSubWindow *window = d->mdiArea->subWindowList().at(i);
            if (window) {
                window->setPalette(qApp->palette());
                KisView *view = qobject_cast<KisView*>(window->widget());
                if (view) {
                    view->slotThemeChanged(qApp->palette());
                }
            }
        }
    }

    customizeTabBar();
    applyBrocreateDarkTheme();

    // Update toolbars
    {
        Q_FOREACH (KisToolBar* aToolBar, toolBars()) {
            QObjectList objects;
            objects.append(aToolBar);
            while (!objects.isEmpty()) {
                QWidget* widget = qobject_cast<QWidget*>(objects.takeFirst());
                if (widget) {
                    objects.append(widget->children());
                    widget->setPalette(qApp->palette());
                }
            }
        }
    }
}

void KisMainWindow::slotThemeChanged()
{
    KConfigGroup group(KSharedConfig::openConfig(), "theme");
#ifndef Q_OS_HAIKU
    if (group.readEntry("Theme", "") == d->themeManager->currentThemeName()) return;

    // save theme changes instantly
    group.writeEntry("Theme", d->themeManager->currentThemeName());
#endif
    updateTheme();

    // Make the other top level windows update as well
    Q_FOREACH (QWidget* topLevelWidget, qApp->topLevelWidgets()) {
        if (topLevelWidget == this) {
            continue;
        }
        if (topLevelWidget->isHidden()) {
            continue;
        }
        KisMainWindow *topLevelMainWindow = qobject_cast<KisMainWindow*>(topLevelWidget);
        if (topLevelMainWindow) {
            topLevelMainWindow->updateTheme();
            Q_EMIT topLevelMainWindow->themeChanged();
        } else {
            QObjectList objects;
            objects.append(topLevelWidget);
            while (!objects.isEmpty()) {
                QWidget* widget = qobject_cast<QWidget*>(objects.takeFirst());
                if (widget) {
                    objects.append(widget->children());
                    KisIconUtils::updateIconCommon(widget);
                }
            }
        }
    }

    applyActionIconOverridesFromLocalXML();

    Q_EMIT themeChanged();
}

void KisMainWindow::customizeTabBar()
{
    QString closeButtonImageUrl;
    QString closeButtonHoverColor;
    if (KisIconUtils::useDarkIcons()) {
        closeButtonImageUrl = QStringLiteral(":/dark_close-tab.svg");
        closeButtonHoverColor = QStringLiteral("lightcoral");
    } else {
        closeButtonImageUrl = QStringLiteral(":/light_close-tab.svg");
        closeButtonHoverColor = QStringLiteral("darkred");
    }
    QString tabStyleSheet = QStringLiteral(R"(
            QTabBar::close-button {
                image: url(%1);
                padding: 1.5px;
                border-radius: 3px;
            }
            QTabBar::close-button:hover {
                background-color: %2;
            }
            QTabBar::close-button:pressed {
                background-color: red;
            }

            QHeaderView::section {
                padding: 7px;
            }

           )")
           .arg(closeButtonImageUrl, closeButtonHoverColor);


    QTabBar* tabBar = d->findTabBarHACK();
    if (tabBar) {
        tabBar->setStyleSheet(tabStyleSheet);
    }

}

bool KisMainWindow::canvasDetached() const
{
    return centralWidget() != d->widgetStack;
}

void KisMainWindow::setCanvasDetached(bool detach)
{
#ifdef Q_OS_ANDROID
    if (detach) {
        QMessageBox::warning(this, i18nc("@title:window", "Brocreate"),
                             "Detach Canvas is unsupported on Android");
    }
#else
    if (detach == canvasDetached()) return;

    QWidget *outgoingWidget = centralWidget() ? takeCentralWidget() : nullptr;
    QWidget *incomingWidget = d->canvasWindow->swapMainWidget(outgoingWidget);

    if (incomingWidget) {
        setCentralWidget(incomingWidget);
    }

    if (detach) {
        d->canvasWindow->show();
    } else {
        d->canvasWindow->hide();
    }
    d->toggleDetachCanvas->setChecked(detach);
#endif
}

QWidget * KisMainWindow::canvasWindow() const
{
    return d->canvasWindow;
}

void KisMainWindow::clearRecentFiles()
{
    KisRecentFilesManager::instance()->clear();
}

void KisMainWindow::removeRecentFile(QString url)
{
    KisRecentFilesManager::instance()->remove(QUrl::fromLocalFile(url));
}

void KisMainWindow::slotUpdateSaveActionTitle(const QString &documentPath)
{
    const QString fileName = QFileInfo(documentPath).fileName();

    if (!fileName.isEmpty()) {
        d->saveAction->setToolTip(i18n("Save as %1", fileName));
    }
    else {
        d->saveAction->setToolTip(i18n("Save"));
    }
}

void KisMainWindow::slotUpdateReadWriteMode(bool readWrite)
{
    Q_UNUSED(readWrite);
    d->actionManager()->updateGUI();
}

KisView *KisMainWindow::activeView() const
{
    if (d->activeView) {
        return d->activeView;
    }
    return 0;
}

bool KisMainWindow::openDocument(const QString &path, OpenFlags flags)
{
    ScopedWidgetDisabler disabler(d->welcomePage->dropFrameBorder);
    QApplication::processEvents(); // make UI more responsive

    if (!QFile(path).exists()) {
        if (!(flags & BatchMode)) {
            QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Brocreate"), i18n("The file %1 does not exist.", path));
        }
        KisRecentFilesManager::instance()->remove(QUrl::fromLocalFile(path));
        return false;
    }
    return openDocumentInternal(path, flags);
}

bool KisMainWindow::openDocumentInternal(const QString &path, OpenFlags flags)
{
    if (!QFile(path).exists()) {
        qWarning() << "KisMainWindow::openDocumentInternal. Could not open:" << path;
        return false;
    }

    KisDocument *newdoc = KisPart::instance()->createDocument();

    if (flags & BatchMode) {
        newdoc->setFileBatchMode(true);
    }

    d->firstTime = true;
    connect(newdoc, SIGNAL(completed()), this, SLOT(slotLoadCompleted()));
    connect(newdoc, SIGNAL(canceled(QString)), this, SLOT(slotLoadCanceled(QString)));

    KisDocument::OpenFlags openFlags = KisDocument::None;
    if (flags & RecoveryFile) {
        openFlags |= KisDocument::RecoveryFile;
    }

    bool openRet = !(flags & Import) ? newdoc->openPath(path, openFlags) : newdoc->importDocument(path);

    if (!openRet) {
        delete newdoc;
        return false;
    }

    KisPart::instance()->addDocument(newdoc);

    // Try to determine whether this was an unnamed autosave
    if (flags & RecoveryFile &&
            (   path.startsWith(QDir::tempPath())
                || path.startsWith(QDir::homePath())
                ) &&
            (      QFileInfo(path).fileName().startsWith(".krita")
                   || QFileInfo(path).fileName().startsWith("krita")
                   )
            )
    {
        QString path = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
        if (!QFileInfo(path).exists()) {
            path = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        }
        newdoc->setPath(path + "/" + newdoc->objectName() + ".kra");
    }

    return true;
}

void KisMainWindow::showDocument(KisDocument *document) {
    Q_FOREACH(QMdiSubWindow *subwindow, d->mdiArea->subWindowList()) {
        KisView *view = qobject_cast<KisView*>(subwindow->widget());
        KIS_SAFE_ASSERT_RECOVER_NOOP(view);

        if (view) {
            if (view->document() == document) {
                setActiveSubWindow(subwindow);
                return;
            }
        }
    }

    addViewAndNotifyLoadingCompleted(document);
}

KisView* KisMainWindow::addViewAndNotifyLoadingCompleted(KisDocument *document,
                                                         QMdiSubWindow *subWindow)
{
    showWelcomeScreen(false); // see workaround in function header

    KisView *view = KisPart::instance()->createView(document, d->viewManager, this);
    addView(view, subWindow);

    Q_EMIT guiLoadingFinished();

#ifdef Q_OS_ANDROID
    if (!KisAndroidUtils::looksLikeXiaomiDevice()) {
        slotFlashWindowHack();
    }
#endif

    return view;
}

QStringList KisMainWindow::showOpenFileDialog(bool isImporting)
{
    KoFileDialog dialog(this, KoFileDialog::ImportFiles, "OpenDocument");
    dialog.setDefaultDir(QStandardPaths::writableLocation(QStandardPaths::PicturesLocation));
    dialog.setMimeTypeFilters(KisImportExportManager::supportedMimeTypes(KisImportExportManager::Import));
    dialog.setCaption(isImporting ? i18n("Import Images") : i18n("Open Images"));

    QStringList filenames = dialog.filenames();
    filenames.sort();

    return filenames;
}

// Separate from openDocument to handle async loading (remote URLs)
void KisMainWindow::slotLoadCompleted()
{
    KisDocument *newdoc = qobject_cast<KisDocument*>(sender());
    if (newdoc && newdoc->image()) {
        addViewAndNotifyLoadingCompleted(newdoc);

        disconnect(newdoc, SIGNAL(completed()), this, SLOT(slotLoadCompleted()));
        disconnect(newdoc, SIGNAL(canceled(QString)), this, SLOT(slotLoadCanceled(QString)));

        Q_EMIT loadCompleted();
    }
}

void KisMainWindow::slotLoadCanceled(const QString &errMsg)
{
    KisUsageLogger::log(QString("Loading canceled: %1.").arg(errMsg));
    KisDocument* doc = qobject_cast<KisDocument*>(sender());
    Q_ASSERT(doc);
    disconnect(doc, SIGNAL(completed()), this, SLOT(slotLoadCompleted()));
    disconnect(doc, SIGNAL(canceled(QString)), this, SLOT(slotLoadCanceled(QString)));
}

void KisMainWindow::slotSaveCanceled(const QString &errMsg)
{
    if (!errMsg.isEmpty()) {   // empty when cancelled by user
        KisUsageLogger::log(QString("Saving cancelled. Error:").arg(errMsg));
        QMessageBox::critical(this, i18nc("@title:window", "Brocreate"), errMsg);
    }
    else {
        KisUsageLogger::log(QString("Saving cancelled by the user."));
    }
    slotSaveCompleted();
}

void KisMainWindow::slotSaveCompleted()
{
    KisUsageLogger::log(QString("Saving Completed"));
    KisDocument* doc = qobject_cast<KisDocument*>(sender());
    if (doc) {
        disconnect(doc, SIGNAL(completed()), this, SLOT(slotSaveCompleted()));
        disconnect(doc, SIGNAL(canceled(QString)), this, SLOT(slotSaveCanceled(QString)));
    }

    if (d->deferredClosingEvent) {
        KXmlGuiWindow::closeEvent(d->deferredClosingEvent);
    }
}

bool KisMainWindow::hackIsSaving() const
{
    std::unique_lock<QMutex> l(d->savingEntryMutex, std::try_to_lock);
    return !l.owns_lock();
}

bool KisMainWindow::installBundle(const QString &fileName) const
{
    QFileInfo from(fileName);
    QFileInfo to(QStringLiteral("%1/%2").arg(KoResourcePaths::getAppDataLocation(), from.fileName()));
    if (to.exists()) {
        QFile::remove(to.canonicalFilePath());
    }
    return QFile::copy(fileName, to.absoluteFilePath());
}

QImage KisMainWindow::layoutThumbnail()
{
    int size = 256;
    qreal scale = qreal(size)/qreal(qMax(geometry().width(), geometry().height()));
    QImage layoutThumbnail = QImage(qRound(geometry().width()*scale), qRound(geometry().height()*scale), QImage::Format_ARGB32);
    QPainter gc(&layoutThumbnail);
    gc.fillRect(0, 0, layoutThumbnail.width(), layoutThumbnail.height(), this->palette().dark());

    Q_FOREACH(const QObject *child, children()) {
        if (child->isWidgetType()) {
            const QWidget *w = static_cast<const QWidget *>(child);

            if (w->isVisible() && !w->property("_kis_excludeFromLayoutThumbnail").toBool()) {
                QRect wRect = QRectF(w->geometry().x()*scale
                                     , w->geometry().y()*scale
                                     , w->geometry().width()*scale
                                     , w->geometry().height()*scale
                                     ).toRect();

                wRect = wRect.intersected(layoutThumbnail.rect().adjusted(-1, -1, -1, -1));

                gc.setBrush(this->palette().window());
                if (w == d->widgetStack) {
                    gc.setBrush(d->mdiArea->background());
                }
                gc.setPen(this->palette().windowText().color());
                gc.drawRect(wRect);
            }
        }
    }
    gc.end();
    return layoutThumbnail;
}

bool KisMainWindow::saveDocument(KisDocument *document, bool saveas, bool isExporting, bool isAdvancedExporting )
{
    if (!document) {
        return true;
    }

    std::unique_lock<QMutex> l(d->savingEntryMutex, std::try_to_lock);
    if (!l.owns_lock()) return false;

    // no busy wait for saving because it is dangerous!
    KisDelayedSaveDialog dlg(document->image(), KisDelayedSaveDialog::SaveDialog, 0, this);
    dlg.blockIfImageIsBusy();

    if (dlg.result() == KisDelayedSaveDialog::Rejected) {
        return false;
    }
    else if (dlg.result() == KisDelayedSaveDialog::Ignored) {
        QMessageBox::critical(qApp->activeWindow(),
                              i18nc("@title:window", "Brocreate"),
                              i18n("You are saving a file while the image is "
                                   "still rendering. The saved file may be "
                                   "incomplete or corrupted.\n\n"
                                   "Please select a location where the original "
                                   "file will not be overridden!"));


        saveas = true;
    }

    if (document->isRecovered()) {
        saveas = true;
    }

    if (document->path().isEmpty()) {
        saveas = true;
    }

    QFile target(document->path());
    if (!target.exists()) { // Possible when file is moved/unmounted during Brocreate session.
        saveas = true;
    }

    connect(document, SIGNAL(completed()), this, SLOT(slotSaveCompleted()));
    connect(document, SIGNAL(canceled(QString)), this, SLOT(slotSaveCanceled(QString)));

    QByteArray nativeFormat = document->nativeFormatMimeType();
    QByteArray oldMimeFormat = document->mimeType();

    QUrl suggestedURL = QUrl::fromLocalFile(document->path());

    QStringList mimeFilter = KisImportExportManager::supportedMimeTypes(KisImportExportManager::Export);

    mimeFilter = KisImportExportManager::supportedMimeTypes(KisImportExportManager::Export);
    if (!mimeFilter.contains(oldMimeFormat)) {
        dbgUI << "KisMainWindow::saveDocument no export filter for" << oldMimeFormat;

        QString suggestedFilename = QFileInfo(suggestedURL.toLocalFile()).completeBaseName();

        if (!suggestedFilename.isEmpty()) {
            suggestedFilename = suggestedFilename + "." + KisMimeDatabase::suffixesForMimeType(KIS_MIME_TYPE).first();
            suggestedURL = suggestedURL.adjusted(QUrl::RemoveFilename);
            suggestedURL.setPath(suggestedURL.path() + suggestedFilename);
        }

        // force the user to choose outputMimeType
        saveas = true;
    }

    bool ret = false;

    if (document->path().isEmpty() || isExporting || saveas) {
        bool justChangingFilterOptions = false;

        KoFileDialog dialog(this, KoFileDialog::SaveFile, "SaveAs");
        dialog.setCaption(isExporting ? i18n("Exporting") : i18n("Saving As"));

        if (isExporting && !d->lastExportLocation.isEmpty() && !d->lastExportLocation.contains(QDir::tempPath())) {

            QString proposedPath = QFileInfo(d->lastExportLocation).absolutePath();
            QString proposedFileName = suggestedURL.isEmpty() ? document->documentInfo()->aboutInfo("title") :  QFileInfo(suggestedURL.toLocalFile()).completeBaseName();
            QString proposedMimeType =  d->lastExportedFormat.isEmpty() ? "" : d->lastExportedFormat;
            QString proposedExtension = KisMimeDatabase::suffixesForMimeType(proposedMimeType).first().remove("*,");

            dialog.setDefaultDir(proposedPath + "/" + proposedFileName + "." + proposedExtension, true);
            dialog.setMimeTypeFilters(mimeFilter, proposedMimeType);
        }
        else {
            KisConfig cfg(true);
            QByteArray default_mime_type = cfg.exportMimeType(false).toUtf8();
            QString proposedMimeType = QString::fromLatin1(default_mime_type);

            KConfigGroup group =  KSharedConfig::openConfig()->group("File Dialogs");
            QString proposedPath = group.readEntry("SaveAs", "");
            if (proposedPath.isEmpty()) {
                proposedPath = group.readEntry("OpenDocument", "");
            }
            if (proposedPath.isEmpty()) {
                proposedPath = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
            }

            if(default_mime_type == "all/mime"){
                dialog.setDefaultDir(suggestedURL.isEmpty() ? proposedPath : proposedPath + "/" + QFileInfo(suggestedURL.toLocalFile()).completeBaseName(), true);
            }

            if (default_mime_type != "all/mime" && !default_mime_type.isEmpty()) {
                QString proposedExtension = KisMimeDatabase::suffixesForMimeType(proposedMimeType).first().remove("*,");
                dialog.setDefaultDir(suggestedURL.isEmpty() ? proposedPath :  proposedPath + "/" + QFileInfo(suggestedURL.toLocalFile()).completeBaseName() + "." + proposedExtension, true);
            }

            if (!isExporting) {
                dialog.setDefaultDir(suggestedURL.isEmpty() ? proposedPath : suggestedURL.toLocalFile(), true);
                default_mime_type = document->mimeType().isEmpty() ? nativeFormat : document->mimeType();
            }
            dialog.setMimeTypeFilters(mimeFilter, QString::fromLatin1(default_mime_type));
        }

        QString newFilePath = dialog.filename();

        bool bOk = true;
        if (newFilePath.isEmpty()) {
            bOk = false;
        }

        if (bOk) {
            if (document->documentInfo()->aboutInfo("title") == i18n("Unnamed")) {
                QString fn = newFilePath;
                QFileInfo info(fn);
                document->documentInfo()->setAboutInfo("title", info.completeBaseName());
            }

            QByteArray outputFormat = nativeFormat;

            QString outputFormatString = KisMimeDatabase::mimeTypeForFile(newFilePath, false);
            outputFormat = outputFormatString.toLatin1();


            if (!isExporting) {
                justChangingFilterOptions = (newFilePath == document->path()) && (outputFormat == document->mimeType());
            }
            else {
                QString path = QFileInfo(d->lastExportLocation).absolutePath();
                QString filename = QFileInfo(document->path()).completeBaseName();
                justChangingFilterOptions = (QFileInfo(newFilePath).absolutePath() == path)
                        && (QFileInfo(newFilePath).completeBaseName() == filename)
                        && (outputFormat == d->lastExportedFormat);
            }

            bool wantToSave = true;

            if (!justChangingFilterOptions) {
                if (!document->isNativeFormat(outputFormat))
                    wantToSave = true;
            }

            if (wantToSave) {
                if (!isExporting) {  // Save As
                    ret = document->saveAs(newFilePath, outputFormat, true);
                    if (ret) {
                        dbgUI << "Successful Save As!";
                        KisPart::instance()->queueAddRecentURLToAllMainWindowsOnFileSaved(QUrl::fromLocalFile(newFilePath));
                    } else {
                        dbgUI << "Failed Save As!";

                    }
                }
                else { // Export
                    ret = document->exportDocument(newFilePath, outputFormat, isAdvancedExporting, false);
                    if (ret) {
                        d->lastExportLocation = newFilePath;
                        d->lastExportedFormat = outputFormat;
                    }
                }

            }
            else
                ret = false;
        }
        else
            ret = false;
    } else { // saving
        KIS_ASSERT_RECOVER_NOOP(!isExporting);

        if (document->isModified()) {
            ret = document->save(true, 0);
        }

        if (!ret) {
            dbgUI << "Failed Save!";
        }
    }

    return ret;
}

void KisMainWindow::undo()
{
    if (activeView()) {
        activeView()->document()->undoStack()->undo();
    }
}

void KisMainWindow::redo()
{
    if (activeView()) {
        activeView()->document()->undoStack()->redo();
    }
}

void KisMainWindow::closeEvent(QCloseEvent *e)
{
    if (hackIsSaving()) {
        e->setAccepted(false);
        return;
    }

    if (!KisPart::instance()->closingSession()) {
        QAction *action= d->viewManager->actionCollection()->action("view_show_canvas_only");
        if ((action) && (action->isChecked())) {
            action->setChecked(false);
        }

        // Save session when last window is closed
        if (KisPart::instance()->mainwindowCount() == 1) {
            bool closeAllowed = KisPart::instance()->closeSession();

            if (!closeAllowed) {
                e->setAccepted(false);
                return;
            }
        }
    }

    d->mdiArea->closeAllSubWindows();

    QList<QMdiSubWindow*> childrenList = d->mdiArea->subWindowList();

    if (childrenList.isEmpty()) {
        d->deferredClosingEvent = e;
        d->canvasWindow->close();
    } else {
        e->setAccepted(false);
    }
}

void KisMainWindow::saveWindowSettings()
{
    KSharedConfigPtr config =  KSharedConfig::openConfig();

    if (d->windowSizeDirty ) {
        dbgUI << "KisMainWindow::saveWindowSettings";
        KConfigGroup group = d->windowStateConfig;
        KWindowConfig::saveWindowSize(windowHandle(), group);
        config->sync();
        d->windowSizeDirty = false;
    }

    if (!d->activeView || d->activeView->document()) {
        KConfigGroup group = d->windowStateConfig;
        saveMainWindowSettings(group);

        for (QMap<QString, QDockWidget*>::const_iterator i = d->dockWidgetsMap.constBegin();
             i != d->dockWidgetsMap.constEnd(); ++i) {
            if (i.value()->widget()) {
                KConfigGroup dockGroup = group.group(QString("DockWidget ") + i.key());
                dockGroup.writeEntry("Locked", i.value()->property("Locked").toBool());
                dockGroup.writeEntry("DockArea", (int) dockWidgetArea(i.value()));
                dockGroup.writeEntry("xPosition", (int) i.value()->widget()->x());
                dockGroup.writeEntry("yPosition", (int) i.value()->widget()->y());

                dockGroup.writeEntry("width", (int) i.value()->widget()->width());
                dockGroup.writeEntry("height", (int) i.value()->widget()->height());
            }
        }

    }

    KSharedConfig::openConfig()->sync();
    resetAutoSaveSettings();

}

void KisMainWindow::resizeEvent(QResizeEvent * e)
{
    d->windowSizeDirty = true;
    KXmlGuiWindow::resizeEvent(e);
    slotUpdateBrocreateOverlayGeometry();
}


void KisMainWindow::dragMoveEvent(QDragMoveEvent *event)
{
    dragMove(event);
    event->accept();
}

void KisMainWindow::dragLeaveEvent(QDragLeaveEvent *event)
{
    dragLeave();
    event->accept();
}

bool KisMainWindow::windowsLayoutSavingAllowed() const
{
    QAction *action= d->viewManager->actionCollection()->action("view_show_canvas_only");
    return !action || !action->isChecked();
}

void KisMainWindow::showEvent(QShowEvent *event)
{
    if (!event->spontaneous()) {
        setMainWindowLayoutForCurrentMainWidget(d->widgetStack->currentIndex(), false);
    }
    if (menuBar()) {
        menuBar()->setVisible(false);
    }
    if (statusBar()) {
        statusBar()->hide();
    }
    slotUpdateBrocreateOverlayGeometry();
#ifdef Q_OS_ANDROID
    KisAndroidDonations *androidDonations = KisAndroidDonations::instance();
    if (androidDonations && androidDonations->isShowDonationManagementDialogPending()) {
        QTimer::singleShot(0, this, &KisMainWindow::slotShowDonationManagementDialog);
    }
#endif
    return KXmlGuiWindow::showEvent(event);
}

void KisMainWindow::setMainWindowLayoutForCurrentMainWidget(int widgetIndex, bool widgetIndexChanged)
{
    if (widgetIndex == 0) {
        if (widgetIndexChanged) {
            if (d->mdiArea->subWindowList().isEmpty()) {
                saveWindowState(true);
            } else {
                saveMainWindowSettings(d->windowStateConfig);
            }
        }
        adjustLayoutForWelcomePage();
        updateBrocreateFloatingVisibility(false);
    }
    else {
        setAutoSaveSettings(d->windowStateConfig, false);
        if (menuBar()) {
            menuBar()->setVisible(false);
        }
        if (statusBar()) {
            statusBar()->setVisible(false);
        }
        QList<QToolBar *> toolbars = findChildren<QToolBar *>();
        for (QToolBar *toolbar : toolbars) {
            toolbar->hide();
        }
        arrangeBrocreateStudioDocks(d->animationLayoutActive);
        updateBrocreateFloatingVisibility(!d->canvasOnlyOverlayHidden);
        slotUpdateBrocreateOverlayGeometry();
    }

    QList<QAction *> actions = d->dockWidgetMenu->menu()->actions();
    actions.append(toolBarMenuAction()->menu()->actions());
    for (QAction *action : actions) {
        if (action) {
            action->setEnabled(widgetIndex);
        }
    }
}

void KisMainWindow::adjustLayoutForWelcomePage()
{
    resetAutoSaveSettings();

    toggleDockersVisibility(false, true);
    if (menuBar()) {
        menuBar()->setVisible(false);
    }
    if (statusBar()) {
        statusBar()->hide();
    }
    QList<QToolBar *> toolbars = findChildren<QToolBar *>();
    for (QToolBar *toolbar : toolbars) {
        toolbar->hide();
    }
}

void KisMainWindow::setActiveView(KisView* view)
{
    d->activeView = view;

    if (d->undoActionsUpdateManager) {
        d->undoActionsUpdateManager->setCurrentDocument(view ? view->document() : 0);
    }

    d->viewManager->setCurrentView(view);

    d->activeViewConnections.clear();
    d->activeViewConnections.addConnection(view->document(),
                                           SIGNAL(sigPathChanged(QString)),
                                           this, SLOT(slotUpdateSaveActionTitle(QString)));
    slotUpdateSaveActionTitle(view->document()->path());
    d->activeViewConnections.addConnection(view->document(),
                                           SIGNAL(sigReadWriteChanged(bool)),
                                           this, SLOT(slotUpdateReadWriteMode(bool)));
    slotUpdateReadWriteMode(view->document()->isReadWrite());

    KisWindowLayoutManager::instance()->activeDocumentChanged(view->document());

    if (d->viewManager && d->viewManager->canvasResourceProvider()) {
        d->activeViewConnections.addConnection(
            d->viewManager->canvasResourceProvider()->resourceManager(),
            SIGNAL(canvasResourceChanged(int,QVariant)),
            this,
            SLOT(slotBrocreateCanvasResourceChanged(int,QVariant)));
        updateBrocreateColorSwatch();
        d->updatingFromResource = true;
        if (d->opacityPill) {
            const qreal op = d->viewManager->canvasResourceProvider()->opacity();
            d->opacityPill->setNormalizedValue(op);
            d->opacityPill->setDisplayText(QString("%1%").arg(qRound(op * 100.0)));
        }
        if (d->brushSizePill) {
            const qreal sz = d->viewManager->canvasResourceProvider()->resourceManager()->resource(KoCanvasResource::Size).toDouble();
            const qreal effectiveSz = sz > 0.1 ? sz : 5.0;
            const qreal norm = qBound<qreal>(0.0, std::log(effectiveSz) / std::log(500.0), 1.0);
            d->brushSizePill->setNormalizedValue(norm);
            d->brushSizePill->setDisplayText(QString::number(effectiveSz, 'f', 2) + QStringLiteral(" px"));
        }
        d->updatingFromResource = false;
    }

    Q_EMIT activeViewChanged();
}

void KisMainWindow::unsetActiveView()
{
    d->activeViewConnections.clear();
    slotUpdateSaveActionTitle(QString());
    slotUpdateReadWriteMode(false);
}

void KisMainWindow::dragMove(QDragMoveEvent * event)
{
    QTabBar *tabBar = d->findTabBarHACK();

    if (!tabBar && d->mdiArea->viewMode() == QMdiArea::TabbedView) {
        qWarning() << "WARNING!!! Cannot find QTabBar in the main window!";
    }

    if (tabBar && tabBar->isVisible()) {
        QPoint pos = tabBar->mapFromGlobal(mapToGlobal(event->pos()));
        if (tabBar->rect().contains(pos)) {
            const int tabIndex = tabBar->tabAt(pos);

            if (tabIndex >= 0 && tabBar->currentIndex() != tabIndex) {
                d->tabSwitchCompressor->start(tabIndex);
            }
        } else if (d->tabSwitchCompressor->isActive()) {
            d->tabSwitchCompressor->stop();
        }
    }
}

void KisMainWindow::dragLeave()
{
    if (d->tabSwitchCompressor->isActive()) {
        d->tabSwitchCompressor->stop();
    }
}


void KisMainWindow::switchTab(int index)
{
    QTabBar *tabBar = d->findTabBarHACK();
    if (!tabBar) return;

    tabBar->setCurrentIndex(index);
}

void KisMainWindow::showWelcomeScreen(bool show)
{
    const int currentIndex = show ? 0 : 1;
    if (d->widgetStack->currentIndex() != currentIndex) {
        setUpdatesEnabled(false);
        if (show) {
            setMainWindowLayoutForCurrentMainWidget(currentIndex, true);
            d->widgetStack->setCurrentIndex(currentIndex);
        } else {
            d->widgetStack->setCurrentIndex(currentIndex);
            setMainWindowLayoutForCurrentMainWidget(currentIndex, true);
        }
        setUpdatesEnabled(true);
    }
}

void KisMainWindow::slotFileNew()
{
    KisDlgCreateNewDocument* dlg = new KisDlgCreateNewDocument(this);
    dlg->exec();
    dlg->deleteLater();
}

void KisMainWindow::slotImportFile()
{
    dbgUI << "slotImportFile()";
    slotFileOpen(true);
}


void KisMainWindow::slotFileOpen(bool isImporting)
{
    QStringList urls = showOpenFileDialog(isImporting);

    if (urls.isEmpty())
        return;

    Q_FOREACH (const QString& url, urls) {

        if (!url.isEmpty()) {
            OpenFlags flags = isImporting ? Import : None;
            bool res = openDocument(url, flags);
            if (!res) {
                warnKrita << "Loading" << url << "failed";
            }
        }
    }
}

void KisMainWindow::slotFileOpenRecent(const QUrl &url)
{
    (void) openDocument(url.toLocalFile(), None);
}

void KisMainWindow::slotFileSave()
{
    if (d->activeView && saveDocument(d->activeView->document(), false, false, false)) {
        Q_EMIT documentSaved();
    }
}

void KisMainWindow::slotChangeInterfaceScale()
{
#if defined(Q_OS_ANDROID) && KRITA_QT_HAS_ANDROID_QPLATFORMSCREEN_DENSITY_ADJUSTMENT
    KisAndroidScaling *androidScaling = KisAndroidScaling::instance();
    if (androidScaling) {
        androidScaling->showDialog();
    }
#endif
}

void KisMainWindow::slotFileSaveAs()
{
    if (d->activeView && saveDocument(d->activeView->document(), true, false, false)) {
        Q_EMIT documentSaved();
    }
}

void KisMainWindow::slotExportFile()
{
    if (d->activeView && saveDocument(d->activeView->document(), true, true, false)) {
        Q_EMIT documentSaved();
    }
}
void KisMainWindow::slotExportAdvance()
{
    if (d->activeView && saveDocument(d->activeView->document(), true, true, true)) {
        Q_EMIT documentSaved();
    }
}

void KisMainWindow::slotShowSessionManager() {
    KisPart::instance()->showSessionManager();
}

#ifdef Q_OS_ANDROID
void KisMainWindow::slotShowDonationManagementDialog()
{
    QWidget *win = qApp->activeWindow();
    if (win && !qobject_cast<KisMainWindow *>(win) && (win->isModal() || win->windowModality() != Qt::NonModal)) {
        return;
    }

    QString objectName = QStringLiteral("kisdonationmanagementdialog");
    KisDonationManagementDialog *dlg = findChild<KisDonationManagementDialog *>(objectName, Qt::FindDirectChildrenOnly);
    if (dlg) {
        dlg->reshow();
    } else {
        dlg = new KisDonationManagementDialog(this);

        QAction *action = actionCollection()->action("manage_supporter_bundles");
        if (action) {
            connect(dlg, &KisDonationManagementDialog::sigShowSupporterBundles, action, &QAction::trigger);
        }

        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->setObjectName(objectName);
        dlg->show();
    }
}

void KisMainWindow::slotFlashWindowHack()
{
    if (!d->flashWindowHackInProgress) {
        d->flashWindowHackInProgress = true;
        QTimer::singleShot(0, this, [this] {
            hide();
            QTimer::singleShot(0, this, [this] {
                show();
                d->flashWindowHackInProgress = false;
            });
        });
    }
}
#endif

KoCanvasResourceProvider *KisMainWindow::resourceManager() const
{
    return d->viewManager->canvasResourceProvider()->resourceManager();
}

int KisMainWindow::viewCount() const
{
    return d->mdiArea->subWindowList().size();
}

const KConfigGroup &KisMainWindow::windowStateConfig() const
{
    return d->windowStateConfig;
}

void KisMainWindow::saveWindowState(bool restoreNormalState)
{
    if (d->widgetStack->currentIndex() == 0) {
        return;
    }

    if (restoreNormalState) {
        QAction *showCanvasOnly = d->viewManager->actionCollection()->action("view_show_canvas_only");

        if (showCanvasOnly && showCanvasOnly->isChecked()) {
            showCanvasOnly->setChecked(false);
        }

        d->windowStateConfig.writeEntry("ko_geometry", saveGeometry().toBase64());
        d->windowStateConfig.writeEntry("State", saveState().toBase64());

        if (!d->toggleDockers->isChecked()) {
            restoreState(d->dockerStateBeforeHiding);
        }

        saveWindowSettings();

    } else {
        saveMainWindowSettings(d->windowStateConfig);
    }

}

bool KisMainWindow::restoreWorkspaceState(const QByteArray &state)
{
    QByteArray oldState = saveState();
    const bool showTitlebars = false;

    Q_FOREACH (QDockWidget *dock, dockWidgets()) {
        if (dock) {
            dock->setProperty("Locked", false);
            dock->toggleViewAction()->setEnabled(true);
            dock->hide();
            if (dock->titleBarWidget() && !dock->titleBarWidget()->inherits("KisUtilityTitleBar")) {
                dock->titleBarWidget()->setVisible(showTitlebars);
            }
        }
    }

    bool success = KXmlGuiWindow::restoreState(state);

    if (!success) {
        KXmlGuiWindow::restoreState(oldState);
        Q_FOREACH (QDockWidget *dock, dockWidgets()) {
            if (dock->titleBarWidget() && !dock->titleBarWidget()->inherits("KisUtilityTitleBar")) {
                dock->titleBarWidget()->setVisible(showTitlebars || dock->isFloating());
            }
        }
        return false;
    }
    return success;
}

void KisMainWindow::restoreWorkspace()
{
    QString md5 = sender()->property("md5").toString();
    KoResourceServer<KisWorkspaceResource> *rserver = KisResourceServerProvider::instance()->workspaceServer();
    KoResourceSP resource = rserver->resource(md5, "", "");
    if (resource) {
        restoreWorkspace(resource);
    }
    else {
        qWarning() << "Could not retrieve resource for" << md5;
    }
}

void KisMainWindow::openCommandBar()
{
    QList<KisKActionCollection *> actionCollections;

    auto clients = guiFactory()->clients();
    int actionsCount = 0;
    for (const KisKXMLGUIClient *c : clients) {
        if (!c) {
            continue;
        }
        if (auto collection = c->actionCollection()) {
            actionCollections.append(collection);
            actionsCount += collection->count();
        }
    }

    if (activeKisView()) {
        KisKActionCollection *layerActionCollection = new KisKActionCollection(0, "layeractions (disposable)");
        layerActionCollection->setComponentDisplayName(i18n("Layers/Masks"));
        KisNodeActivationActionCreatorVisitor v(layerActionCollection, viewManager()->nodeManager());
        activeKisView()->image()->rootLayer()->accept(v);
        actionCollections.append(layerActionCollection);
        actionsCount += layerActionCollection->count();
    }

    d->commandBar->updateBar(actionCollections, actionsCount);
    d->commandBar->activateWindow();
}

void KisMainWindow::slotStoragesWarning(const QString &/*location*/)
{
    QString warning;
    if (!checkActiveBundlesAvailable()) {
        warning = i18n("You don't have any resource bundles enabled.");
    }

    if (!checkPaintOpAvailable()) {
        warning += i18n("\nThere are no brush presets available. Please enable a bundle that has presets before continuing.\n");
        QMessageBox::critical(this, i18nc("@title:window", "Brocreate"), warning);

        QAction *action = actionCollection()->action("manage_bundles");
        if (action) {
            action->trigger();
        }
    }

    if (!checkActiveBundlesAvailable()) {
        QMessageBox::warning(this, i18nc("@title:window", "Brocreate"), warning + i18n("\nOnly your local resources are available."));
    }

}

bool KisMainWindow::restoreWorkspace(KoResourceSP res)
{
    KisWorkspaceResourceSP workspace = res.dynamicCast<KisWorkspaceResource>();

    bool success = restoreWorkspaceState(workspace->dockerState());

    const bool showTitlebars = false;
    Q_FOREACH (QDockWidget *dock, dockWidgets()) {
        if (dock->titleBarWidget() && !dock->titleBarWidget()->inherits("KisUtilityTitleBar")) {
            dock->titleBarWidget()->setVisible(showTitlebars || dock->isFloating());
        }
    }

    if (activeKisView()) {
        activeKisView()->resourceProvider()->notifyLoadingWorkspace(workspace);
    }

    d->viewManager->notifyWorkspaceLoaded();

    return success;
}

QByteArray KisMainWindow::borrowWorkspace(KisMainWindow *other)
{
    QByteArray currentWorkspace = saveState();

    if (!d->workspaceBorrowedBy.isNull()) {
        if (other->id() == d->workspaceBorrowedBy) {
            d->workspaceBorrowedBy = QUuid();
            return currentWorkspace;
        } else {
            KisMainWindow *borrower = KisPart::instance()->windowById(d->workspaceBorrowedBy);
            if (borrower) {
                QByteArray originalLayout = borrower->borrowWorkspace(this);
                borrower->restoreWorkspaceState(currentWorkspace);

                d->workspaceBorrowedBy = other->id();
                return originalLayout;
            }
        }
    }

    d->workspaceBorrowedBy = other->id();
    return currentWorkspace;
}

void KisMainWindow::swapWorkspaces(KisMainWindow *a, KisMainWindow *b)
{
    QByteArray workspaceA = a->borrowWorkspace(b);
    QByteArray workspaceB = b->borrowWorkspace(a);

    a->restoreWorkspaceState(workspaceB);
    b->restoreWorkspaceState(workspaceA);
}

KisViewManager *KisMainWindow::viewManager() const
{
    return d->viewManager;
}

void KisMainWindow::slotDocumentInfo()
{
    if (!d->activeView || !d->activeView->document())
        return;

    KoDocumentInfo *docInfo = d->activeView->document()->documentInfo();

    if (!docInfo)
        return;

    KoDocumentInfoDlg *dlg = d->activeView->document()->createDocumentInfoDialog(this, docInfo);

    if (dlg->exec()) {
        if (dlg->isDocumentSaved()) {
            d->activeView->document()->setModified(false);
        } else {
            d->activeView->document()->setModified(true);
        }
    }

    delete dlg;
}

bool KisMainWindow::slotFileCloseAll()
{
    Q_FOREACH (QMdiSubWindow *subwin, d->mdiArea->subWindowList()) {
        if (subwin) {
            if(!subwin->close())
                return false;
        }
    }

    return true;
}

void KisMainWindow::slotFileQuit()
{
    if (hackIsSaving()) {
        return;
    }
    KisPart::instance()->closeSession();
}

void KisMainWindow::importAnimation()
{
    if (!activeView()) return;

    KisDocument *document = activeView()->document();
    if (!document) return;

    KisDlgImportImageSequence dlg(this, document);

    if (dlg.exec() == QDialog::Accepted) {
        QStringList files = dlg.files();
        int firstFrame = dlg.firstFrame();
        int step = dlg.step();
        bool startFrom1 = dlg.startFrom1();
        bool autoAddHoldframes = dlg.autoAddHoldframes();


        KoUpdaterPtr updater =
                !document->fileBatchMode() ? viewManager()->createUnthreadedUpdater(i18n("Import frames")) : 0;
        KisAnimationImporter importer(document->image(), updater);
        int isAscending = dlg.isAscending();
        KisImportExportErrorCode status = importer.import(files, firstFrame, step, autoAddHoldframes, startFrom1, isAscending);

        if (!status.isOk() && !status.isInternalError()) {
            QString msg = status.errorMessage();
            if (!msg.isEmpty())
                QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Brocreate"), i18n("Could not finish import animation:\n%1", msg));
        }
        activeView()->canvasBase()->refetchDataFromImage();
    }
}

void KisMainWindow::importVideoAnimation()
{
#ifndef Q_OS_ANDROID
    KisDocument *document;
    KisDlgImportVideoAnimation dlg(this, activeView());

    if (dlg.exec() == QDialog::Accepted) {
        const QTemporaryDir outputLocation(QDir::tempPath() + QDir::separator() + "krita" + QDir::separator() + "import_files");
        RenderedFrames renderedFrames = dlg.renderFrames(QDir(outputLocation.path()));
        dbgFile << "Frames rendered to directory: " << outputLocation.path();
        QStringList documentInfoList = dlg.documentInfo();

        if (renderedFrames.isEmpty()) return;

        dbgFile << "Animation Import options: " << documentInfoList;

        int firstFrame = 0;
        const int step = documentInfoList[0].toInt();
        const int fps = documentInfoList[1].toInt();
        const int totalFrames = renderedFrames.framesNeedRelocation() ? (renderedFrames.renderedFrameTargetTimes.last() + 1) : renderedFrames.size() * step;
        const QString name = QFileInfo(documentInfoList[3]).fileName();
        const bool useCurrentDocument = documentInfoList[4].toInt();
        bool useDocumentColorSpace = false;

        if ( useCurrentDocument ) {
            document = activeView()->document();

            dbgFile << "Current frames:" << document->image()->animationInterface()->totalLength() << "total frames:" << totalFrames;
            if ( document->image()->animationInterface()->totalLength() < totalFrames ) {
                document->image()->animationInterface()->setDocumentRangeStartFrame(0);
                document->image()->animationInterface()->setDocumentRangeEndFrame(totalFrames);
            }
        } else {
            const int width = documentInfoList[5].toInt();
            const int height = documentInfoList[6].toInt();
            const double resolution = documentInfoList[7].toDouble();

            const QString colorModel = documentInfoList[8];
            const QString colorDepth = documentInfoList[9];
            const QString profile = documentInfoList[10];
            useDocumentColorSpace = profile != "Default";

            document = KisPart::instance()->createDocument();
            document->setObjectName(name);

            KisPart::instance()->addDocument(document, false);
            const KoColorSpace *cs = KoColorSpaceRegistry::instance()->colorSpace(colorModel, colorDepth, profile);
            Q_ASSERT(cs);

            QColor qc(Qt::white);
            qc.setAlpha(0);
            KoColor bgColor(qc, cs);

            if (!document->newImage(name, width, height, cs, bgColor, KisConfig::RASTER_LAYER, 1, "", double(resolution / 72) )) {
                QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Brocreate"), i18n("Failed to create new document. Animation import aborted."));
                return;
            }

            document->image()->animationInterface()->setFramerate(fps);
            document->image()->animationInterface()->setDocumentRangeStartFrame(0);
            document->image()->animationInterface()->setDocumentRangeEndFrame(totalFrames);

            this->showDocument(document);
        }

        KoUpdaterPtr updater =
                !document->fileBatchMode() ? viewManager()->createUnthreadedUpdater(i18n("Import frames")) : 0;
        KisAnimationImporter importer(document->image(), updater);
        KisImportExportErrorCode status = importer.import(renderedFrames.renderedFrameFiles, firstFrame, step, false, false, 0, useDocumentColorSpace, renderedFrames.renderedFrameTargetTimes);

        if (!status.isOk() && !status.isInternalError()) {
            QString msg = status.errorMessage();
            if (!msg.isEmpty())
                QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Brocreate"), i18n("Could not finish import animation:\n%1", msg));
        }

        activeView()->canvasBase()->refetchDataFromImage();
        document->image()->refreshGraphAsync();
        document->image()->waitForDone();

    }
#endif
}

void KisMainWindow::renderAnimation()
{
    if (!activeView()) return;

    KisImageSP image = viewManager()->image();

    if (!image) return;
    if (!image->animationInterface()->hasAnimation()) return;

    KisDocument *doc = viewManager()->document();

    KisDlgAnimationRenderer dlgAnimationRenderer(doc, viewManager()->mainWindow());
    dlgAnimationRenderer.setCaption(i18n("Render Animation"));
    if (dlgAnimationRenderer.exec() == QDialog::Accepted) {
        KisAnimationRenderingOptions encoderOptions = dlgAnimationRenderer.getEncoderOptions();
        KisAnimationRender::render(doc, viewManager(), encoderOptions);
    }
}

void KisMainWindow::renderAnimationAgain()
{
    if (!activeView()) return;

    KisImageSP image = viewManager()->image();

    if (!image) return;
    if (!image->animationInterface()->hasAnimation()) return;

    KisDocument *doc = viewManager()->document();

    KisConfig cfg(true);

    KisPropertiesConfigurationSP settings = cfg.exportConfiguration("ANIMATION_EXPORT");

    KisAnimationRenderingOptions encoderOptions;
    encoderOptions.fromProperties(settings);

    KisAnimationRender::render(doc, viewManager(), encoderOptions);
}

void KisMainWindow::slotConfigureToolbars()
{
    saveWindowState();
    KisKEditToolBar edit(factory(), this);
    connect(&edit, SIGNAL(newToolBarConfig()), this, SLOT(slotNewToolbarConfig()));
    (void) edit.exec();
    applyToolBarLayout();
}

void KisMainWindow::slotResetConfigurations()
{
    KisApplication *kisApp = static_cast<KisApplication*>(qApp);
    kisApp->askResetConfig();
}

void KisMainWindow::slotNewToolbarConfig()
{
    applyMainWindowSettings(d->windowStateConfig);

    KisKXMLGUIFactory *factory = guiFactory();
    Q_UNUSED(factory);

    if (!d->activeView)
        return;

    plugActionList("toolbarlist", d->toolbarList);

    applyActionIconOverridesFromLocalXML();
    applyToolBarLayout();
}

void KisMainWindow::slotToolbarToggled(bool toggle)
{
    KisToolBar * bar = toolBar(sender()->objectName());
    if (bar) {
        if (toggle) {
            bar->show();
        }
        else {
            bar->hide();
        }

        if (d->activeView && d->activeView->document()) {
            saveWindowState();
        }
    } else
        warnUI << "slotToolbarToggled : Toolbar " << sender()->objectName() << " not found!";
}

void KisMainWindow::viewFullscreen(bool fullScreen)
{
#ifdef Q_OS_ANDROID
    KisAndroidUtils::setFullScreen(fullScreen);
    d->fullScreenMode->setChecked(fullScreen);
#else
    KisConfig cfg(false);
    cfg.setFullscreenMode(fullScreen);

    if (fullScreen) {
        setWindowState(windowState() | Qt::WindowFullScreen);
    } else {
        setWindowState(windowState() & ~Qt::WindowFullScreen);
    }
    d->fullScreenMode->setChecked(isFullScreen());
#endif
}

QDockWidget* KisMainWindow::createDockWidget(KoDockFactoryBase* factory)
{
    QDockWidget* dockWidget = 0;
    const bool showTitlebars = false;

    if (!d->dockWidgetsMap.contains(factory->id())) {
        KisScopedPerformanceLogger perfLog(QStringLiteral("KisMainWindow::createDockWidget(%1)").arg(factory->id()));
        dockWidget = factory->createDockWidget();
        KAcceleratorManager::setNoAccel(dockWidget);

        if (!dockWidget) {
            warnKrita << "Could not create docker for" << factory->id();
            return 0;
        }

        KoDockWidgetTitleBar *titleBar = dynamic_cast<KoDockWidgetTitleBar*>(dockWidget->titleBarWidget());

        if (!dockWidget->titleBarWidget()) {
            titleBar = new KoDockWidgetTitleBar(dockWidget);
            dockWidget->setTitleBarWidget(titleBar);
        }
        if (titleBar) {
            titleBar->setFont(KisUiFont::dockFont());
        }

        if (dockWidget->titleBarWidget() && !dockWidget->titleBarWidget()->inherits("KisUtilityTitleBar")) {
            dockWidget->titleBarWidget()->setVisible(showTitlebars);
        }

        dockWidget->setObjectName(factory->id());
        dockWidget->setParent(this);

        if (dockWidget->widget() && dockWidget->widget()->layout())
            dockWidget->widget()->layout()->setContentsMargins(4, 4, 4, 4);

        Qt::DockWidgetArea side = Qt::RightDockWidgetArea;
        bool visible = true;

        switch (factory->defaultDockPosition()) {
        case KoDockFactoryBase::DockTornOff:
            dockWidget->setFloating(true);
            break;
        case KoDockFactoryBase::DockTop:
            side = Qt::TopDockWidgetArea; break;
        case KoDockFactoryBase::DockLeft:
            side = Qt::LeftDockWidgetArea; break;
        case KoDockFactoryBase::DockBottom:
            side = Qt::BottomDockWidgetArea; break;
        case KoDockFactoryBase::DockRight:
            side = Qt::RightDockWidgetArea; break;
        case KoDockFactoryBase::DockMinimized:
        default:
            side = Qt::RightDockWidgetArea;
            visible = false;
        }

        KConfigGroup group = d->windowStateConfig.group("DockWidget " + factory->id());
        side = static_cast<Qt::DockWidgetArea>(group.readEntry("DockArea", static_cast<int>(side)));
        if (side == Qt::NoDockWidgetArea) side = Qt::RightDockWidgetArea;

        addDockWidget(side, dockWidget);
        if (!visible) {
            dockWidget->hide();
        }

        bool locked = group.readEntry("Locked", false);
        if (titleBar && locked) {
            titleBar->setLocked(true);
        }
        else if ( dockWidget->titleBarWidget()->inherits("KisUtilityTitleBar") && locked){
            KisUtilityTitleBar *utilityTitleBar = dynamic_cast<KisUtilityTitleBar*>(dockWidget->titleBarWidget());
            utilityTitleBar->setLocked(true);
        }

        d->dockWidgetsMap.insert(factory->id(), dockWidget);
    }
    else {
        dockWidget = d->dockWidgetsMap[factory->id()];
    }

#ifdef Q_OS_MACOS
    dockWidget->setAttribute(Qt::WA_MacSmallSize, true);
#endif
    dockWidget->setFont(KisUiFont::dockFont());

    connect(dockWidget, SIGNAL(dockLocationChanged(Qt::DockWidgetArea)), this, SLOT(forceDockTabFonts()));

    return dockWidget;
}

void KisMainWindow::forceDockTabFonts()
{
    Q_FOREACH (QObject *child, children()) {
        if (child->inherits("QTabBar")) {
            ((QTabBar *)child)->setFont(KisUiFont::dockFont());
        }
    }
}

void KisMainWindow::slotUpdateWidgetStyle()
{
     KisConfig cfg(true);
     QString themeFromConfig = cfg.widgetStyle();

     Q_FOREACH (auto key, d->actionMap.keys()) {
         if(d->actionMap.value(key)->isChecked()) {
            cfg.setWidgetStyle(key);
            qApp->setProperty(currentUnderlyingStyleNameProperty, key);
            qApp->setStyle(key);

#ifndef Q_OS_HAIKU
            QString currentStyle = qApp->style()->objectName();
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
            if (currentStyle == "macintosh" || currentStyle == "windowsvista") {
                d->themeManager->setCurrentTheme("System");
            }
#else
            if (currentStyle == "macos" || currentStyle == "windowsvista" || currentStyle == "windows11") {
                d->themeManager->setCurrentTheme("System");
            }
#endif
#endif
         }
     }
}

QList<QDockWidget*> KisMainWindow::dockWidgets() const
{
    return d->dockWidgetsMap.values();
}

QDockWidget* KisMainWindow::dockWidget(const QString &id)
{
    if (!d->dockWidgetsMap.contains(id)) return 0;
    return d->dockWidgetsMap[id];
}

QList<KoCanvasObserverBase*> KisMainWindow::canvasObservers() const
{
    QList<KoCanvasObserverBase*> observers;

    observers.append(static_cast<KoCanvasObserverBase*>(KisPart::instance()->playbackEngine()));

    Q_FOREACH (QDockWidget *docker, dockWidgets()) {
        KoCanvasObserverBase *observer = dynamic_cast<KoCanvasObserverBase*>(docker);
        if (observer) {
            observers << observer;
        }
        else {
            warnKrita << docker << "is not a canvas observer";
        }
    }

    return observers;
}


void KisMainWindow::toggleDockersVisibility(bool visible, bool onWelcomePage)
{
    if (!visible) {
        d->dockerStateBeforeHiding = saveState();

        Q_FOREACH (QObject* widget, children()) {
            if (widget->inherits("QDockWidget")) {
                QDockWidget* dw = static_cast<QDockWidget*>(widget);
                if (dw->isVisible() && !(onWelcomePage && dw->property("ShowOnWelcomePage").toBool())) {
                    dw->hide();
                }
            }
        }
    }
    else {
        restoreState(d->dockerStateBeforeHiding);
    }
    QTimer::singleShot(0, this, SLOT(slotUpdateBrocreateOverlayGeometry()));
}

void KisMainWindow::subWindowActivated()
{
    bool enabled = (activeKisView() != 0);

    d->mdiCascade->setEnabled(enabled);
    d->mdiNextWindow->setEnabled(enabled);
    d->mdiPreviousWindow->setEnabled(enabled);
    d->mdiTile->setEnabled(enabled);
    d->close->setEnabled(enabled);
    d->closeAll->setEnabled(enabled);

    setActiveSubWindow(d->mdiArea->activeSubWindow());

    QMdiSubWindow *subWindow = d->mdiArea->currentSubWindow();
    if (subWindow) {
        QMenu *menu = subWindow->systemMenu();
        if (menu && menu->actions().size() == 8) {
            Q_FOREACH (QAction *action, menu->actions()) {
                action->setShortcut(QKeySequence());

            }
            menu->actions().last()->deleteLater();
        }
    }

    d->actionManager()->updateGUI();
}

void KisMainWindow::windowFocused()
{
    if (viewManager() && viewManager()->selectionManager()) {
        viewManager()->selectionManager()->selectionChanged();
    }

    KisPart *kisPart = KisPart::instance();
    KisWindowLayoutManager *layoutManager = KisWindowLayoutManager::instance();
    if (!layoutManager->primaryWorkspaceFollowsFocus()) return;

    QUuid primary = layoutManager->primaryWindowId();
    if (primary.isNull()) return;

    if (d->id == primary) {
        if (!d->workspaceBorrowedBy.isNull()) {
            KisMainWindow *borrower = kisPart->windowById(d->workspaceBorrowedBy);
            if (!borrower) return;
            swapWorkspaces(this, borrower);
        }
    } else {
        if (d->workspaceBorrowedBy == primary) return;

        KisMainWindow *primaryWindow = kisPart->windowById(primary);
        if (!primaryWindow) return;
        swapWorkspaces(this, primaryWindow);
    }
}


void KisMainWindow::updateWindowMenu()
{
    QMenu *menu = d->windowMenu->menu();
    menu->clear();

#ifndef Q_OS_ANDROID
    menu->addAction(d->newWindow);
#endif
    menu->addAction(d->documentMenu);

    QMenu *docMenu = d->documentMenu->menu();
    docMenu->clear();

    QFontMetrics fontMetrics = docMenu->fontMetrics();
    QRect geom = this->geometry();
    QPoint p(geom.width() / 2 + geom.left(), geom.height() / 2 + geom.top());
    QScreen *screen = qApp->screenAt(p);
    int fileStringWidth = 300;
    if (screen) {
        fileStringWidth = int(screen->availableGeometry().width() * .40f);
    }
    Q_FOREACH (QPointer<KisDocument> doc, KisPart::instance()->documents()) {
        if (doc) {
            QString title = fontMetrics.elidedText(doc->path(), Qt::ElideMiddle, fileStringWidth);
            if (title.isEmpty() && doc->image()) {
                title = doc->image()->objectName();
            }
            QAction *action = docMenu->addAction(title);
            action->setIcon(qApp->windowIcon());
            connect(action, SIGNAL(triggered()), d->documentMapper, SLOT(map()));
            d->documentMapper->setMapping(action, doc);
        }
    }

    menu->addAction(d->workspaceMenu);
    QMenu *workspaceMenu = d->workspaceMenu->menu();
    workspaceMenu->clear();
    KisResourceModel resourceModel(ResourceType::Workspaces);
    KisResourceIterator resourceIterator(&resourceModel);
    KisMainWindow *m_this = this;

    while (resourceIterator.hasNext()) {
        KisResourceItemSP resource = resourceIterator.next();
        QAction *action = workspaceMenu->addAction(resource->name());
        action->setProperty("md5", QVariant::fromValue<QString>(resource->md5sum()));
        connect(action, SIGNAL(triggered()), this, SLOT(restoreWorkspace()));
    }
    workspaceMenu->addSeparator();
    connect(workspaceMenu->addAction(i18nc("@action:inmenu", "&Import Workspace...")),
            &QAction::triggered,
            this,
            [&]()
    {
        QStringList mimeTypes = KisResourceLoaderRegistry::instance()->mimeTypes(ResourceType::Workspaces);

        KoFileDialog dialog(0, KoFileDialog::OpenFile, "OpenDocument");
        dialog.setMimeTypeFilters(mimeTypes);
        dialog.setCaption(i18nc("@title:window", "Choose File to Add"));
        QString filename = dialog.filename();

        KisResourceUserOperations::importResourceFileWithUserInput(this, "", ResourceType::Workspaces, filename);
    });

    connect(workspaceMenu->addAction(i18nc("@action:inmenu", "&New Workspace...")),
            &QAction::triggered,
            this,
            [=]() {
        QString name;
        name = QInputDialog::getText(this, i18nc("@title:window", "New Workspace..."),
                                                                i18nc("@label:textbox", "Name:"));
        if (name.isEmpty()) {
            return;
        }

        KisWorkspaceResourceSP workspace(new KisWorkspaceResource(""));
        workspace->setDockerState(m_this->saveState());
        workspace->setImage(layoutThumbnail());
        workspace->setValid(true);

        d->viewManager->canvasResourceProvider()->notifySavingWorkspace(workspace);
        workspace->setValid(true);

        workspace->setFilename(name.replace(" ", "_") + workspace->defaultFileExtension());
        workspace->setName(name);

        KisResourceUserOperations::addResourceWithUserInput(this, workspace);
    });

    menu->addSeparator();
    menu->addAction(d->close);
    menu->addAction(d->closeAll);
    if (d->mdiArea->viewMode() == QMdiArea::SubWindowView) {
        menu->addSeparator();
        menu->addAction(d->mdiTile);
        menu->addAction(d->mdiCascade);
    }
    menu->addSeparator();
    menu->addAction(d->mdiNextWindow);
    menu->addAction(d->mdiPreviousWindow);
    menu->addSeparator();

    QList<QMdiSubWindow *> windows = d->mdiArea->subWindowList();
    for (int i = 0; i < windows.size(); ++i) {
        QPointer<KisView>child = qobject_cast<KisView*>(windows.at(i)->widget());
        if (child && child->document()) {
            QString text;
            if (i < 9) {
                text = i18n("&%1 %2", i + 1, fontMetrics.elidedText(child->document()->path(), Qt::ElideMiddle, fileStringWidth));
            }
            else {
                text = i18n("%1 %2", i + 1, fontMetrics.elidedText(child->document()->path(), Qt::ElideMiddle, fileStringWidth));
            }

            QAction *action  = menu->addAction(text);
            action->setIcon(qApp->windowIcon());
            action->setCheckable(true);
            action->setChecked(child == activeKisView());
            connect(action, SIGNAL(triggered()), d->windowMapper, SLOT(map()));
            d->windowMapper->setMapping(action, windows.at(i));
        }
    }

    bool showMdiArea = windows.count( ) > 0;
    if (!showMdiArea) {
        showWelcomeScreen(true);
    }

    Q_FOREACH (QObject* widget, children()) {
        if (widget->inherits("QDockWidget")) {
            QDockWidget* dw = static_cast<QDockWidget*>(widget);

            if ( dw->objectName() == "ToolBox") {
                dw->setEnabled(showMdiArea);
            }
        }
    }
}

void KisMainWindow::updateSubwindowFlags()
{
    bool onlyOne = false;
    if (d->mdiArea->subWindowList().size() == 1 && d->mdiArea->viewMode() == QMdiArea::SubWindowView) {
        onlyOne = true;
    }
    Q_FOREACH (QMdiSubWindow *subwin, d->mdiArea->subWindowList()) {
        if (onlyOne) {
            subwin->setWindowFlags(subwin->windowFlags() | Qt::FramelessWindowHint);
            subwin->showMaximized();
        } else {
            subwin->setWindowFlags((subwin->windowFlags() | Qt::FramelessWindowHint) ^ Qt::FramelessWindowHint);
        }
    }
}

void KisMainWindow::setActiveSubWindow(QWidget *window)
{
    if (!window) {
        unsetActiveView();
        return;
    }
    QMdiSubWindow *subwin = qobject_cast<QMdiSubWindow *>(window);

    if (subwin && subwin != d->activeSubWindow) {
        KisView *view = qobject_cast<KisView *>(subwin->widget());
        if (view && view != activeView()) {
            setActiveView(view);
        }
        d->activeSubWindow = subwin;
    }
    updateWindowMenu();
    d->actionManager()->updateGUI();
}

void KisMainWindow::configChanged()
{
    KisConfig cfg(true);
    QMdiArea::ViewMode viewMode = (QMdiArea::ViewMode)cfg.readEntry<int>("mdi_viewmode", (int)QMdiArea::TabbedView);
    d->mdiArea->setViewMode(viewMode);
    customizeTabBar();

    Q_FOREACH (QMdiSubWindow *subwin, d->mdiArea->subWindowList()) {
        subwin->setOption(QMdiSubWindow::RubberBandMove, cfg.readEntry<int>("mdi_rubberband", cfg.useOpenGL()));
        subwin->setOption(QMdiSubWindow::RubberBandResize, cfg.readEntry<int>("mdi_rubberband", cfg.useOpenGL()));
        if (viewMode == QMdiArea::TabbedView) {
            subwin->setWindowState(Qt::WindowMaximized);
        }

        if (viewMode == QMdiArea::TabbedView) {
            Qt::WindowFlags oldFlags = subwin->windowFlags();
            Qt::WindowFlags flags = oldFlags;

            flags &= ~Qt::WindowStaysOnTopHint;
            flags &= ~Qt::WindowStaysOnBottomHint;

            if (flags != oldFlags) {
                subwin->setWindowFlags(flags);
                subwin->showMaximized();
            }
        }
    }
#ifdef Q_OS_MACOS
    updateSubwindowFlags();
#endif

    KConfigGroup group( KSharedConfig::openConfig(), "theme");
#ifndef Q_OS_HAIKU
    d->themeManager->setCurrentTheme(group.readEntry("Theme", "Krita dark"));
#endif
    d->actionManager()->updateGUI();

    // Enforce Brocreate canvas #0B0B0D backdrop
    d->mdiArea->setBackground(QBrush(QColor("#0B0B0D")));
    d->mdiArea->update();

    qApp->setFont(KisUiFont::normalFont());

    Q_FOREACH (QObject* widget, children()) {
        if (widget->inherits("QDockWidget")) {
            QDockWidget* dw = static_cast<QDockWidget*>(widget);
            dw->setFont(KisUiFont::dockFont());
        }
    }
}

KisView* KisMainWindow::newView(QObject *document, QMdiSubWindow *subWindow)
{
    KisDocument *doc = qobject_cast<KisDocument*>(document);
    KisView *view = addViewAndNotifyLoadingCompleted(doc, subWindow);
    d->actionManager()->updateGUI();

    return view;
}

void KisMainWindow::newWindow()
{
#ifdef Q_OS_ANDROID
    if (KisPart::instance()->currentMainwindow()) {
        QMessageBox::warning(this, i18nc("@title:window", "Brocreate"),
                             "Creating a New Main Window is unsupported on Android");
        return;
    }
#endif
    KisMainWindow *mainWindow = KisPart::instance()->createMainWindow();
    mainWindow->initializeGeometry();
    mainWindow->show();
}

void KisMainWindow::closeCurrentWindow()
{
    if (d->mdiArea->currentSubWindow()) {
        d->mdiArea->currentSubWindow()->close();
        d->actionManager()->updateGUI();
    }
}

void KisMainWindow::checkSanity()
{
    if (!KoColorSpaceEngineRegistry::instance()->contains("icc")) {
        m_errorMessage = i18n("The Brocreate LittleCMS color management plugin is not installed. Brocreate will quit now.");
        m_dieOnError = true;
        QTimer::singleShot(0, this, SLOT(showErrorAndDie()));
        return;
    }

    slotStoragesWarning();

    Q_EMIT KisPart::instance()->sigMainWindowCreated();
}

void KisMainWindow::showErrorAndDie()
{
    QMessageBox::critical(qApp->activeWindow(), i18nc("@title:window", "Installation error"), m_errorMessage);
    if (m_dieOnError) {
        exit(10);
    }
}

void KisMainWindow::showAboutApplication()
{
    KisAboutApplication dlg(this);
    dlg.setWindowTitle(i18n("About Brocreate"));
    dlg.exec();
}

QPointer<KisView> KisMainWindow::activeKisView()
{
    if (!d->mdiArea) return 0;
    QMdiSubWindow *activeSubWindow = d->mdiArea->activeSubWindow();
    if (!activeSubWindow) return 0;
    return qobject_cast<KisView*>(activeSubWindow->widget());
}

void KisMainWindow::newOptionWidgets(KoCanvasController *controller, const QList<QPointer<QWidget> > &optionWidgetList)
{
    KIS_ASSERT_RECOVER_NOOP(controller == d->viewManager->toolManager()->activeCanvasController());
    bool isOurOwnView = false;

    Q_FOREACH (QPointer<KisView> view, KisPart::instance()->views()) {
        if (view && view->canvasController() == controller) {
            isOurOwnView = view->mainWindow() == this;
        }
    }

    if (!isOurOwnView) return;

    Q_FOREACH (QWidget *w, optionWidgetList) {
#ifdef Q_OS_MACOS
        w->setAttribute(Qt::WA_MacSmallSize, true);
#endif
        w->setFont(KisUiFont::dockFont());
    }

    if (d->toolOptionsDocker) {
        d->toolOptionsDocker->setOptionWidgets(optionWidgetList);
    }
    else {
        d->viewManager->paintOpBox()->newOptionWidgets(optionWidgetList);
    }
}

void KisMainWindow::createActions()
{
    KisActionManager *actionManager = d->actionManager();

    actionManager->createStandardAction(KStandardAction::New, this, SLOT(slotFileNew()));
    actionManager->createStandardAction(KStandardAction::Open, this, SLOT(slotFileOpen()));
    actionManager->createStandardAction(KStandardAction::Quit, this, SLOT(slotFileQuit()));
    actionManager->createStandardAction(KStandardAction::ConfigureToolbars, this, SLOT(slotConfigureToolbars()));
    d->fullScreenMode = actionManager->createStandardAction(KStandardAction::FullScreen, this, SLOT(viewFullscreen(bool)));

    d->recentFiles = KStandardAction::openRecent(this, SLOT(slotFileOpenRecent(QUrl)), actionCollection());

    d->saveAction = actionManager->createStandardAction(KStandardAction::Save, this, SLOT(slotFileSave()));
    d->saveAction->setActivationFlags(KisAction::IMAGE_IS_WRITABLE);

    d->saveActionAs = actionManager->createStandardAction(KStandardAction::SaveAs, this, SLOT(slotFileSaveAs()));
    d->saveActionAs->setActivationFlags(KisAction::ACTIVE_IMAGE);

    d->undo = actionManager->createStandardAction(KStandardAction::Undo, this, SLOT(undo()));
    d->undo->setActivationFlags(KisAction::ACTIVE_IMAGE);

    d->redo = actionManager->createStandardAction(KStandardAction::Redo, this, SLOT(redo()));
    d->redo->setActivationFlags(KisAction::ACTIVE_IMAGE);

    d->undoActionsUpdateManager.reset(new KisUndoActionsUpdateManager(d->undo, d->redo));
    d->undoActionsUpdateManager->setCurrentDocument(d->activeView ? d->activeView->document() : 0);

    d->importAnimation  = actionManager->createAction("file_import_animation");
    connect(d->importAnimation, SIGNAL(triggered()), this, SLOT(importAnimation()));

#ifndef Q_OS_ANDROID
    d->importVideoAnimation = actionManager->createAction("file_import_video_animation");
    connect(d->importVideoAnimation, SIGNAL(triggered()), this, SLOT(importVideoAnimation()));
#endif

    d->renderAnimation = actionManager->createAction("render_animation");
    d->renderAnimation->setActivationFlags(KisAction::IMAGE_HAS_ANIMATION);
    connect( d->renderAnimation, SIGNAL(triggered()), this, SLOT(renderAnimation()));

    d->renderAnimationAgain = actionManager->createAction("render_animation_again");
    d->renderAnimationAgain->setActivationFlags(KisAction::IMAGE_HAS_ANIMATION);
    connect( d->renderAnimationAgain, SIGNAL(triggered()), this, SLOT(renderAnimationAgain()));

    d->closeAll = actionManager->createAction("file_close_all");
    connect(d->closeAll, SIGNAL(triggered()), this, SLOT(slotFileCloseAll()));

    d->importFile  = actionManager->createAction("file_import_file");
    d->importFile->setActivationFlags(KisAction::IMAGE_IS_WRITABLE);
    connect(d->importFile, SIGNAL(triggered(bool)), this, SLOT(slotImportFile()));

    d->exportFile  = actionManager->createAction("file_export_file");
    connect(d->exportFile, SIGNAL(triggered(bool)), this, SLOT(slotExportFile()));

    d->exportFileAdvance  = actionManager->createAction("file_export_advanced");
    connect(d->exportFileAdvance, SIGNAL(triggered(bool)), this, SLOT(slotExportAdvance()));

    d->showDocumentInfo  = actionManager->createAction("file_documentinfo");
    connect(d->showDocumentInfo, SIGNAL(triggered(bool)), this, SLOT(slotDocumentInfo()));
#ifndef Q_OS_HAIKU
    d->themeManager->setThemeMenuAction(new KActionMenu(i18nc("@action:inmenu", "&Themes"), this));
    d->themeManager->registerThemeActions(actionCollection());
    connect(d->themeManager, SIGNAL(signalThemeChanged()), this, SLOT(slotThemeChanged()), Qt::QueuedConnection);
    connect(this, SIGNAL(themeChanged()), d->welcomePage, SLOT(slotUpdateThemeColors()), Qt::UniqueConnection);
#endif
    d->toggleDockers = actionManager->createAction("view_toggledockers");

    KisConfig(true).showDockers(true);
    d->toggleDockers->setChecked(true);
    connect(d->toggleDockers, SIGNAL(toggled(bool)), SLOT(toggleDockersVisibility(bool)));

    d->resetConfigurations  = actionManager->createAction("reset_configurations");
    connect(d->resetConfigurations, SIGNAL(triggered()), this, SLOT(slotResetConfigurations()));

#ifndef Q_OS_ANDROID
    d->toggleDetachCanvas = actionManager->createAction("view_detached_canvas");
    d->toggleDetachCanvas->setChecked(false);
    connect(d->toggleDetachCanvas, SIGNAL(toggled(bool)), SLOT(setCanvasDetached(bool)));
#endif
    setCanvasDetached(false);

    d->toggleDockerTitleBars = actionManager->createAction("view_toggledockertitlebars");
    d->toggleDockerTitleBars->setChecked(false);
    connect(d->toggleDockerTitleBars, SIGNAL(toggled(bool)), SLOT(showDockerTitleBars(bool)));

    actionCollection()->addAction("settings_dockers_menu", d->dockWidgetMenu);
    actionCollection()->addAction("window", d->windowMenu);

    actionCollection()->addAction("style_menu", d->styleMenu);

    d->mdiCascade = actionManager->createAction("windows_cascade");
    connect(d->mdiCascade, SIGNAL(triggered()), d->mdiArea, SLOT(cascadeSubWindows()));

    d->mdiTile = actionManager->createAction("windows_tile");
    connect(d->mdiTile, SIGNAL(triggered()), d->mdiArea, SLOT(tileSubWindows()));

    d->mdiNextWindow = actionManager->createAction("windows_next");
    connect(d->mdiNextWindow, SIGNAL(triggered()), d->mdiArea, SLOT(activateNextSubWindow()));

    d->mdiPreviousWindow = actionManager->createAction("windows_previous");
    connect(d->mdiPreviousWindow, SIGNAL(triggered()), d->mdiArea, SLOT(activatePreviousSubWindow()));

#ifdef Q_OS_ANDROID
    d->showDonationManagementDialog = actionManager->createAction("manage_donations");
    connect(d->showDonationManagementDialog,
            &QAction::triggered,
            this,
            &KisMainWindow::slotShowDonationManagementDialog);

    KisAndroidDonations *androidDonations = KisAndroidDonations::instance();
    if (androidDonations) {
        d->manageSubscriptions = actionManager->createAction("manage_subscriptions");
        connect(d->manageSubscriptions,
                &QAction::triggered,
                androidDonations,
                &KisAndroidDonations::slotManageSubscriptions);
        connect(androidDonations,
                &KisAndroidDonations::sigShowDonationManagementDialogRequested,
                this,
                &KisMainWindow::slotShowDonationManagementDialog,
                Qt::QueuedConnection);
    }
#if KRITA_QT_HAS_ANDROID_QPLATFORMSCREEN_DENSITY_ADJUSTMENT
    KisAndroidScaling *androidScaling = KisAndroidScaling::instance();
    if (androidScaling && androidScaling->isSupported()) {
        d->changeInterfaceScale = actionManager->createAction("change_interface_scale");
        connect(d->changeInterfaceScale, &QAction::triggered, this, &KisMainWindow::slotChangeInterfaceScale);
    }
#endif
#else
    d->newWindow = actionManager->createAction("view_newwindow");
    connect(d->newWindow, SIGNAL(triggered(bool)), this, SLOT(newWindow()));
#endif

    d->close = actionManager->createStandardAction(KStandardAction::Close, this, SLOT(closeCurrentWindow()));

    d->showSessionManager = actionManager->createAction("file_sessions");
    connect(d->showSessionManager, SIGNAL(triggered(bool)), this, SLOT(slotShowSessionManager()));

    d->commandBarAction = actionManager->createAction("command_bar_open");
    connect(d->commandBarAction, SIGNAL(triggered(bool)), this, SLOT(openCommandBar()));

    actionManager->createStandardAction(KStandardAction::Preferences, this, SLOT(slotPreferences()));

    for (int i = 0; i < 2; i++) {
        d->expandingSpacers[i] = new KisAction(i18n("Expanding Spacer"));
        d->expandingSpacers[i]->setDefaultWidget(new QWidget(this));
        d->expandingSpacers[i]->defaultWidget()->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        actionManager->addAction(QString("expanding_spacer_%1").arg(i), d->expandingSpacers[i]);
    }

    {
        using namespace KisWidgetConnectionUtils;

        QAction *action = actionManager->createAction("lock_toolbars");
        connectControl(action, KisToolBar::toolBarStateModel(), "toolBarsLocked");
    }
}

void KisMainWindow::applyToolBarLayout()
{
    Q_FOREACH (KisToolBar *toolBar, toolBars()) {
        toolBar->layout()->setSpacing(4);
        toolBar->setStyleSheet("QToolBar { border: none }");
        toolBar->hide();

        Q_FOREACH (QAction *ac, toolBar->actions()){
            if (ac->icon().pixmap(QSize(1,1)).isNull() == false){
                ac->setPriority(QAction::LowPriority);
            } else {
                ac->setIcon(QIcon());
            }
        }
    }
}

void KisMainWindow::initializeGeometry()
{
    KConfigGroup cfg = d->windowStateConfig;
    QByteArray geom = QByteArray::fromBase64(cfg.readEntry("ko_geometry", QByteArray()));
    if (!restoreGeometry(geom)) {
        QRect desk = this->screen()->availableGeometry();

        quint32 x = desk.x();
        quint32 y = desk.y();
        quint32 w = 0;
        quint32 h = 0;

        const int deskWidth = desk.width();
        if (deskWidth > 1024) {
            w = (deskWidth / 3) * 2;
            h = (desk.height() / 3) * 2;
        }
        else {
            w = desk.width();
            h = desk.height();
        }

        x += (desk.width() - w) / 2;
        y += (desk.height() - h) / 2;

        move(x,y);
        setGeometry(geometry().x(), geometry().y(), w, h);
    }
#ifdef Q_OS_ANDROID
    bool fullScreen = KisAndroidUtils::isInFullScreen();
#else
    bool fullScreen = isFullScreen();
#endif
    d->fullScreenMode->setChecked(fullScreen);
}

void KisMainWindow::showManual()
{
    QDesktopServices::openUrl(QUrl("https://docs.krita.org"));
}

void KisMainWindow::showDockerTitleBars(bool show)
{
    Q_FOREACH (QDockWidget *dock, dockWidgets()) {
        if (dock->titleBarWidget() && !dock->titleBarWidget()->inherits("KisUtilityTitleBar")) {
            dock->titleBarWidget()->setVisible(show || dock->isFloating());
        }
    }

    KisConfig cfg(true);
    cfg.setShowDockerTitleBars(show);
}

void KisMainWindow::slotXmlGuiMakingChanges(bool finished)
{
    if (finished) {
        subWindowActivated();
    }
}

void KisMainWindow::orientationChanged()
{
    QScreen *screen = QGuiApplication::primaryScreen();

    for (QWindow* window: QGuiApplication::topLevelWindows()) {
        if ((window->type() == Qt::Popup)
            && (window->flags() & Qt::FramelessWindowHint) == 0
            && (window->geometry().topLeft() != QPoint(0, 0))) {
            int screenHeight = screen->geometry().width();
            int screenWidth = screen->geometry().height();

            int new_x = (window->position().x() * screenWidth) / screenHeight;
            int new_y = (window->position().y() * screenHeight) / screenWidth;

            int winWidth = window->geometry().width();
            int winHeight = window->geometry().height();

            if (new_x > screenWidth - winWidth) {
                new_x = screenWidth - winWidth;
                if (new_x < 0)
                    new_x = 0;
            }
            if (new_y > screenHeight - winHeight) {
                new_y = screenHeight - winHeight;
                if (new_y < 0)
                    new_y = 0;
            }

            window->setPosition(QPoint(new_x, new_y));
        }
    }
}

bool KisMainWindow::checkActiveBundlesAvailable()
{
    KisStorageFilterProxyModel proxy;
    proxy.setSourceModel(KisStorageModel::instance());
    proxy.setFilter(KisStorageFilterProxyModel::ByStorageType,
                    QStringList()
                    << KisResourceStorage::storageTypeToUntranslatedString(KisResourceStorage::StorageType::Bundle));

    return (proxy.rowCount() > 0);
}

bool KisMainWindow::checkPaintOpAvailable()
{
    KisPaintOpPresetResourceServer * rserver = KisResourceServerProvider::instance()->paintOpPresetServer();
    return (rserver->resourceCount() > 0);
}

void KisMainWindow::applyActionIconOverridesFromLocalXML()
{
    QString xmlPath = KoResourcePaths::locateLocal("data", "krita5.xmlgui");
    QFile file(xmlPath);
    if (!file.exists()) {
        return;
    }

    QDomDocument doc;
    if (!file.open(QIODevice::ReadOnly) || !doc.setContent(&file)) {
        return;
    }

    QDomNodeList overridesList = doc.elementsByTagName(QStringLiteral("ActionIconOverrides"));
    if (!overridesList.isEmpty()) {
        QDomElement overridesElement = overridesList.at(0).toElement();
        QDomNodeList actions = overridesElement.elementsByTagName(QStringLiteral("Action"));
        for (int i = 0; i < actions.count(); ++i) {
            QDomElement actionElement = actions.at(i).toElement();
            QString name = actionElement.attribute(QStringLiteral("name"));
            QString icon = actionElement.attribute(QStringLiteral("icon"));
            if (!name.isEmpty() && !icon.isEmpty()) {
                QAction *qa = actionCollection()->action(name);
                if (qa) {
                    qa->setIcon(KisIconUtils::loadIcon(icon));
                }
            }
        }
    }
}

void KisMainWindow::synchronizeDynamicActions()
{
    KisActionManager::synchronizeDynamicActions(this->actionCollection()->actions(), this->menuBar()->actions());
}

// ============================================================================
// BROCREATE TABLET UI, QSS DARK THEME, FLOATING ISLANDS & PROCREATE GESTURES
// ============================================================================

void KisMainWindow::setupBrocreateUI()
{
    if (d->brocreateUiInitialized) {
        return;
    }
    d->brocreateUiInitialized = true;

    setAttribute(Qt::WA_AcceptTouchEvents, true);
    d->widgetStack->installEventFilter(this);
    d->mdiArea->setAttribute(Qt::WA_AcceptTouchEvents, true);
    d->mdiArea->installEventFilter(this);

    if (menuBar()) {
        menuBar()->setVisible(false);
    }
    if (statusBar()) {
        statusBar()->hide();
    }
    showDockerTitleBars(false);

    // Hide desktop toolbars by default for clean Brocreate tablet workspace
    Q_FOREACH (QToolBar *tb, findChildren<QToolBar *>()) {
        tb->hide();
    }

    // Long-press eyedropper timer (1-finger hold >= 450ms)
    d->longPressEyedropperTimer = new QTimer(this);
    d->longPressEyedropperTimer->setSingleShot(true);
    d->longPressEyedropperTimer->setInterval(450);
    connect(d->longPressEyedropperTimer, SIGNAL(timeout()), this, SLOT(slotBrocreateLongPressEyedropper()));

    auto makeIslandBtn = [this](QWidget *parent, BrocreateGlyph glyph, const QString &tooltip, const QString &actionName, bool checkable = false) -> QToolButton* {
        QToolButton *btn = new QToolButton(parent);
        btn->setIcon(makeBrocreateIcon(glyph));
        btn->setIconSize(QSize(20, 20));
        btn->setFixedSize(34, 34);
        btn->setCheckable(checkable);
        btn->setToolTip(tooltip);
        btn->setProperty("brocreateAction", actionName);
        if (!actionName.isEmpty()) {
            connect(btn, SIGNAL(clicked()), this, SLOT(slotBrocreateTriggerActionByName()));
        }
        return btn;
    };

    // ------------------------------------------------------------------------
    // 1. TOP-LEFT FLOATING ISLAND
    // ------------------------------------------------------------------------
    d->topLeftIsland = new QFrame(d->widgetStack);
    d->topLeftIsland->setObjectName(QStringLiteral("brocreateTopLeftIsland"));
    QHBoxLayout *tlLayout = new QHBoxLayout(d->topLeftIsland);
    tlLayout->setContentsMargins(8, 5, 8, 5);
    tlLayout->setSpacing(4);

    tlLayout->addWidget(makeIslandBtn(d->topLeftIsland, BrocreateGlyph::NewDoc, i18n("New Canvas"), QStringLiteral("file_new")));
    tlLayout->addWidget(makeIslandBtn(d->topLeftIsland, BrocreateGlyph::OpenDoc, i18n("Open"), QStringLiteral("file_open")));
    tlLayout->addWidget(makeIslandBtn(d->topLeftIsland, BrocreateGlyph::SaveDoc, i18n("Save"), QStringLiteral("file_save")));
    tlLayout->addWidget(makeIslandBtn(d->topLeftIsland, BrocreateGlyph::Settings, i18n("Preferences"), QStringLiteral("options_configure")));

    QFrame *tlDiv1 = new QFrame(d->topLeftIsland);
    tlDiv1->setObjectName(QStringLiteral("brocreateDividerV"));
    tlDiv1->setFixedSize(1, 22);
    tlLayout->addWidget(tlDiv1);

    d->colorSwatchButton = new QPushButton(d->topLeftIsland);
    d->colorSwatchButton->setObjectName(QStringLiteral("brocreateColorSwatch"));
    d->colorSwatchButton->setFixedSize(24, 24);
    d->colorSwatchButton->setToolTip(i18n("Active Color"));
    connect(d->colorSwatchButton, &QPushButton::clicked, this, [this]() {
        if (QDockWidget *colorDock = dockWidget(QStringLiteral("ColorSelectorNg"))) {
            colorDock->setVisible(!colorDock->isVisible());
            if (colorDock->isVisible()) {
                colorDock->raise();
            }
        }
    });
    tlLayout->addWidget(d->colorSwatchButton);

    d->blendModeCombo = new QComboBox(d->topLeftIsland);
    d->blendModeCombo->setObjectName(QStringLiteral("brocreateBlendCombo"));
    d->blendModeCombo->setFixedHeight(30);
    d->blendModeCombo->setMinimumWidth(98);
    const QList<QPair<QString, QString>> blendModes = {
        {QStringLiteral("Normal"), QStringLiteral("normal")},
        {QStringLiteral("Multiply"), QStringLiteral("multiply")},
        {QStringLiteral("Screen"), QStringLiteral("screen")},
        {QStringLiteral("Overlay"), QStringLiteral("overlay")},
        {QStringLiteral("Darken"), QStringLiteral("darken")},
        {QStringLiteral("Lighten"), QStringLiteral("lighten")},
        {QStringLiteral("Color Dodge"), QStringLiteral("dodge")},
        {QStringLiteral("Color Burn"), QStringLiteral("burn")},
        {QStringLiteral("Soft Light"), QStringLiteral("soft_light_svg")},
        {QStringLiteral("Hard Light"), QStringLiteral("hard_light")},
        {QStringLiteral("Add"), QStringLiteral("add")},
        {QStringLiteral("Difference"), QStringLiteral("diff")}
    };
    for (const auto &bm : blendModes) {
        d->blendModeCombo->addItem(bm.first, bm.second);
    }
    connect(d->blendModeCombo, SIGNAL(currentIndexChanged(int)), this, SLOT(slotBrocreateBlendModeChanged(int)));
    tlLayout->addWidget(d->blendModeCombo);

    QFrame *tlDiv2 = new QFrame(d->topLeftIsland);
    tlDiv2->setObjectName(QStringLiteral("brocreateDividerV"));
    tlDiv2->setFixedSize(1, 22);
    tlLayout->addWidget(tlDiv2);

    tlLayout->addWidget(makeIslandBtn(d->topLeftIsland, BrocreateGlyph::Undo, i18n("Undo"), QStringLiteral("edit_undo")));
    tlLayout->addWidget(makeIslandBtn(d->topLeftIsland, BrocreateGlyph::Redo, i18n("Redo"), QStringLiteral("edit_redo")));

    d->animationModeBtn = makeIslandBtn(d->topLeftIsland, BrocreateGlyph::Animation, i18n("Animation & Onion Skin"), QString(), true);
    connect(d->animationModeBtn, SIGNAL(clicked()), this, SLOT(slotBrocreateToggleAnimationLayout()));
    tlLayout->addWidget(d->animationModeBtn);

    d->studioToggleBtn = makeIslandBtn(d->topLeftIsland, BrocreateGlyph::StudioDock, i18n("Toggle Studio Dock"), QString(), true);
    d->studioToggleBtn->setChecked(true);
    connect(d->studioToggleBtn, SIGNAL(clicked()), this, SLOT(slotBrocreateToggleStudioDock()));
    tlLayout->addWidget(d->studioToggleBtn);

    d->topLeftIsland->adjustSize();

    // ------------------------------------------------------------------------
    // 2. TOP-CENTER DYNAMIC ISLAND
    // ------------------------------------------------------------------------
    d->topCenterIsland = new QFrame(d->widgetStack);
    d->topCenterIsland->setObjectName(QStringLiteral("brocreateTopCenterIsland"));
    QHBoxLayout *tcLayout = new QHBoxLayout(d->topCenterIsland);
    tcLayout->setContentsMargins(10, 5, 10, 5);
    tcLayout->setSpacing(6);

    d->brushModeBtn = makeIslandBtn(d->topCenterIsland, BrocreateGlyph::Brush, i18n("Brush"), QStringLiteral("KritaShape/KisToolBrush"), true);
    d->brushModeBtn->setChecked(true);
    d->eraserModeBtn = makeIslandBtn(d->topCenterIsland, BrocreateGlyph::Eraser, i18n("Eraser"), QStringLiteral("erase_action"), true);
    d->selectModeBtn = makeIslandBtn(d->topCenterIsland, BrocreateGlyph::Selection, i18n("Selection"), QStringLiteral("KisToolSelectOutline"), true);

    tcLayout->addWidget(d->brushModeBtn);
    tcLayout->addWidget(d->eraserModeBtn);
    tcLayout->addWidget(d->selectModeBtn);

    d->opacityPill = new BrocreatePillSlider(BrocreatePillSlider::OpacityBluePill, d->topCenterIsland);
    d->opacityPill->setNormalizedValue(0.35);
    d->opacityPill->setDisplayText(QStringLiteral("35%"));
    d->opacityPill->onValueChanged = [this](qreal val) { slotBrocreateOpacityChanged(val); };
    tcLayout->addWidget(d->opacityPill);

    d->brushSizePill = new BrocreatePillSlider(BrocreatePillSlider::SizeDarkPill, d->topCenterIsland);
    d->brushSizePill->setNormalizedValue(0.26);
    d->brushSizePill->setDisplayText(QStringLiteral("5.00 px"));
    d->brushSizePill->onValueChanged = [this](qreal val) { slotBrocreateBrushSizeChanged(val); };
    tcLayout->addWidget(d->brushSizePill);

    d->mirrorBtn = makeIslandBtn(d->topCenterIsland, BrocreateGlyph::Mirror, i18n("Mirror Canvas"), QStringLiteral("mirror_canvas"), true);
    d->fullscreenBtn = makeIslandBtn(d->topCenterIsland, BrocreateGlyph::Fullscreen, i18n("Canvas Only / Fullscreen"), QString(), true);
    connect(d->fullscreenBtn, SIGNAL(clicked()), this, SLOT(slotBrocreateToggleFullscreenCanvasOnly()));

    tcLayout->addWidget(d->mirrorBtn);
    tcLayout->addWidget(d->fullscreenBtn);

    d->topCenterIsland->adjustSize();

    // ------------------------------------------------------------------------
    // 3. LEFT FLOATING TOOL CAPSULE (44x44px touch targets split 4 + 5)
    // ------------------------------------------------------------------------
    d->leftToolCapsule = new QFrame(d->widgetStack);
    d->leftToolCapsule->setObjectName(QStringLiteral("brocreateLeftToolCapsule"));
    QVBoxLayout *ltLayout = new QVBoxLayout(d->leftToolCapsule);
    ltLayout->setContentsMargins(6, 8, 6, 8);
    ltLayout->setSpacing(4);

    d->leftToolGroup = new QButtonGroup(this);
    d->leftToolGroup->setExclusive(true);

    struct ToolEntry {
        BrocreateGlyph glyph;
        QString label;
        QString toolId;
        bool isQuickMenu;
    };

    const QList<ToolEntry> topTools = {
        {BrocreateGlyph::Move, i18n("Move / Transform"), QStringLiteral("KritaTransform/KisToolMove"), false},
        {BrocreateGlyph::Brush, i18n("Freehand Brush"), QStringLiteral("KritaShape/KisToolBrush"), false},
        {BrocreateGlyph::VectorLine, i18n("Vector Line"), QStringLiteral("KritaShape/KisToolLine"), false},
        {BrocreateGlyph::Fill, i18n("Fill"), QStringLiteral("KritaFill/KisToolFill"), false}
    };

    const QList<ToolEntry> bottomTools = {
        {BrocreateGlyph::Lasso, i18n("Lasso Selection"), QStringLiteral("KisToolSelectOutline"), false},
        {BrocreateGlyph::RectSelect, i18n("Rectangular Selection"), QStringLiteral("KisToolSelectRectangular"), false},
        {BrocreateGlyph::Eyedropper, i18n("Eyedropper"), QStringLiteral("KritaSelected/KisToolColorSampler"), false},
        {BrocreateGlyph::Symmetry, i18n("Symmetry"), QStringLiteral("KisToolMultiBrush"), false},
        {BrocreateGlyph::QuickAdd, i18n("Quick Settings"), QString(), true}
    };

    auto addCapsuleTool = [&](const ToolEntry &entry, bool defaultChecked) {
        QToolButton *btn = new QToolButton(d->leftToolCapsule);
        btn->setIcon(makeBrocreateIcon(entry.glyph));
        btn->setIconSize(QSize(22, 22));
        btn->setFixedSize(44, 44);
        btn->setToolTip(entry.label);
        btn->setProperty("brocreateToolId", entry.toolId);
        if (entry.isQuickMenu) {
            btn->setCheckable(false);
            connect(btn, SIGNAL(clicked()), this, SLOT(slotBrocreateQuickSettingsMenu()));
        } else {
            btn->setCheckable(true);
            btn->setChecked(defaultChecked);
            d->leftToolGroup->addButton(btn);
            d->leftToolButtons.append(btn);
            connect(btn, SIGNAL(clicked()), this, SLOT(slotBrocreateToolButtonClicked()));
        }
        ltLayout->addWidget(btn, 0, Qt::AlignHCenter);
    };

    for (int i = 0; i < topTools.size(); ++i) {
        addCapsuleTool(topTools.at(i), i == 1);
    }

    QFrame *capsuleDivider = new QFrame(d->leftToolCapsule);
    capsuleDivider->setObjectName(QStringLiteral("brocreateDividerH"));
    capsuleDivider->setFixedSize(30, 1);
    ltLayout->addWidget(capsuleDivider, 0, Qt::AlignHCenter);

    for (const ToolEntry &entry : bottomTools) {
        addCapsuleTool(entry, false);
    }

    d->leftToolCapsule->adjustSize();

    updateBrocreateColorSwatch();
    updateBrocreateFloatingVisibility(d->widgetStack->currentIndex() == 1);
    slotUpdateBrocreateOverlayGeometry();
}

void KisMainWindow::applyBrocreateDarkTheme()
{
    const QString brocreateQss = QStringLiteral(R"(
        QMainWindow, KisMainWindow, QDialog {
            background-color: #0B0B0D;
            color: #F4F4F5;
        }
        QMdiArea {
            background-color: #0B0B0D;
            border: none;
        }
        QFrame#brocreateTopLeftIsland,
        QFrame#brocreateTopCenterIsland {
            background-color: #141417;
            border: 1px solid #27272A;
            border-radius: 14px;
        }
        QFrame#brocreateLeftToolCapsule {
            background-color: #141417;
            border: 1px solid #27272A;
            border-radius: 16px;
        }
        QFrame#brocreateDividerV,
        QFrame#brocreateDividerH {
            background-color: #27272A;
            border: none;
        }
        QFrame#brocreateTopLeftIsland QToolButton,
        QFrame#brocreateTopCenterIsland QToolButton,
        QFrame#brocreateLeftToolCapsule QToolButton {
            background-color: transparent;
            border: none;
            border-radius: 10px;
            padding: 4px;
        }
        QFrame#brocreateTopLeftIsland QToolButton:hover,
        QFrame#brocreateTopCenterIsland QToolButton:hover,
        QFrame#brocreateLeftToolCapsule QToolButton:hover {
            background-color: #1E1E22;
        }
        QFrame#brocreateTopLeftIsland QToolButton:checked,
        QFrame#brocreateTopCenterIsland QToolButton:checked,
        QFrame#brocreateLeftToolCapsule QToolButton:checked {
            background-color: #0066FF;
        }
        QComboBox#brocreateBlendCombo {
            background-color: #1E1E22;
            color: #F4F4F5;
            border: 1px solid #27272A;
            border-radius: 8px;
            padding: 3px 8px;
            font-weight: 600;
        }
        QComboBox#brocreateBlendCombo::drop-down {
            border: none;
            width: 18px;
        }
        QDockWidget {
            background-color: #141417;
            color: #F4F4F5;
            border: 1px solid #27272A;
            border-radius: 14px;
        }
        QDockWidget > QWidget {
            background-color: #141417;
            border-radius: 12px;
        }
        QTabBar::tab {
            background-color: #1E1E22;
            color: #A1A1AA;
            border: none;
            border-radius: 8px;
            padding: 6px 12px;
            margin: 2px;
        }
        QTabBar::tab:selected {
            background-color: #0066FF;
            color: #FFFFFF;
            font-weight: 600;
        }
        QListView, QTreeView, QTableView {
            background-color: #141417;
            alternate-background-color: #18181C;
            color: #F4F4F5;
            border: none;
            outline: none;
        }
        QListView::item, QTreeView::item {
            background-color: #1E1E22;
            border-radius: 8px;
            margin: 2px 4px;
            padding: 4px 6px;
            min-height: 36px;
        }
        QListView::item:selected, QTreeView::item:selected {
            background-color: #0066FF;
            color: #FFFFFF;
        }
        QPushButton, QToolButton {
            background-color: #1E1E22;
            color: #F4F4F5;
            border: 1px solid #27272A;
            border-radius: 8px;
            padding: 4px 8px;
        }
        QPushButton:checked, QToolButton:checked {
            background-color: #0066FF;
            border-color: #0066FF;
            color: #FFFFFF;
        }
        QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
            background-color: #1E1E22;
            color: #F4F4F5;
            border: 1px solid #27272A;
            border-radius: 8px;
            padding: 4px 8px;
        }
        QProgressBar {
            background-color: #1E1E22;
            border: 1px solid #27272A;
            border-radius: 8px;
            text-align: center;
            color: #FFFFFF;
        }
        QProgressBar::chunk {
            background-color: #0066FF;
            border-radius: 7px;
        }
        QMenu {
            background-color: #141417;
            color: #F4F4F5;
            border: 1px solid #27272A;
            border-radius: 12px;
            padding: 6px;
        }
        QMenu::item {
            padding: 8px 18px;
            border-radius: 8px;
        }
        QMenu::item:selected {
            background-color: #0066FF;
            color: #FFFFFF;
        }
        QScrollBar:vertical, QScrollBar:horizontal {
            background: #141417;
            width: 6px;
            height: 6px;
            border-radius: 3px;
        }
        QScrollBar::handle:vertical, QScrollBar::handle:horizontal {
            background: #27272A;
            border-radius: 3px;
            min-height: 24px;
            min-width: 24px;
        }
        QDockWidget#TimelineDocker, QDockWidget#OnionSkinsDocker {
            background-color: #121215;
        }
    )");

    this->setStyleSheet(brocreateQss);

    if (d->welcomeScroller) {
        d->welcomeScroller->setStyleSheet(QStringLiteral(R"(
            QScrollArea, KisWelcomePageWidget {
                background-color: #121214;
                color: #F4F4F5;
                border: none;
            }
            QFrame, QAbstractButton {
                border-radius: 16px;
            }
            QPushButton {
                background-color: #232327;
                color: #F4F4F5;
                border: 1px solid #27272A;
                border-radius: 12px;
                padding: 8px 14px;
            }
            QPushButton:hover {
                background-color: #2B2B30;
            }
            QPushButton:checked, QPushButton:default {
                background-color: #0066FF;
                border-color: #0066FF;
                color: #FFFFFF;
            }
        )"));
    }
}

void KisMainWindow::updateBrocreateFloatingVisibility(bool showCanvasUI)
{
    if (!d->brocreateUiInitialized) {
        return;
    }
    if (d->topLeftIsland) {
        d->topLeftIsland->setVisible(showCanvasUI);
        if (showCanvasUI) d->topLeftIsland->raise();
    }
    if (d->topCenterIsland) {
        d->topCenterIsland->setVisible(showCanvasUI);
        if (showCanvasUI) d->topCenterIsland->raise();
    }
    if (d->leftToolCapsule) {
        d->leftToolCapsule->setVisible(showCanvasUI);
        if (showCanvasUI) d->leftToolCapsule->raise();
    }
}

void KisMainWindow::slotUpdateBrocreateOverlayGeometry()
{
    if (!d->brocreateUiInitialized || !d->widgetStack) {
        return;
    }

    const QRect area = d->widgetStack->rect();
    const int margin = 12;

    if (d->topLeftIsland) {
        d->topLeftIsland->adjustSize();
        d->topLeftIsland->move(margin, margin);
        d->topLeftIsland->raise();
    }

    if (d->topCenterIsland) {
        d->topCenterIsland->adjustSize();
        int tcX = (area.width() - d->topCenterIsland->width()) / 2;
        if (d->topLeftIsland) {
            const int minX = d->topLeftIsland->geometry().right() + 10;
            if (tcX < minX) {
                tcX = qMin(minX, qMax(margin, area.width() - d->topCenterIsland->width() - margin));
            }
        }
        d->topCenterIsland->move(tcX, margin);
        d->topCenterIsland->raise();
    }

    if (d->leftToolCapsule) {
        d->leftToolCapsule->adjustSize();
        const int ltY = qMax(72, (area.height() - d->leftToolCapsule->height()) / 2);
        d->leftToolCapsule->move(margin, ltY);
        d->leftToolCapsule->raise();
    }
}

void KisMainWindow::updateBrocreateColorSwatch()
{
    if (!d->colorSwatchButton || !d->viewManager || !d->viewManager->canvasResourceProvider()) {
        return;
    }
    const QColor c = d->viewManager->canvasResourceProvider()->fgColor().toQColor();
    const QColor displayColor = c.isValid() ? c : QColor("#E06D94");
    d->colorSwatchButton->setStyleSheet(
        QStringLiteral("QPushButton#brocreateColorSwatch { background-color: %1; border: 2px solid #F4F4F5; border-radius: 6px; padding: 0px; }")
            .arg(displayColor.name(QColor::HexRgb)));
}

void KisMainWindow::arrangeBrocreateStudioDocks(bool animationMode)
{
    QDockWidget *colorDock = dockWidget(QStringLiteral("ColorSelectorNg"));
    QDockWidget *layerDock = dockWidget(QStringLiteral("KisLayerBox"));
    QDockWidget *presetDock = dockWidget(QStringLiteral("PresetDocker"));
    QDockWidget *timelineDock = dockWidget(QStringLiteral("TimelineDocker"));
    QDockWidget *onionDock = dockWidget(QStringLiteral("OnionSkinsDocker"));

    const Qt::DockWidgetArea studioArea = animationMode ? Qt::LeftDockWidgetArea : Qt::RightDockWidgetArea;

    QList<QDockWidget *> studioDocks;
    if (colorDock) studioDocks << colorDock;
    if (layerDock) studioDocks << layerDock;
    if (presetDock) studioDocks << presetDock;

    for (int i = 0; i < studioDocks.size(); ++i) {
        QDockWidget *dw = studioDocks.at(i);
        addDockWidget(studioArea, dw);
        dw->setVisible(true);
        if (i > 0) {
            splitDockWidget(studioDocks.at(i - 1), dw, Qt::Vertical);
        }
    }

    if (timelineDock) {
        addDockWidget(Qt::BottomDockWidgetArea, timelineDock);
        timelineDock->setVisible(animationMode);
    }
    if (onionDock) {
        addDockWidget(Qt::BottomDockWidgetArea, onionDock);
        onionDock->setVisible(animationMode);
        if (timelineDock && animationMode) {
            splitDockWidget(timelineDock, onionDock, Qt::Horizontal);
        }
    }

    showDockerTitleBars(false);
    QTimer::singleShot(0, this, SLOT(slotUpdateBrocreateOverlayGeometry()));
}

void KisMainWindow::slotBrocreateBlendModeChanged(int index)
{
    if (!d->blendModeCombo || !d->viewManager || !d->viewManager->canvasResourceProvider()) {
        return;
    }
    const QString compositeId = d->blendModeCombo->itemData(index).toString();
    if (!compositeId.isEmpty()) {
        d->viewManager->canvasResourceProvider()->setCurrentCompositeOp(compositeId);
    }
}

void KisMainWindow::slotBrocreateOpacityChanged(qreal normalizedValue)
{
    if (d->updatingFromResource || !d->viewManager || !d->viewManager->canvasResourceProvider()) {
        return;
    }
    const qreal clamped = qBound<qreal>(0.0, normalizedValue, 1.0);
    if (d->opacityPill) {
        d->opacityPill->setDisplayText(QString("%1%").arg(qRound(clamped * 100.0)));
    }
    d->viewManager->canvasResourceProvider()->setOpacity(clamped);
}

void KisMainWindow::slotBrocreateBrushSizeChanged(qreal normalizedValue)
{
    if (d->updatingFromResource || !d->viewManager || !d->viewManager->canvasResourceProvider()) {
        return;
    }
    const qreal clamped = qBound<qreal>(0.0, normalizedValue, 1.0);
    const qreal sizePx = qMax<qreal>(1.0, std::pow(500.0, clamped));
    if (d->brushSizePill) {
        d->brushSizePill->setDisplayText(QString::number(sizePx, 'f', 2) + QStringLiteral(" px"));
    }
    d->viewManager->canvasResourceProvider()->resourceManager()->setResource(KoCanvasResource::Size, sizePx);
}

void KisMainWindow::slotBrocreateToolButtonClicked()
{
    QToolButton *btn = qobject_cast<QToolButton *>(sender());
    if (!btn || !d->viewManager || !d->viewManager->toolManager()) {
        return;
    }
    const QString toolId = btn->property("brocreateToolId").toString();
    if (!toolId.isEmpty()) {
        d->viewManager->toolManager()->switchToolRequested(toolId);
    }
}

void KisMainWindow::slotBrocreateTriggerActionByName()
{
    QToolButton *btn = qobject_cast<QToolButton *>(sender());
    if (!btn) {
        return;
    }
    const QString actionName = btn->property("brocreateAction").toString();
    if (actionName.isEmpty()) {
        return;
    }
    if (QAction *act = actionCollection()->action(actionName)) {
        act->trigger();
    } else if (d->viewManager && d->viewManager->toolManager()) {
        d->viewManager->toolManager()->switchToolRequested(actionName);
    }
}

void KisMainWindow::slotBrocreateToggleAnimationLayout()
{
    d->animationLayoutActive = !d->animationLayoutActive;
    if (d->animationModeBtn) {
        d->animationModeBtn->setChecked(d->animationLayoutActive);
    }
    arrangeBrocreateStudioDocks(d->animationLayoutActive);
}

void KisMainWindow::slotBrocreateToggleFullscreenCanvasOnly()
{
    d->canvasOnlyOverlayHidden = !d->canvasOnlyOverlayHidden;
    if (d->fullscreenBtn) {
        d->fullscreenBtn->setChecked(d->canvasOnlyOverlayHidden);
    }
    if (QAction *canvasOnly = actionCollection()->action(QStringLiteral("view_show_canvas_only"))) {
        if (canvasOnly->isChecked() != d->canvasOnlyOverlayHidden) {
            canvasOnly->trigger();
        }
    } else {
        toggleDockersVisibility(!d->canvasOnlyOverlayHidden);
    }
    updateBrocreateFloatingVisibility(!d->canvasOnlyOverlayHidden && d->widgetStack->currentIndex() == 1);
}

void KisMainWindow::slotBrocreateToggleStudioDock()
{
    const bool showStudio = d->studioToggleBtn ? d->studioToggleBtn->isChecked() : true;
    if (d->toggleDockers) {
        d->toggleDockers->setChecked(showStudio);
    } else {
        toggleDockersVisibility(showStudio);
    }
}

void KisMainWindow::slotBrocreateQuickSettingsMenu()
{
    QMenu menu(this);
    menu.addAction(i18n("New Canvas..."), this, SLOT(slotFileNew()));
    menu.addAction(i18n("Save Canvas"), this, SLOT(slotFileSave()));
    menu.addSeparator();
    menu.addAction(d->animationLayoutActive ? i18n("Switch to Illustration Studio") : i18n("Switch to Animation & Onion Skin"),
                   this, SLOT(slotBrocreateToggleAnimationLayout()));
    menu.addAction(i18n("Fit Canvas to Screen"), this, [this]() {
        if (QAction *a = actionCollection()->action(QStringLiteral("zoom_to_fit"))) {
            a->trigger();
        } else if (QAction *a = actionCollection()->action(QStringLiteral("view_zoom_fit_to_page"))) {
            a->trigger();
        }
    });
    menu.addAction(i18n("Mirror Canvas"), this, [this]() {
        if (QAction *a = actionCollection()->action(QStringLiteral("mirror_canvas"))) {
            a->trigger();
        }
    });
    menu.addSeparator();
    menu.addAction(i18n("Preferences..."), this, SLOT(slotPreferences()));
    menu.addAction(i18n("Return to Gallery"), this, SLOT(slotFileCloseAll()));

    const QPoint pos = d->leftToolCapsule
        ? d->leftToolCapsule->mapToGlobal(QPoint(d->leftToolCapsule->width() + 8, d->leftToolCapsule->height() - 44))
        : QCursor::pos();
    menu.exec(pos);
}

void KisMainWindow::slotBrocreateCopyPasteMenu(const QPoint &globalPos)
{
    QMenu menu(this);
    menu.setTitle(i18n("Copy & Paste"));
    auto addActionIfPresent = [&menu, this](const QString &name, const QString &fallbackLabel) {
        if (QAction *a = actionCollection()->action(name)) {
            menu.addAction(a);
        } else {
            Q_UNUSED(fallbackLabel);
        }
    };
    addActionIfPresent(QStringLiteral("edit_cut"), i18n("Cut"));
    addActionIfPresent(QStringLiteral("edit_copy"), i18n("Copy"));
    addActionIfPresent(QStringLiteral("copy_merged"), i18n("Copy All"));
    addActionIfPresent(QStringLiteral("edit_paste"), i18n("Paste"));
    menu.addSeparator();
    addActionIfPresent(QStringLiteral("duplicatelayer"), i18n("Duplicate"));
    addActionIfPresent(QStringLiteral("clear"), i18n("Clear"));
    addActionIfPresent(QStringLiteral("deselect"), i18n("Deselect"));

    if (!menu.isEmpty()) {
        menu.exec(globalPos);
    }
}

void KisMainWindow::slotBrocreateLongPressEyedropper()
{
    if (d->viewManager && d->viewManager->toolManager()) {
        d->viewManager->toolManager()->switchToolRequested(QStringLiteral("KritaSelected/KisToolColorSampler"));
    }
}

void KisMainWindow::slotBrocreateCanvasResourceChanged(int key, const QVariant &value)
{
    Q_UNUSED(value);
    if (!d->viewManager || !d->viewManager->canvasResourceProvider()) {
        return;
    }

    d->updatingFromResource = true;
    if (key == KoCanvasResource::ForegroundColor) {
        updateBrocreateColorSwatch();
    } else if (key == KoCanvasResource::Opacity && d->opacityPill) {
        const qreal op = d->viewManager->canvasResourceProvider()->opacity();
        d->opacityPill->setNormalizedValue(op);
        d->opacityPill->setDisplayText(QString("%1%").arg(qRound(op * 100.0)));
    } else if (key == KoCanvasResource::Size && d->brushSizePill) {
        const qreal sz = value.toDouble();
        if (sz > 0.1) {
            const qreal norm = qBound<qreal>(0.0, std::log(sz) / std::log(500.0), 1.0);
            d->brushSizePill->setNormalizedValue(norm);
            d->brushSizePill->setDisplayText(QString::number(sz, 'f', 2) + QStringLiteral(" px"));
        }
    }
    d->updatingFromResource = false;
}

bool KisMainWindow::handleBrocreateTabletEvent(QTabletEvent *tabletEvent)
{
    Q_UNUSED(tabletEvent);
    d->lastStylusTimer.restart();
    if (d->longPressEyedropperTimer && d->longPressEyedropperTimer->isActive()) {
        d->longPressEyedropperTimer->stop();
    }
    return false; // Never block 144Hz Adreno stylus drawing
}

bool KisMainWindow::handleBrocreateTouchEvent(QObject *watched, QTouchEvent *touchEvent)
{
    Q_UNUSED(watched);
    const QList<QTouchEvent::TouchPoint> points = touchEvent->touchPoints();
    if (points.isEmpty()) {
        return false;
    }

    const int count = points.size();
    QPointF centroid(0, 0);
    QPointF globalCentroid(0, 0);
    for (const QTouchEvent::TouchPoint &pt : points) {
        centroid += pt.pos();
        globalCentroid += pt.screenPos();
    }
    centroid /= count;
    globalCentroid /= count;

    // Palm Rejection: block stray palm or finger touches while stylus is active
    if (count == 1) {
        const QSizeF diam = points.first().ellipseDiameters();
        const bool looksLikePalm = (diam.width() > 38.0 || diam.height() > 38.0);
        const bool stylusRecentlyActive = d->lastStylusTimer.isValid() && d->lastStylusTimer.elapsed() < 350;
        if (looksLikePalm || stylusRecentlyActive) {
            if (d->longPressEyedropperTimer) {
                d->longPressEyedropperTimer->stop();
            }
            touchEvent->accept();
            return true;
        }
    }

    if (touchEvent->type() == QEvent::TouchBegin) {
        d->touchStartTimer.restart();
        d->touchStartCentroid = centroid;
        d->lastTouchCentroid = centroid;
        d->maxTouchPoints = count;
        d->touchGestureConsumed = false;
        d->pinchFitTriggered = false;
        d->initialPinchSpan = 0.0;

        if (count == 1 && d->longPressEyedropperTimer) {
            d->longPressEyedropperTimer->start();
        } else if (d->longPressEyedropperTimer) {
            d->longPressEyedropperTimer->stop();
        }
        return false;
    }

    if (touchEvent->type() == QEvent::TouchUpdate) {
        if (count > d->maxTouchPoints) {
            d->maxTouchPoints = count;
            d->touchStartCentroid = centroid;
            if (d->longPressEyedropperTimer) {
                d->longPressEyedropperTimer->stop();
            }
        }

        if (count == 1 && d->longPressEyedropperTimer && d->longPressEyedropperTimer->isActive()) {
            if (QLineF(centroid, d->touchStartCentroid).length() > 14.0) {
                d->longPressEyedropperTimer->stop();
            }
        }

        // QuickPinch -> Fit to screen
        if (count == 2 && !d->pinchFitTriggered) {
            const qreal span = QLineF(points.at(0).pos(), points.at(1).pos()).length();
            if (d->initialPinchSpan <= 1.0) {
                d->initialPinchSpan = span;
            } else if (d->initialPinchSpan > 70.0 && (d->initialPinchSpan - span) > 95.0 && d->touchStartTimer.elapsed() < 280) {
                d->pinchFitTriggered = true;
                d->touchGestureConsumed = true;
                if (QAction *a = actionCollection()->action(QStringLiteral("zoom_to_fit"))) {
                    a->trigger();
                } else if (QAction *a = actionCollection()->action(QStringLiteral("view_zoom_fit_to_page"))) {
                    a->trigger();
                }
                touchEvent->accept();
                return true;
            }
        }

        // 3-Finger Swipe Down -> Copy/Paste Menu
        if (count == 3 && !d->touchGestureConsumed) {
            const qreal dx = centroid.x() - d->touchStartCentroid.x();
            const qreal dy = centroid.y() - d->touchStartCentroid.y();
            if (dy > 65.0 && dy > std::abs(dx) * 1.35) {
                d->touchGestureConsumed = true;
                const QPoint menuPos = globalCentroid.toPoint();
                QTimer::singleShot(0, this, [this, menuPos]() {
                    slotBrocreateCopyPasteMenu(menuPos);
                });
                touchEvent->accept();
                return true;
            }
        }

        d->lastTouchCentroid = centroid;
        return false;
    }

    if (touchEvent->type() == QEvent::TouchEnd || touchEvent->type() == QEvent::TouchCancel) {
        if (d->longPressEyedropperTimer) {
            d->longPressEyedropperTimer->stop();
        }

        const qint64 elapsed = d->touchStartTimer.isValid() ? d->touchStartTimer.elapsed() : 1000;
        const qreal moved = QLineF(centroid, d->touchStartCentroid).length();

        if (!d->touchGestureConsumed && touchEvent->type() == QEvent::TouchEnd && elapsed < 340 && moved < 32.0) {
            if (d->maxTouchPoints == 2) {
                d->touchGestureConsumed = true;
                undo();
                touchEvent->accept();
                return true;
            } else if (d->maxTouchPoints == 3) {
                d->touchGestureConsumed = true;
                redo();
                touchEvent->accept();
                return true;
            } else if (d->maxTouchPoints == 4) {
                d->touchGestureConsumed = true;
                slotBrocreateToggleFullscreenCanvasOnly();
                touchEvent->accept();
                return true;
            }
        }

        d->maxTouchPoints = 0;
        d->initialPinchSpan = 0.0;
        return false;
    }

    return false;
}

bool KisMainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == d->widgetStack && event->type() == QEvent::Resize) {
        slotUpdateBrocreateOverlayGeometry();
    }

    switch (event->type()) {
    case QEvent::TabletPress:
    case QEvent::TabletMove:
    case QEvent::TabletRelease:
        return handleBrocreateTabletEvent(static_cast<QTabletEvent *>(event));
    case QEvent::TouchBegin:
    case QEvent::TouchUpdate:
    case QEvent::TouchEnd:
    case QEvent::TouchCancel:
        if (handleBrocreateTouchEvent(watched, static_cast<QTouchEvent *>(event))) {
            return true;
        }
        break;
    default:
        break;
    }

    return KXmlGuiWindow::eventFilter(watched, event);
}

#include <moc_KisMainWindow.cpp>
