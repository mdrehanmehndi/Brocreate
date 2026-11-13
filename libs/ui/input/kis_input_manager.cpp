/* This file is part of the KDE project
 *
 *  SPDX-FileCopyrightText: 2012 Arjen Hiemstra <ahiemstra@heimr.nl>
 *  SPDX-FileCopyrightText: 2015 Michael Abrahams <miabraha@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_input_manager.h"

#include <kis_debug.h>
#include <QQueue>
#include <klocalizedstring.h>
#include <QAction>
#include <QApplication>
#include <QElapsedTimer>
#include <QHash>
#include <QLineF>
#include <QMenu>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QTouchEvent>
#include <QWidget>
#include <QWindow>

#include <KoColor.h>
#include <KoToolManager.h>
#include <KoPointerEvent.h>

#include "kis_tool_proxy.h"

#include <kis_config.h>
#include <kis_config_notifier.h>
#include <kis_canvas2.h>
#include <kis_coordinates_converter.h>
#include <KisViewManager.h>
#include <kis_image.h>
#include <kis_paint_device.h>
#include <kis_canvas_resource_provider.h>
#include <kis_favorite_resource_manager.h>

#include "kis_abstract_input_action.h"
#include "kis_tool_invocation_action.h"
#include "KisPopupWidgetInterface.h"

#include "kis_shortcut_matcher.h"
#include "kis_stroke_shortcut.h"
#include "kis_single_action_shortcut.h"
#include "kis_touch_shortcut.h"

#include "kis_input_profile.h"
#include "kis_input_profile_manager.h"
#include "kis_shortcut_configuration.h"

#include <input/kis_tablet_debugger.h>
#include <kis_signal_compressor.h>

#include "kis_extended_modifiers_mapper.h"
#include "kis_input_manager_p.h"
#include "kis_algebra_2d.h"
#include "config-qt-patches-present.h"

namespace {

constexpr qint64 BROCREATE_144HZ_BATCH_NS = 6500000; // ~144Hz (6.94ms frame budget)
constexpr int BROCREATE_144HZ_DELAY_MS = 6;
constexpr int BROCREATE_PALM_COOLDOWN_MS = 220;
constexpr int BROCREATE_EYEDROPPER_HOLD_MS = 350;
constexpr int BROCREATE_TAP_MAX_MS = 360;
constexpr qreal BROCREATE_TAP_SLOP_PX = 18.0;
constexpr qreal BROCREATE_SWIPE_DOWN_MIN_PX = 55.0;
constexpr qreal BROCREATE_QUICK_PINCH_RATIO = 0.60;
constexpr int BROCREATE_QUICK_PINCH_MAX_MS = 280;

class BrocreateColorLoupe : public QWidget
{
public:
    explicit BrocreateColorLoupe(QWidget *parent = nullptr)
        : QWidget(parent, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
    {
        setAttribute(Qt::WA_TransparentForInput, true);
        setAttribute(Qt::WA_TranslucentBackground, true);
        setAttribute(Qt::WA_ShowWithoutActivating, true);
        setFixedSize(76, 76);
    }

    void setSampledColor(const QColor &color, const QPoint &globalPos)
    {
        m_color = color;
        move(globalPos - QPoint(width() / 2, height() + 28));
        if (!isVisible()) {
            show();
        }
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);

        const QRectF outerRect(3.0, 3.0, width() - 6.0, height() - 6.0);
        p.setPen(QPen(QColor(0x27, 0x27, 0x2A), 2.5));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(outerRect);

        const QRectF ringRect = outerRect.adjusted(6.0, 6.0, -6.0, -6.0);
        p.setPen(QPen(m_color, 11.0));
        p.drawEllipse(ringRect);

        const QRectF innerRect = ringRect.adjusted(6.0, 6.0, -6.0, -6.0);
        p.setPen(QPen(QColor(255, 255, 255, 220), 1.5));
        p.setBrush(QColor(20, 20, 23, 120));
        p.drawEllipse(innerRect);
    }

private:
    QColor m_color = QColor(0x00, 0x66, 0xFF);
};

class BrocreateGestureHandler : public QObject
{
public:
    explicit BrocreateGestureHandler(KisInputManager *manager)
        : QObject(manager)
        , m_manager(manager)
    {
        m_eyedropperTimer.setSingleShot(true);
        m_eyedropperTimer.setTimerType(Qt::PreciseTimer);
        connect(&m_eyedropperTimer, &QTimer::timeout, this, [this]() {
            onEyedropperTimeout();
        });
    }

    ~BrocreateGestureHandler() override
    {
        if (m_loupe) {
            m_loupe->hide();
            m_loupe->deleteLater();
        }
    }

    int batchDelayMs() const
    {
        QWidget *w = m_manager && m_manager->canvas() ? m_manager->canvas()->canvasWidget() : nullptr;
        if (w && w->window() && w->window()->windowHandle() && w->window()->windowHandle()->screen()) {
            const qreal hz = w->window()->windowHandle()->screen()->refreshRate();
            if (hz >= 30.0 && hz <= 240.0) {
                return qBound(4, static_cast<int>(1000.0 / hz), 16);
            }
        }
        return BROCREATE_144HZ_DELAY_MS;
    }

    bool shouldBatchHiResMove(const QPointF &pos)
    {
        if (!m_hiResBatchTimer.isValid()) {
            m_hiResBatchTimer.start();
            m_lastHiResPos = pos;
            return false;
        }
        const qint64 elapsedNs = m_hiResBatchTimer.nsecsElapsed();
        const qreal dist = QLineF(m_lastHiResPos, pos).length();
        const qint64 budgetNs = static_cast<qint64>(batchDelayMs()) * 1000000LL;
        if (elapsedNs < qMin(budgetNs, BROCREATE_144HZ_BATCH_NS) && dist < 8.0) {
            return true;
        }
        m_hiResBatchTimer.restart();
        m_lastHiResPos = pos;
        return false;
    }

    void resetHiResBatching()
    {
        m_hiResBatchTimer.invalidate();
    }

    void onTabletPress()
    {
        m_stylusDown = true;
        m_lastStylusTimer.restart();
        cancelActiveGesture();
    }

    void onTabletMove()
    {
        m_lastStylusTimer.restart();
    }

    void onTabletRelease()
    {
        m_stylusDown = false;
        m_lastStylusTimer.restart();
    }

    bool isPalmRejected() const
    {
        return m_stylusDown ||
               (m_lastStylusTimer.isValid() && m_lastStylusTimer.elapsed() < BROCREATE_PALM_COOLDOWN_MS);
    }

    void cancelActiveGesture()
    {
        m_eyedropperTimer.stop();
        m_eyedropperActive = false;
        if (m_loupe) {
            m_loupe->hide();
        }
        m_gestureConsumed = true;
        m_touchStartPos.clear();
        m_touchLastPos.clear();
    }

    bool handleTouchBegin(QTouchEvent *event, KisShortcutMatcher &matcher)
    {
        if (isPalmRejected()) {
            cancelActiveGesture();
            matcher.touchCancelEvent(event);
            return true;
        }

        m_eyedropperTimer.stop();
        m_eyedropperActive = false;
        if (m_loupe) {
            m_loupe->hide();
        }

        m_gestureTimer.restart();
        m_touchStartPos.clear();
        m_touchLastPos.clear();
        m_gestureMovedTooMuch = false;
        m_swipeTriggered = false;
        m_pinchTriggered = false;
        m_gestureConsumed = false;
        m_initialPinchSpan = 0.0;
        m_pinchStartMs = 0;

        int activeCount = 0;
        QPointF firstPos;
        const QList<QTouchEvent::TouchPoint> points = event->touchPoints();
        for (const QTouchEvent::TouchPoint &tp : points) {
            if (tp.state() != Qt::TouchPointReleased) {
                m_touchStartPos.insert(tp.id(), tp.pos());
                m_touchLastPos.insert(tp.id(), tp.pos());
                if (activeCount == 0) {
                    firstPos = tp.pos();
                }
                ++activeCount;
            }
        }

        m_maxTouchCount = activeCount;
        if (activeCount == 1) {
            m_eyedropperPos = firstPos;
            m_eyedropperTimer.start(BROCREATE_EYEDROPPER_HOLD_MS);
        }

        return false;
    }

    bool handleTouchUpdate(QTouchEvent *event, KisShortcutMatcher &matcher)
    {
        if (isPalmRejected()) {
            cancelActiveGesture();
            matcher.touchCancelEvent(event);
            return true;
        }

        QList<QTouchEvent::TouchPoint> activePoints;
        const QList<QTouchEvent::TouchPoint> points = event->touchPoints();
        for (const QTouchEvent::TouchPoint &tp : points) {
            if (tp.state() != Qt::TouchPointReleased) {
                activePoints.append(tp);
            }
        }

        if (m_eyedropperActive) {
            if (!activePoints.isEmpty()) {
                m_eyedropperPos = activePoints.first().pos();
                sampleColorAt(m_eyedropperPos);
            }
            return true;
        }

        if (m_gestureConsumed) {
            return true;
        }

        m_maxTouchCount = qMax(m_maxTouchCount, activePoints.size());

        if (activePoints.size() != 1) {
            m_eyedropperTimer.stop();
        }

        for (const QTouchEvent::TouchPoint &tp : activePoints) {
            if (!m_touchStartPos.contains(tp.id())) {
                m_touchStartPos.insert(tp.id(), tp.pos());
            }
            m_touchLastPos[tp.id()] = tp.pos();

            const qreal dist = QLineF(m_touchStartPos.value(tp.id()), tp.pos()).length();
            if (dist > BROCREATE_TAP_SLOP_PX) {
                m_gestureMovedTooMuch = true;
                m_eyedropperTimer.stop();
            } else if (activePoints.size() == 1) {
                m_eyedropperPos = tp.pos();
            }
        }

        // 3-Finger Swipe Down = Copy/Paste Menu
        if (activePoints.size() == 3 && !m_swipeTriggered && !m_pinchTriggered && m_gestureTimer.elapsed() < 700) {
            qreal sumDx = 0.0;
            qreal sumDy = 0.0;
            QPointF centroid(0.0, 0.0);
            bool allMovedDown = true;

            for (const QTouchEvent::TouchPoint &tp : activePoints) {
                const QPointF start = m_touchStartPos.value(tp.id(), tp.pos());
                const QPointF delta = tp.pos() - start;
                sumDx += delta.x();
                sumDy += delta.y();
                centroid += tp.pos();
                if (delta.y() < 25.0) {
                    allMovedDown = false;
                }
            }

            const qreal avgDx = sumDx / 3.0;
            const qreal avgDy = sumDy / 3.0;
            centroid /= 3.0;

            if (allMovedDown && avgDy > BROCREATE_SWIPE_DOWN_MIN_PX && avgDy > qAbs(avgDx) * 1.35) {
                m_swipeTriggered = true;
                m_gestureConsumed = true;
                m_eyedropperTimer.stop();
                matcher.touchCancelEvent(event);
                showCopyPasteMenu(centroid);
                return true;
            }
        }

        // QuickPinch = Fit to screen
        if (activePoints.size() == 2 && !m_pinchTriggered && !m_swipeTriggered) {
            const qreal span = QLineF(activePoints[0].pos(), activePoints[1].pos()).length();
            if (m_initialPinchSpan <= 0.0) {
                m_initialPinchSpan = span;
                m_pinchStartMs = m_gestureTimer.elapsed();
            } else if (m_initialPinchSpan > 50.0) {
                const qreal ratio = span / m_initialPinchSpan;
                const qint64 dt = m_gestureTimer.elapsed() - m_pinchStartMs;
                if (ratio < BROCREATE_QUICK_PINCH_RATIO && dt < BROCREATE_QUICK_PINCH_MAX_MS) {
                    m_pinchTriggered = true;
                    m_gestureConsumed = true;
                    m_eyedropperTimer.stop();
                    matcher.touchCancelEvent(event);
                    triggerFitToScreen();
                    return true;
                }
            }
        }

        return false;
    }

    bool handleTouchEnd(QTouchEvent *event, KisShortcutMatcher &matcher)
    {
        m_eyedropperTimer.stop();

        if (m_eyedropperActive) {
            m_eyedropperActive = false;
            if (m_loupe) {
                m_loupe->hide();
            }
            return true;
        }

        if (m_gestureConsumed) {
            m_gestureConsumed = false;
            return true;
        }

        int remainingActive = 0;
        const QList<QTouchEvent::TouchPoint> points = event->touchPoints();
        for (const QTouchEvent::TouchPoint &tp : points) {
            if (tp.state() != Qt::TouchPointReleased) {
                ++remainingActive;
            }
        }

        if (remainingActive == 0 &&
            !isPalmRejected() &&
            !m_gestureMovedTooMuch &&
            !m_swipeTriggered &&
            !m_pinchTriggered &&
            m_gestureTimer.elapsed() < BROCREATE_TAP_MAX_MS) {
            bool fired = false;
            if (m_maxTouchCount == 2) {
                // 2-Finger Tap = Undo
                fired = triggerActionByCandidates({QStringLiteral("edit_undo")});
            } else if (m_maxTouchCount == 3) {
                // 3-Finger Tap = Redo
                fired = triggerActionByCandidates({QStringLiteral("edit_redo")});
            } else if (m_maxTouchCount == 4) {
                // 4-Finger Tap = Fullscreen / Canvas-Only
                fired = triggerActionByCandidates({
                    QStringLiteral("view_show_canvas_only"),
                    QStringLiteral("fullscreen")
                });
            }

            if (fired) {
                matcher.touchCancelEvent(event);
                return true;
            }
        }

        return false;
    }

    void handleTouchCancel()
    {
        m_eyedropperTimer.stop();
        m_eyedropperActive = false;
        m_gestureConsumed = false;
        if (m_loupe) {
            m_loupe->hide();
        }
        m_touchStartPos.clear();
        m_touchLastPos.clear();
    }

private:
    QAction *findActionByCandidates(const QStringList &names) const
    {
        if (!m_manager || !m_manager->canvas()) {
            return nullptr;
        }

        QList<QObject *> searchRoots;
        if (m_manager->canvas()->viewManager()) {
            searchRoots.append(m_manager->canvas()->viewManager());
        }
        if (m_manager->canvas()->canvasWidget() && m_manager->canvas()->canvasWidget()->window()) {
            searchRoots.append(m_manager->canvas()->canvasWidget()->window());
        }
        if (QApplication::activeWindow()) {
            searchRoots.append(QApplication::activeWindow());
        }

        for (const QString &name : names) {
            for (QObject *root : searchRoots) {
                if (!root) {
                    continue;
                }
                if (QAction *act = root->findChild<QAction *>(name)) {
                    return act;
                }
            }
        }
        return nullptr;
    }

    bool triggerActionByCandidates(const QStringList &names) const
    {
        if (QAction *act = findActionByCandidates(names)) {
            if (act->isEnabled()) {
                act->trigger();
                return true;
            }
        }
        return false;
    }

    void triggerFitToScreen() const
    {
        triggerActionByCandidates({
            QStringLiteral("view_zoom_to_page"),
            QStringLiteral("zoom_to_page"),
            QStringLiteral("view_zoom_fit_page"),
            QStringLiteral("zoom_to_100pct")
        });
    }

    void onEyedropperTimeout()
    {
        if (isPalmRejected() || m_gestureMovedTooMuch || m_swipeTriggered || m_pinchTriggered) {
            return;
        }
        m_eyedropperActive = true;
        m_gestureConsumed = true;
        sampleColorAt(m_eyedropperPos);
    }

    void sampleColorAt(const QPointF &widgetPos)
    {
        if (!m_manager || !m_manager->canvas()) {
            return;
        }

        KisCanvas2 *canvas = m_manager->canvas();
        KisImageWSP image = canvas->image();
        if (!image || !image->projection()) {
            return;
        }

        const KisCoordinatesConverter *converter = canvas->coordinatesConverter();
        if (!converter) {
            return;
        }

        const QPoint imagePt = converter->widgetToImage(widgetPos).toPoint();
        const QRect bounds = image->bounds();
        if (!bounds.contains(imagePt)) {
            return;
        }

        KoColor sampled(image->projection()->colorSpace());
        if (image->projection()->pixel(imagePt.x(), imagePt.y(), &sampled)) {
            if (canvas->viewManager() && canvas->viewManager()->canvasResourceProvider()) {
                canvas->viewManager()->canvasResourceProvider()->setFGColor(sampled);
            }

            QWidget *cw = canvas->canvasWidget();
            if (cw) {
                if (!m_loupe) {
                    m_loupe = new BrocreateColorLoupe(cw->window());
                }
                m_loupe->setSampledColor(sampled.toQColor(), cw->mapToGlobal(widgetPos.toPoint()));
            }
        }
    }

    void showCopyPasteMenu(const QPointF &widgetPos)
    {
        if (!m_manager || !m_manager->canvas() || !m_manager->canvas()->canvasWidget()) {
            return;
        }

        QWidget *cw = m_manager->canvas()->canvasWidget();
        QMenu *menu = new QMenu(cw);
        menu->setAttribute(Qt::WA_DeleteOnClose, true);
        menu->setStyleSheet(QStringLiteral(
            "QMenu {"
            "  background-color: #141417;"
            "  color: #F4F4F5;"
            "  border: 1px solid #27272A;"
            "  border-radius: 14px;"
            "  padding: 6px;"
            "  font-size: 14px;"
            "}"
            "QMenu::item {"
            "  padding: 10px 22px;"
            "  border-radius: 8px;"
            "}"
            "QMenu::item:selected {"
            "  background-color: #0066FF;"
            "  color: #FFFFFF;"
            "}"
            "QMenu::item:disabled {"
            "  color: #71717A;"
            "}"
            "QMenu::separator {"
            "  height: 1px;"
            "  background: #27272A;"
            "  margin: 4px 10px;"
            "}"
        ));

        auto addActionEntry = [this, menu](const QString &label, const QStringList &candidates) {
            QAction *target = findActionByCandidates(candidates);
            QAction *entry = menu->addAction(label);
            entry->setEnabled(target && target->isEnabled());
            if (target) {
                connect(entry, &QAction::triggered, target, &QAction::trigger);
            }
        };

        addActionEntry(i18n("Cut"), {QStringLiteral("edit_cut")});
        addActionEntry(i18n("Copy"), {QStringLiteral("edit_copy")});
        addActionEntry(i18n("Copy Merged"), {QStringLiteral("copy_merged")});
        menu->addSeparator();
        addActionEntry(i18n("Paste"), {QStringLiteral("edit_paste")});
        addActionEntry(i18n("Paste into New Layer"), {QStringLiteral("paste_new")});
        addActionEntry(i18n("Duplicate Layer"), {QStringLiteral("duplicatelayer")});
        menu->addSeparator();
        addActionEntry(i18n("Clear"), {QStringLiteral("clear")});

        menu->popup(cw->mapToGlobal(widgetPos.toPoint()));
    }

    KisInputManager *m_manager = nullptr;
    bool m_stylusDown = false;
    QElapsedTimer m_lastStylusTimer;
    QElapsedTimer m_hiResBatchTimer;
    QPointF m_lastHiResPos;

    QElapsedTimer m_gestureTimer;
    int m_maxTouchCount = 0;
    QHash<int, QPointF> m_touchStartPos;
    QHash<int, QPointF> m_touchLastPos;
    bool m_gestureMovedTooMuch = false;
    bool m_swipeTriggered = false;
    bool m_pinchTriggered = false;
    bool m_gestureConsumed = false;
    qreal m_initialPinchSpan = 0.0;
    qint64 m_pinchStartMs = 0;

    bool m_eyedropperActive = false;
    QPointF m_eyedropperPos;
    QTimer m_eyedropperTimer;
    QPointer<BrocreateColorLoupe> m_loupe;
};

QHash<const KisInputManager *, BrocreateGestureHandler *> &gestureHandlerRegistry()
{
    static QHash<const KisInputManager *, BrocreateGestureHandler *> registry;
    return registry;
}

BrocreateGestureHandler *gestureHandlerFor(const KisInputManager *manager)
{
    return gestureHandlerRegistry().value(manager, nullptr);
}

} // namespace


template <typename T>
uint qHash(QPointer<T> value) {
    return reinterpret_cast<quintptr>(value.data());
}

KisInputManager::KisInputManager(QObject *parent, KoToolManager *toolManager)
    : QObject(parent), d(new Private(this, toolManager))
{
    d->setupActions();

    BrocreateGestureHandler *handler = new BrocreateGestureHandler(this);
    gestureHandlerRegistry().insert(this, handler);
    d->moveEventCompressor.setDelay(handler->batchDelayMs());

    connect(KisConfigNotifier::instance(), SIGNAL(configChanged()), SLOT(slotConfigChanged()));
    slotConfigChanged();

    connect(d->toolManager, SIGNAL(aboutToChangeTool(KoCanvasController*)), SLOT(slotAboutToChangeTool()));
    connect(d->toolManager, SIGNAL(changedTool(KoCanvasController*)), SLOT(slotToolChanged()));
    connect(d->toolManager, SIGNAL(textModeChanged(bool)), SLOT(slotTextModeChanged()));
    connect(&d->moveEventCompressor, SIGNAL(timeout()), SLOT(slotCompressedMoveEvent()));


    QApplication::instance()->
            installEventFilter(new Private::ProximityNotifier(d, this));

    // on macos global Monitor listen to keypresses when krita is not in focus
    // and local monitor listen presses when krita is in focus.
#ifdef Q_OS_MACOS
    KisExtendedModifiersMapper::setLocalMonitor(true, &d->matcher);
#endif
}

KisInputManager::~KisInputManager()
{
    gestureHandlerRegistry().remove(this);
#ifdef Q_OS_MACOS
    KisExtendedModifiersMapper::setLocalMonitor(false);
#endif
    delete d;
}

void KisInputManager::addTrackedCanvas(KisCanvas2 *canvas)
{
    d->canvasSwitcher.addCanvas(canvas);
}

void KisInputManager::removeTrackedCanvas(KisCanvas2 *canvas)
{
    d->canvasSwitcher.removeCanvas(canvas);
}

void KisInputManager::registerPopupWidget(KisPopupWidgetInterface *popupWidget)
{
    d->popupWidget = popupWidget;

    // FUNKY!
    auto popupObject = dynamic_cast<QObject*>(d->popupWidget);
    KIS_ASSERT(popupObject);
    connect(popupObject, SIGNAL(finished()), this, SLOT(deregisterPopupWidget()));
}

void KisInputManager::deregisterPopupWidget()
{
    if (d->popupWidget->onScreen()) {
        d->popupWidget->dismiss();
    }

    // FUNKY!
    auto popupObject = dynamic_cast<QObject*>(d->popupWidget);
    KIS_ASSERT(popupObject);
    disconnect(popupObject, nullptr, this, nullptr); // Disconnect all.

    d->popupWidget = nullptr;
}

void KisInputManager::slotConfigChanged()
{
#ifdef Q_OS_WIN
    d->ignoreHighFunctionKeys = KisConfig(true).ignoreHighFunctionKeys();
    d->fixShortcutMatcherModifiersState();
#endif
    if (BrocreateGestureHandler *handler = gestureHandlerFor(this)) {
        d->moveEventCompressor.setDelay(handler->batchDelayMs());
    }
}

void KisInputManager::toggleTabletLogger()
{
    KisTabletDebugger::instance()->toggleDebugging();
}

void KisInputManager::attachPriorityEventFilter(QObject *filter, int priority)
{
    Private::PriorityList::iterator begin = d->priorityEventFilter.begin();
    Private::PriorityList::iterator it = begin;
    Private::PriorityList::iterator end = d->priorityEventFilter.end();

    it = std::find_if(begin, end,
                      kismpl::mem_equal_to(&Private::PriorityPair::second, filter));

    if (it != end) return;

    it = std::find_if(begin, end,
                      kismpl::mem_greater(&Private::PriorityPair::first, priority));

    d->priorityEventFilter.insert(it, qMakePair(priority, filter));
    d->priorityEventFilterSeqNo++;
}

void KisInputManager::detachPriorityEventFilter(QObject *filter)
{
    Private::PriorityList::iterator it = d->priorityEventFilter.begin();
    Private::PriorityList::iterator end = d->priorityEventFilter.end();

    it = std::find_if(it, end,
                      kismpl::mem_equal_to(&Private::PriorityPair::second, filter));

    if (it != end) {
        d->priorityEventFilter.erase(it);
    }
}

void KisInputManager::setupAsEventFilter(QObject *receiver)
{
    if (d->eventsReceiver) {
        d->eventsReceiver->removeEventFilter(this);
    }

    d->eventsReceiver = receiver;

    if (d->eventsReceiver) {
        d->eventsReceiver->installEventFilter(this);
    }
}

#if defined (__clang__)
#pragma GCC diagnostic ignored "-Wswitch"
#endif

bool KisInputManager::eventFilter(QObject* object, QEvent* event)
{
    if (object != d->eventsReceiver) return false;

    if (d->eventEater.eventFilter(object, event)) return false;

    if (!d->matcher.hasRunningShortcut()) {

        int savedPriorityEventFilterSeqNo = d->priorityEventFilterSeqNo;

        for (auto it = d->priorityEventFilter.begin(); it != d->priorityEventFilter.end(); /*noop*/) {
            const QPointer<QObject> &filter = it->second;

            if (filter.isNull()) {
                it = d->priorityEventFilter.erase(it);

                d->priorityEventFilterSeqNo++;
                savedPriorityEventFilterSeqNo++;
                continue;
            }

            if (filter->eventFilter(object, event)) return true;

            /**
             * If the filter removed itself from the filters list or
             * added something there, just exit the loop
             */
            if (d->priorityEventFilterSeqNo != savedPriorityEventFilterSeqNo) {
                return true;
            }

            ++it;
        }

        // KoToolProxy needs to pre-process some events to ensure the
        // global shortcuts (not the input manager's ones) are not
        // executed, in particular, this line will accept events when the
        // tool is in text editing, preventing shortcut triggering
        if (d->toolProxy) {
            d->toolProxy->processEvent(event);
        }
    }

    // Continue with the actual switch statement...
    return eventFilterImpl(event);
}

