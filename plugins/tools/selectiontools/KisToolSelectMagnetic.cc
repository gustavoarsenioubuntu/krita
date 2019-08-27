/*
 *  Copyright (c) 2019 Kuntal Majumder <hellozee@disroot.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "KisToolSelectMagnetic.h"

#include <QApplication>
#include <QPainter>
#include <QWidget>
#include <QPainterPath>
#include <QLayout>
#include <QVBoxLayout>

#include <kis_debug.h>
#include <klocalizedstring.h>

#include <KoPointerEvent.h>
#include <KoShapeController.h>
#include <KoPathShape.h>
#include <KoColorSpace.h>
#include <KoCompositeOp.h>
#include <KoViewConverter.h>

#include <kis_layer.h>
#include <kis_selection_options.h>
#include <kis_cursor.h>
#include <kis_image.h>

#include "kis_painter.h"
#include <brushengine/kis_paintop_registry.h>
#include "canvas/kis_canvas2.h"
#include "kis_pixel_selection.h"
#include "kis_selection_tool_helper.h"

#include "kis_algebra_2d.h"

#include "KisHandlePainterHelper.h"

#include <kis_slider_spin_box.h>

#define FEEDBACK_LINE_WIDTH 2

KisToolSelectMagnetic::KisToolSelectMagnetic(KoCanvasBase *canvas)
    : KisToolSelect(canvas,
                    KisCursor::load("tool_magnetic_selection_cursor.png", 5, 5),
                    i18n("Magnetic Selection")),
    m_continuedMode(false), m_complete(true), m_threshold(70), m_checkPoint(-1), m_frequency(30), m_radius(3.0)
{ }

void KisToolSelectMagnetic::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control) {
        m_continuedMode = true;
    }

    KisToolSelect::keyPressEvent(event);
}

void KisToolSelectMagnetic::keyReleaseEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Control ||
        !(event->modifiers() & Qt::ControlModifier))
    {
        m_continuedMode = false;
        if (mode() != PAINT_MODE && !m_points.isEmpty()) {
            finishSelectionAction();
        }
    }

    KisToolSelect::keyReleaseEvent(event);
}

// the cursor is still tracked even when no mousebutton is pressed
void KisToolSelectMagnetic::mouseMoveEvent(KoPointerEvent *event)
{
    KisToolSelect::mouseMoveEvent(event);
    m_paintPath = QPainterPath();
    m_paintPath.moveTo(pixelToView(m_points[0]));

    for (int i = 1; i < m_points.count(); i++) {
        m_paintPath.lineTo(pixelToView(m_points[i]));
    }

    updateFeedback();

    if (m_continuedMode && mode() != PAINT_MODE) {
        updateContinuedMode();
    }
} // KisToolSelectMagnetic::mouseMoveEvent

// press primary mouse button
void KisToolSelectMagnetic::beginPrimaryAction(KoPointerEvent *event)
{
    setMode(KisTool::PAINT_MODE);
    QPointF temp(convertToPixelCoord(event));

    if(m_points.count() != 0){
        vQPointF edge = m_worker.computeEdge(m_frequency, m_anchorPoints.last(), temp.toPoint());
        m_points.append(edge);
        if(m_snapBound.contains(temp.toPoint())){
            finishSelectionAction();
            return;
        }
    } else {
        m_points.push_back(temp.toPoint());
        m_snapBound = QRectF(temp.toPoint(), QSize(5,5));
    }

    m_lastAnchor = temp.toPoint();
    m_anchorPoints.push_back(m_lastAnchor);
    updateCanvasPixelRect(image()->bounds());
    m_complete = false;
}

// drag while primary mouse button is pressed
void KisToolSelectMagnetic::continuePrimaryAction(KoPointerEvent *event)
{
    KisToolSelectBase::continuePrimaryAction(event);
}

// release primary mouse button
void KisToolSelectMagnetic::endPrimaryAction(KoPointerEvent *event)
{
    KisToolSelectBase::endPrimaryAction(event);
}

void KisToolSelectMagnetic::finishSelectionAction()
{
    KisCanvas2 *kisCanvas = dynamic_cast<KisCanvas2 *>(canvas());
    KIS_ASSERT_RECOVER_RETURN(kisCanvas);
    kisCanvas->updateCanvas();
    setMode(KisTool::HOVER_MODE);
    m_complete = true;

    // just for testing out
    //m_worker.saveTheImage(m_points);

    QRectF boundingViewRect =
        pixelToView(KisAlgebra2D::accumulateBounds(m_points));

    KisSelectionToolHelper helper(kisCanvas, kundo2_i18n("Magnetic Selection"));

    if (m_points.count() > 2 &&
        !helper.tryDeselectCurrentSelection(boundingViewRect, selectionAction()))
    {
        QApplication::setOverrideCursor(KisCursor::waitCursor());

        const SelectionMode mode =
            helper.tryOverrideSelectionMode(kisCanvas->viewManager()->selection(),
                                            selectionMode(),
                                            selectionAction());
        if (mode == PIXEL_SELECTION) {
            KisPixelSelectionSP tmpSel = KisPixelSelectionSP(new KisPixelSelection());

            KisPainter painter(tmpSel);
            painter.setPaintColor(KoColor(Qt::black, tmpSel->colorSpace()));
            painter.setAntiAliasPolygonFill(antiAliasSelection());
            painter.setFillStyle(KisPainter::FillStyleForegroundColor);
            painter.setStrokeStyle(KisPainter::StrokeStyleNone);

            painter.paintPolygon(m_points);

            QPainterPath cache;
            cache.addPolygon(m_points);
            cache.closeSubpath();
            tmpSel->setOutlineCache(cache);

            helper.selectPixelSelection(tmpSel, selectionAction());
        } else {
            KoPathShape *path = new KoPathShape();
            path->setShapeId(KoPathShapeId);

            QTransform resolutionMatrix;
            resolutionMatrix.scale(1 / currentImage()->xRes(), 1 / currentImage()->yRes());
            path->moveTo(resolutionMatrix.map(m_points[0]));
            for (int i = 1; i < m_points.count(); i++)
                path->lineTo(resolutionMatrix.map(m_points[i]));
            path->close();
            path->normalize();
            helper.addSelectionShape(path, selectionAction());
        }
        QApplication::restoreOverrideCursor();
    }
    m_points.clear();
    m_anchorPoints.clear();
    m_paintPath = QPainterPath();
} // KisToolSelectMagnetic::finishSelectionAction

void KisToolSelectMagnetic::paint(QPainter& gc, const KoViewConverter &converter)
{
    Q_UNUSED(converter);

    if ((mode() == KisTool::PAINT_MODE || m_continuedMode) &&
        !m_points.isEmpty())
    {
        QPainterPath outline = m_paintPath;
        if (m_continuedMode && mode() != KisTool::PAINT_MODE) {
            outline.lineTo(pixelToView(m_lastCursorPos));
        }
        paintToolOutline(&gc, outline);
        Q_FOREACH (const QPoint pt, m_anchorPoints) {
            KisHandlePainterHelper helper(&gc, handleRadius());
            helper.setHandleStyle(KisHandleStyle::primarySelection());
            helper.drawHandleRect(pixelToView(pt), 4, QPoint(0, 0));
        }
    }
}

void KisToolSelectMagnetic::updateFeedback()
{
    if (m_points.count() > 1) {
        qint32 lastPointIndex = m_points.count() - 1;

        QRectF updateRect = QRectF(m_points[lastPointIndex - 1], m_points[lastPointIndex]).normalized();
        updateRect = kisGrowRect(updateRect, FEEDBACK_LINE_WIDTH);

        updateCanvasPixelRect(updateRect);
    }
}

void KisToolSelectMagnetic::updateContinuedMode()
{
    if (!m_points.isEmpty()) {
        qint32 lastPointIndex = m_points.count() - 1;

        QRectF updateRect = QRectF(m_points[lastPointIndex - 1], m_lastCursorPos).normalized();
        updateRect = kisGrowRect(updateRect, FEEDBACK_LINE_WIDTH);

        updateCanvasPixelRect(updateRect);
    }
}

void KisToolSelectMagnetic::activate(KoToolBase::ToolActivation activation, const QSet<KoShape *> &shapes)
{
    m_worker      = KisMagneticWorker(image()->projection(), m_radius);
    m_configGroup = KSharedConfig::openConfig()->group(toolId());
    connect(action("undo_polygon_selection"), SIGNAL(triggered()), SLOT(undoPoints()), Qt::UniqueConnection);
    KisToolSelect::activate(activation, shapes);
}

void KisToolSelectMagnetic::deactivate()
{
    KisCanvas2 *kisCanvas = dynamic_cast<KisCanvas2 *>(canvas());
    KIS_ASSERT_RECOVER_RETURN(kisCanvas);
    kisCanvas->updateCanvas();

    m_continuedMode = false;
    m_complete      = true;

    disconnect(action("undo_polygon_selection"), nullptr, this, nullptr);

    KisTool::deactivate();
}

void KisToolSelectMagnetic::undoPoints()
{
}

void KisToolSelectMagnetic::requestStrokeEnd()
{
    if(m_complete) return;
    finishSelectionAction();
}

void KisToolSelectMagnetic::requestStrokeCancellation()
{
}

QWidget * KisToolSelectMagnetic::createOptionWidget()
{
    KisToolSelectBase::createOptionWidget();
    KisSelectionOptions *selectionWidget = selectionOptionWidget();
    QHBoxLayout *f1 = new QHBoxLayout();
    QLabel *lblRad  = new QLabel(i18n("Radius: "), selectionWidget);
    f1->addWidget(lblRad);

    KisDoubleSliderSpinBox *radInput = new KisDoubleSliderSpinBox(selectionWidget);
    radInput->setObjectName("radius");
    radInput->setRange(2.5, 100.0, 2);
    radInput->setSingleStep(0.5);
    radInput->setToolTip("Radius of the filter for the detecting edges, might take some time to calculate");
    f1->addWidget(radInput);
    connect(radInput, SIGNAL(valueChanged(qreal)), this, SLOT(slotSetRadius(qreal)));

    QHBoxLayout *f2      = new QHBoxLayout();
    QLabel *lblThreshold = new QLabel(i18n("Threshold: "), selectionWidget);
    f2->addWidget(lblThreshold);

    KisSliderSpinBox *threshInput = new KisSliderSpinBox(selectionWidget);
    threshInput->setObjectName("threshold");
    threshInput->setRange(1, 255);
    threshInput->setSingleStep(10);
    threshInput->setToolTip("Threshold for determining the minimum intensity of the edges");
    f2->addWidget(threshInput);
    connect(threshInput, SIGNAL(valueChanged(int)), this, SLOT(slotSetThreshold(int)));

    QHBoxLayout *f3     = new QHBoxLayout();
    QLabel *lblFrquency = new QLabel(i18n("Anchor Gap: "), selectionWidget);
    f3->addWidget(lblFrquency);

    KisSliderSpinBox *freqInput = new KisSliderSpinBox(selectionWidget);
    freqInput->setObjectName("frequency");
    freqInput->setRange(20, 200);
    freqInput->setSingleStep(10);
    freqInput->setToolTip("Average distance between 2 anchors in pixels");
    freqInput->setSuffix(" px");
    f3->addWidget(freqInput);
    connect(freqInput, SIGNAL(valueChanged(int)), this, SLOT(slotSetFrequency(int)));

    QVBoxLayout *l = dynamic_cast<QVBoxLayout *>(selectionWidget->layout());
    Q_ASSERT(l);
    l->insertLayout(1, f1);
    l->insertLayout(2, f2);
    l->insertLayout(3, f3);

    radInput->setValue(m_configGroup.readEntry("radius", 3.0));
    threshInput->setValue(m_configGroup.readEntry("threshold", 100));
    freqInput->setValue(m_configGroup.readEntry("frequency", 30));
    return selectionWidget;
} // KisToolSelectMagnetic::createOptionWidget

void KisToolSelectMagnetic::slotSetRadius(qreal r)
{
    m_radius = r;
    m_configGroup.writeEntry("radius", r);
    m_worker = KisMagneticWorker(image()->projection(), m_radius);
}

void KisToolSelectMagnetic::slotSetThreshold(int t)
{
    m_threshold = t;
    m_configGroup.writeEntry("threshold", t);
}

void KisToolSelectMagnetic::slotSetFrequency(int f)
{
    m_frequency = f;
    m_configGroup.writeEntry("frequency", f);
}

void KisToolSelectMagnetic::resetCursorStyle()
{
    if (selectionAction() == SELECTION_ADD) {
        useCursor(KisCursor::load("tool_magnetic_selection_cursor_add.png", 6, 6));
    } else if (selectionAction() == SELECTION_SUBTRACT) {
        useCursor(KisCursor::load("tool_magnetic_selection_cursor_sub.png", 6, 6));
    } else {
        KisToolSelect::resetCursorStyle();
    }
}