template <class Event>
bool KisInputManager::compressMoveEventCommon(Event *event)
{
    /**
     * We construct a copy of this event object, so we must ensure it
     * has a correct type.
     */
    static_assert(std::is_same<Event, QMouseEvent>::value ||
                  std::is_same<Event, QTabletEvent>::value,
                  "event should be a mouse or a tablet event");


    bool retval = false;
    BrocreateGestureHandler *handler = gestureHandlerFor(this);
    const bool isMoveEvent =
        (event->type() == QEvent::MouseMove || event->type() == QEvent::TabletMove);

    const bool shouldBatchFor144Hz =
        isMoveEvent && handler && handler->shouldBatchHiResMove(event->pos());

    /**
     * Compress the events if the tool doesn't need high resolution input
     * or when sub-frame events arrive faster than the 144Hz render cadence.
     */
    if (isMoveEvent &&
            (!d->matcher.supportsHiResInputEvents() ||
             d->testingCompressBrushEvents ||
             shouldBatchFor144Hz)) {
#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
        KoPointerEvent::copyQtPointerEvent(event, d->compressedMoveEvent);
#else
        d->compressedMoveEvent.reset(event->clone());
#endif
        if (handler) {
            d->moveEventCompressor.setDelay(handler->batchDelayMs());
        }
        d->moveEventCompressor.start();

        /**
         * On Linux Qt eats the rest of unneeded events if we
         * ignore the first of the chunk of tablet events. So
         * generally we should never activate this feature. Only
         * for testing purposes!
         */
        if (d->testingAcceptCompressedTabletEvents) {
            event->setAccepted(true);
        }

        retval = true;
    } else {
        slotCompressedMoveEvent();
        retval = d->handleCompressedTabletEvent(event);
    }

    return retval;
}

bool shouldResetWheelDelta(QEvent * event)
{
    return
        event->type() == QEvent::FocusIn ||
        event->type() == QEvent::FocusOut ||
        event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::MouseButtonDblClick ||
        event->type() == QEvent::TabletPress ||
        event->type() == QEvent::TabletRelease ||
        event->type() == QEvent::Enter ||
        event->type() == QEvent::Leave ||
        event->type() == QEvent::TouchBegin ||
        event->type() == QEvent::TouchEnd ||
        event->type() == QEvent::TouchCancel ||
        event->type() == QEvent::NativeGesture;

}

bool KisInputManager::eventFilterImpl(QEvent * event)
{
    bool retval = false;
    BrocreateGestureHandler *handler = gestureHandlerFor(this);

    // Try closing any open popup widget and
    // consume input if possible.
    if (d->popupWidget) {
        QEvent::Type type = event->type();

        if (type == QEvent::MouseButtonPress
         || type == QEvent::MouseButtonDblClick
         || type == QEvent::TabletPress
         || type == QEvent::TouchBegin
         || type == QEvent::NativeGesture) {
            bool wasVisible = d->popupWidget->onScreen();
            deregisterPopupWidget();

            if (wasVisible) {
                d->popupWasActive = true;
                event->setAccepted(true);
                return true; // Event consumed.
            }
        }
    }

    if (shouldResetWheelDelta(event)) {
        d->accumulatedScrollDelta = {};
    }

    if (event->type() == QEvent::MouseMove ||
        event->type() == QEvent::MouseButtonPress ||
        event->type() == QEvent::MouseButtonRelease ||
        event->type() == QEvent::TabletMove ||
        event->type() == QEvent::TabletPress ||
        event->type() == QEvent::TabletRelease ||
        event->type() == QEvent::Wheel) {

        /**
         * When Krita (as an application) has no input focus, we cannot
         * handle key events. But at the same time, when the user hovers
         * Krita canvas, we should still show him the correct cursor.
         *
         * So here we just add a simple workaround to resync shortcut
         * matcher's state at least against the basic modifiers, like
         * Shift, Control and Alt.
         */
        QWidget *receivingWidget = dynamic_cast<QWidget*>(d->eventsReceiver);
        if (receivingWidget && !receivingWidget->hasFocus()) {
            d->fixShortcutMatcherModifiersState();
        } else {
            /**
             * On Windows, when the user presses some global window manager shortcuts,
             * e.g. Alt+Space (to show window title menu), events for these key presses
             * and releases are not delivered (see bug 424319). This code is a workaround
             * for this problem. It checks consistency of standard modifiers and resets
             * shortcut's matcher state in case of a trouble.
             */
            QInputEvent *inputEvent = static_cast<QInputEvent*>(event);
            if (event->type() != QEvent::ShortcutOverride &&
                !d->matcher.sanityCheckModifiersCorrectness(inputEvent->modifiers())) {

                d->fixShortcutMatcherModifiersState();
            } else if (d->matcher.hasPolledKeys()) {
                /**
                 * Re-check the native platform key API against keys we are unsure about,
                 * and fix them in case they now show as released.
                 *
                 * The other part of the fix is placed in the handler of ShortcutOverride,
                 * because it needs a custom set of the presset keys.
                 */
                d->fixShortcutMatcherModifiersState();
            }
        }
    }

    switch (event->type()) {
    case QEvent::MouseButtonPress:
    case QEvent::MouseButtonDblClick: {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);

        // Palm rejection: never let touch-synthesized mouse events draw on the canvas
        if (mouseEvent->source() != Qt::MouseEventNotSynthesized) {
            event->setAccepted(true);
            return true;
        }

        if (d->popupWidget) {
            retval = true;
        } else {
            //Make sure the input actions know we are active.
            KisAbstractInputAction::setInputManager(this);
            retval = d->matcher.buttonPressed(mouseEvent->button(), mouseEvent);
        }
        //Reset signal compressor to prevent processing events before press late
        d->resetCompressor();
        if (handler) {
            handler->resetHiResBatching();
        }
        event->setAccepted(retval);
        break;
    }
    case QEvent::MouseButtonRelease: {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->source() != Qt::MouseEventNotSynthesized) {
            event->setAccepted(true);
            return true;
        }
        slotCompressedMoveEvent();
        retval = d->matcher.buttonReleased(mouseEvent->button(), mouseEvent);
        event->setAccepted(retval);
        break;
    }
    case QEvent::ShortcutOverride: {
        KisInputEventsEater::debugEvent<QKeyEvent>(event);
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        Qt::Key key = KisExtendedModifiersMapper::workaroundShiftAltMetaHell(keyEvent);

        /** See a comment in the handler of KeyRelease event for
         * shouldSynchronizeOnNextKeyPress explanation
         *
         * There is also a case when Krita gets focus via Win+1 key, then
         * the polled key '1' gets into the matcher, but OS does not deliver any
         * signals for it (see bug 451424)
         */
        if (d->shouldSynchronizeOnNextKeyPress || d->matcher.hasPolledKeys()) {
            QVector<Qt::Key> guessedKeys;
            KisExtendedModifiersMapper mapper;
            Qt::KeyboardModifiers modifiers = mapper.queryStandardModifiers();
            Q_FOREACH (Qt::Key key, mapper.queryExtendedModifiers()) {
                QKeyEvent kevent(QEvent::ShortcutOverride, key, modifiers);
                guessedKeys << KisExtendedModifiersMapper::workaroundShiftAltMetaHell(&kevent);
            }

            if (!d->matcher.debugPressedKeys().contains(key)) {
                guessedKeys.removeOne(key);
            }

            d->fixShortcutMatcherModifiersState(guessedKeys, modifiers);
            d->shouldSynchronizeOnNextKeyPress = false;
        }

        if (!keyEvent->isAutoRepeat()) {
            retval = d->matcher.keyPressed(key);
        } else {
            retval = d->matcher.autoRepeatedKeyPressed(key);
        }

        // In case we matched a shortcut we should accept the event to
        // notify Qt that it shouldn't try to trigger its partially matched
        // shortcuts.
        if (retval) {
            keyEvent->setAccepted(true);
        }

        break;
    }
    case QEvent::KeyRelease: {
        KisInputEventsEater::debugEvent<QKeyEvent>(event);
        QKeyEvent *keyEvent = static_cast<QKeyEvent*>(event);

        if (!keyEvent->isAutoRepeat()) {
            Qt::Key key = KisExtendedModifiersMapper::workaroundShiftAltMetaHell(keyEvent);
            retval = d->matcher.keyReleased(key);

            /**
             * On some systems Qt fails to generate a correct
             * sequence of events when the user releases a
             * modifier key while some other key is pressed.
             *
             * In such cases Qt doesn't understand that the key
             * has changed its name in the meantime and sends
             * incorrect key-release event for it (or an auto-
             * repeated key-release/key-press pair).
             *
             * Example (on en-US keyboard):
             *
             * 1) Press Shift (Key_Shift-press is delivered)
             * 2) Press '2' (Key_At-press is delivered)
             * 3) Release Shift (Key_Shift-release is delivered)
             * 4) Release '2' (Key_2-release is delivered,
             *                 which is unbalanced)
             *
             * The same issue happens with non-latin keyboards,
             * where Qt does auto-key-replace routines when
             * Control modifier is pressed.
             *
             * https://bugs.kde.org/show_bug.cgi?id=454256
             * https://bugreports.qt.io/browse/QTBUG-103868
             */

            if (d->useUnbalancedKeyPressEventWorkaround &&
                !d->matcher.debugPressedKeys().isEmpty() &&
                (key == Qt::Key_Shift || key == Qt::Key_Alt ||
                 key == Qt::Key_Control || key == Qt::Key_Meta)) {

                d->shouldSynchronizeOnNextKeyPress = true;
            }
        }
        break;
    }
    case QEvent::MouseMove: {
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (mouseEvent->source() != Qt::MouseEventNotSynthesized) {
            event->setAccepted(true);
            return true;
        }
        retval = compressMoveEventCommon(mouseEvent);

        break;
    }
    case QEvent::Wheel: {
        KisInputEventsEater::debugEvent<QWheelEvent>(event);
        QWheelEvent *wheelEvent = static_cast<QWheelEvent*>(event);

        if (wheelEvent->phase() != Qt::NoScrollPhase && d->matcher.hasTouchpadScrollShortcuts()) {
            /**
             * Wayland sends us a duplicated ScrollBegin event (one for
             * every axis), so we cannot rely just on status of the
             * running stroke and should use hasTouchpadScrollShortcuts()
             * instead.
             */

            if (wheelEvent->phase() == Qt::ScrollBegin) {
                d->matcher.touchpadScrollBeginEvent(wheelEvent);
            } else if (wheelEvent->phase() == Qt::ScrollEnd) {
                d->matcher.touchpadScrollEndEvent(wheelEvent);
            } else {
                 d->matcher.touchpadScrollEvent(wheelEvent);
            }

            break;
        }

        d->accumulatedScrollDelta += wheelEvent->angleDelta();

        /**
         * When wheelEvent->phase() is not Qt::NoScrollPhase, the delta
         * can be easily null, so we should ignore these events.
         */
        if (wheelEvent->angleDelta().isNull()) {
            retval = true;
            break;
        }

        bool wasScrolled = false;

        {
            KisSingleActionShortcut::WheelAction yAction;

            if (wheelEvent->angleDelta().y() < 0) {
                yAction = KisSingleActionShortcut::WheelDown;
            } else if (wheelEvent->angleDelta().y() > 0) {
                yAction = KisSingleActionShortcut::WheelUp;
            }

            while (qAbs(d->accumulatedScrollDelta.y()) >= QWheelEvent::DefaultDeltasPerStep) {
                // Make sure the input actions know we are active.
                KisAbstractInputAction::setInputManager(this);
                retval = d->matcher.wheelEvent(yAction, wheelEvent);
                d->accumulatedScrollDelta.ry() -=
                    KisAlgebra2D::signPZ(d->accumulatedScrollDelta.y()) * QWheelEvent::DefaultDeltasPerStep;
                wasScrolled = true;
            }
        }

        {
            KisSingleActionShortcut::WheelAction xAction;

            if (wheelEvent->angleDelta().x() < 0) {
                xAction = KisSingleActionShortcut::WheelRight;
            } else if (wheelEvent->angleDelta().x() > 0) {
                xAction = KisSingleActionShortcut::WheelLeft;
            }

            while (qAbs(d->accumulatedScrollDelta.x()) >= QWheelEvent::DefaultDeltasPerStep) {
                // Make sure the input actions know we are active.
                KisAbstractInputAction::setInputManager(this);
                retval = d->matcher.wheelEvent(xAction, wheelEvent);
                d->accumulatedScrollDelta.rx() -=
                    KisAlgebra2D::signPZ(d->accumulatedScrollDelta.x()) * QWheelEvent::DefaultDeltasPerStep;
                wasScrolled = true;
            }
        }

        retval = !wasScrolled;
        break;
    }
#ifndef Q_OS_ANDROID
    case QEvent::Enter:
        KisInputEventsEater::debugEvent<QEvent>(event);
        //Make sure the input actions know we are active.
        KisAbstractInputAction::setInputManager(this);
        d->matcher.enterEvent();
        break;
    case QEvent::Leave:
        KisInputEventsEater::debugEvent<QEvent>(event);
        d->matcher.leaveEvent();
        break;
#endif
    case QEvent::FocusIn:
        KisInputEventsEater::debugEvent<QEvent>(event);
        KisAbstractInputAction::setInputManager(this);

        d->fixShortcutMatcherModifiersState();
        d->matcher.reinitializeButtons();
        break;

    case QEvent::FocusOut: {
        KisInputEventsEater::debugEvent<QEvent>(event);
        KisAbstractInputAction::setInputManager(this);

        QPointF currentLocalPos =
                canvas()->canvasWidget()->mapFromGlobal(QCursor::pos());

        d->matcher.lostFocusEvent(currentLocalPos);

        break;
    }
    case QEvent::TabletPress: {
        QTabletEvent *tabletEvent = static_cast<QTabletEvent*>(event);
        if (handler) {
            handler->onTabletPress();
            handler->resetHiResBatching();
        }

        {
            //Make sure the input actions know we are active.
            KisAbstractInputAction::setInputManager(this);
            retval = d->matcher.buttonPressed(tabletEvent->button(), tabletEvent);
        }

        event->setAccepted(true);
        retval = true;

        //Reset signal compressor to prevent processing events before press late
        d->resetCompressor();

#if defined Q_OS_LINUX && !KRITA_QT_HAS_ENTER_LEAVE_PATCH
        // remove this hack when this patch is integrated:
        // https://codereview.qt-project.org/#/c/255384/
        event->setAccepted(false);
        d->eventEater.eatOneMousePress();
#elif defined Q_OS_WIN32
        /**
         * Windows is the only platform that synthesizes mouse events for
         * the tablet on OS-level, that is, even when we accept the event
         */
        d->eventEater.eatOneMousePress();
#endif

        break;
    }
    case QEvent::TabletMove: {
        QTabletEvent *tabletEvent = static_cast<QTabletEvent*>(event);
        if (handler) {
            handler->onTabletMove();
        }
        retval = compressMoveEventCommon(tabletEvent);

        if (d->tabletLatencyTracker) {
            d->tabletLatencyTracker->push(tabletEvent->timestamp());
        }

#if defined Q_OS_LINUX && !KRITA_QT_HAS_ENTER_LEAVE_PATCH
        // remove this hack when this patch is integrated:
        // https://codereview.qt-project.org/#/c/255384/
        event->setAccepted(false);
#endif

        break;
    }
    case QEvent::TabletRelease: {
        QTabletEvent *tabletEvent = static_cast<QTabletEvent*>(event);
        slotCompressedMoveEvent();
        if (handler) {
            handler->onTabletRelease();
        }
        retval = d->matcher.buttonReleased(tabletEvent->button(), tabletEvent);
        retval = true;
        event->setAccepted(true);

#if defined Q_OS_LINUX && !KRITA_QT_HAS_ENTER_LEAVE_PATCH
        // remove this hack when this patch is integrated:
        // https://codereview.qt-project.org/#/c/255384/
        event->setAccepted(false);
#endif

        break;
    }

    case QEvent::TouchBegin:
    {
        // The popup was dismissed in previous TouchBegin->TouchEnd sequence. We now have a new TouchBegin.
        d->popupWasActive = false;
        QTouchEvent *touchEvent = static_cast<QTouchEvent *>(event);
        KisAbstractInputAction::setInputManager(this);

        if (handler && handler->handleTouchBegin(touchEvent, d->matcher)) {
            event->accept();
            return true;
        }

        retval = d->matcher.touchBeginEvent(touchEvent);

        d->resetCompressor();
        // we accept touch events unconditionally to disable
        // system-wide gesture on the canvas
        event->accept();
        break;
    }

    case QEvent::TouchUpdate:
    {
        if (d->popupWasActive) {
            event->setAccepted(true);
            return true;
        }
        QTouchEvent *touchEvent = static_cast<QTouchEvent*>(event);

        if (handler && handler->handleTouchUpdate(touchEvent, d->matcher)) {
            event->accept();
            return true;
        }

#ifdef Q_OS_MAC
        const int pressedCount = KisTouchShortcut::countTouchPoints(touchEvent, KisTouchShortcut::pressedOnlyTouchStates());
        const int totalPointCount = KisTouchShortcut::countTouchPoints(touchEvent, KisTouchShortcut::allTouchStates());

        if (pressedCount < 2 && totalPointCount > pressedCount) {
            d->matcher.touchEndEvent(touchEvent);
            retval = true;
        } else {
#endif
            KisAbstractInputAction::setInputManager(this);
            d->matcher.touchUpdateEvent(touchEvent);
            /**
             * We don't try to pass unhandled events to the tool proxy
             * (via toolProxy->forwardHoverEvent(event)) because of
             * the following reasons:
             *
             * On Windows and Linux touch evetns are generated for
             * touch screens only. And touch screens have no "hover"
             * state.
             *
             * On MacOS touch events are also generated for the Apple's
             * trackpad. Though, for single-finger movements, these touch
             * events are usually unbalanced, i.e. they don't have a
             * corresponding TouchBegin event.
             *
             * We could theoretically pass these events to the tool on
             * MacOS only, but it seems like the system-generated mouse
             * events are more precise and granular. That is why we just
             * allow these mouse events in EventEater when we detect they
             * are coming from a touchpad/trackpad outside of
             * TouchBegin/TouchEnd guards.
             */
            retval = true;

#ifdef Q_OS_MACOS
        }
#endif

        // we accept touch events unconditionally to disable
        // system-wide gesture on the canvas
        event->accept();
        break;
    }

    case QEvent::TouchEnd:
    {
        if (d->popupWasActive) {
            event->setAccepted(true);
            return true;
        }
        QTouchEvent *touchEvent = static_cast<QTouchEvent*>(event);

        if (handler && handler->handleTouchEnd(touchEvent, d->matcher)) {
            event->accept();
            return true;
        }

        retval = d->matcher.touchEndEvent(touchEvent);

        // we accept touch events unconditionally to disable
        // system-wide gesture on the canvas
        event->accept();
        break;
    }
    case QEvent::TouchCancel:
    {
        if (handler) {
            handler->handleTouchCancel();
        }
        if (d->popupWasActive) {
            event->setAccepted(true);
            return true;
        }
        QTouchEvent *touchEvent = static_cast<QTouchEvent*>(event);
        d->matcher.touchCancelEvent(touchEvent);

        retval = true;

        // we accept tablet events unconditionally to disable
        // system-wide gesture on the canvas
        event->accept();
        break;
    }

    case QEvent::NativeGesture:
    {
        QNativeGestureEvent *gevent = static_cast<QNativeGestureEvent*>(event);
        switch (gevent->gestureType()) {
            case Qt::BeginNativeGesture:
            {
                KisAbstractInputAction::setInputManager(this);
                retval = d->matcher.nativeGestureBeginEvent(gevent);
                event->accept();
                break;
            }
            case Qt::EndNativeGesture:
            {
                retval = d->matcher.nativeGestureEndEvent(gevent);
                event->accept();
                break;
            }
            default:
            {
                KisAbstractInputAction::setInputManager(this);
                retval = d->matcher.nativeGestureEvent(gevent);
                event->accept();
                break;
            }
        }
        retval = true;
        event->accept();
        break;
    }

    default:
        break;
    }

    return !retval ? d->processUnhandledEvent(event) : true;
}

void KisInputManager::slotCompressedMoveEvent()
{
    if (d->compressedMoveEvent) {
        (void) d->handleCompressedTabletEvent(d->compressedMoveEvent.data());
        d->compressedMoveEvent.reset();
    }
}

KisCanvas2* KisInputManager::canvas() const
{
    return d->canvas;
}

QPointer<KisToolProxy> KisInputManager::toolProxy() const
{
    return d->toolProxy;
}

KisToolInvocationAction *KisInputManager::defaultInputAction() const
{
    return d->defaultInputAction;
}

void KisInputManager::slotAboutToChangeTool()
{
    QPointF currentLocalPos;
    if (canvas() && canvas()->canvasWidget()) {
        currentLocalPos = canvas()->canvasWidget()->mapFromGlobal(QCursor::pos());
    }
    // once more, don't forget the global state whenever matcher may trigger a KisAbstractInputAction
    KisAbstractInputAction::setInputManager(this);

    d->matcher.lostFocusEvent(currentLocalPos);
}

void KisInputManager::slotToolChanged()
{
    if (!d->canvas) return;
    KoToolBase *tool = d->toolManager->toolById(canvas(), d->toolManager->activeToolId());
    if (tool) {
        // once more, don't forget the global state whenever matcher may trigger a KisAbstractInputAction
        KisAbstractInputAction::setInputManager(this);

        d->setMaskSyntheticEvents(tool->maskSyntheticEvents());
        if (tool->isInTextMode()) {
            d->forwardAllEventsToTool = true;
            d->matcher.suppressAllKeyboardActions(true);
        } else {
            d->forwardAllEventsToTool = false;
            d->matcher.suppressAllKeyboardActions(false);
        }

        d->matcher.suppressConflictingKeyActions(toolProxy()->toolPriorityShortcuts());
        d->matcher.toolHasBeenActivated();
    }
}

void KisInputManager::slotTextModeChanged()
{
    if (!d->canvas) return;
    KoToolBase *tool = d->toolManager->toolById(canvas(), d->toolManager->activeToolId());
    if (tool) {
        if (tool->isInTextMode()) {
            d->forwardAllEventsToTool = true;
            d->matcher.suppressAllKeyboardActions(true);
        } else {
            d->forwardAllEventsToTool = false;
            d->matcher.suppressAllKeyboardActions(false);
        }
    }
}

void KisInputManager::profileChanged()
{
    d->matcher.clearShortcuts();

    KisInputProfile *profile = KisInputProfileManager::instance()->currentProfile();
    if (profile) {
        const QList<KisShortcutConfiguration*> shortcuts = profile->allShortcuts();

        for (KisShortcutConfiguration * const shortcut : shortcuts) {
            // Brocreate palm rejection: fingers are strictly for gestures, stylus is for drawing
            if (shortcut->type() == KisShortcutConfiguration::TouchGestureType &&
                shortcut->action() &&
                shortcut->action()->id() == QStringLiteral("Tool Invocation") &&
                (shortcut->touchGesture() == KisShortcutConfiguration::OneFingerDrag ||
                 shortcut->touchGesture() == KisShortcutConfiguration::OneFingerTap)) {
                continue;
            }

            dbgUI << "Adding shortcut" << shortcut->keys() << "for action" << shortcut->action()->name();
            switch(shortcut->type()) {
            case KisShortcutConfiguration::KeyCombinationType:
                d->addKeyShortcut(shortcut->action(), shortcut->mode(), shortcut->keys());
                break;
            case KisShortcutConfiguration::MouseButtonType:
                d->addStrokeShortcut(shortcut->action(), shortcut->mode(), shortcut->keys(), shortcut->buttons());
                break;
            case KisShortcutConfiguration::MouseWheelType:
                d->addWheelShortcut(shortcut->action(), shortcut->mode(), shortcut->keys(), shortcut->wheel());
                break;
            case KisShortcutConfiguration::TouchGestureType:
                d->addTouchShortcut(shortcut->action(), shortcut->mode(), shortcut->touchGesture());
                break;
            case KisShortcutConfiguration::NativeGestureType:
                d->addNativeGestureShortcut(shortcut->action(), shortcut->mode(), shortcut->nativeGesture());
                break;
            default:
                break;
            }
        }
        // Intentionally do not bind OneFingerDrag / OneFingerTap to Tool Invocation:
        // stylus draws exclusively while fingers perform Procreate gestures.
    }
    else {
        warnUI << "No Input Profile Found: canvas interaction will be impossible";
    }
}
