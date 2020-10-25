#include "View.h"
#include "CreatorPopup.h"

#include <QWheelEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QGraphicsItem>
#include <QDebug>

namespace piper
{
    View::View(QWidget* parent)
        : QGraphicsView(parent)
    {
        setFocusPolicy(Qt::ClickFocus);
        setDragMode(QGraphicsView::RubberBandDrag);

        creator_ = new CreatorPopup(this);
    }
    

    void View::wheelEvent(QWheelEvent* event)
    {
        setTransformationAnchor(QGraphicsView::AnchorUnderMouse);

        constexpr qreal inFactor = 1.15;
        constexpr qreal outFactor = 1 / inFactor;

        qreal zoomFactor = outFactor;
        if (event->delta() > 0)
        {
            zoomFactor = inFactor;
        }

        scale(zoomFactor, zoomFactor);
    }


    void View::keyPressEvent(QKeyEvent* event)
    {
        // Keyboard zoom
        setTransformationAnchor(QGraphicsView::AnchorViewCenter);
        constexpr qreal inFactor = 1.15;
        constexpr qreal outFactor = 1 / inFactor;

        if ((event->key() == Qt::Key::Key_Plus) and (event->modifiers() & Qt::ControlModifier))
        {
            scale(inFactor, inFactor);
            event->accept();
        }
        if ((event->key() == Qt::Key::Key_Minus) and (event->modifiers() & Qt::ControlModifier))
        {
            scale(outFactor, outFactor);
            event->accept();
        }
        if (event->key() == Qt::Key::Key_Equal)
        {
            creator_->popup();
            event->accept();
        }
        if (event->key() == Qt::Key::Key_Escape)
        {
            fitInView(scene()->itemsBoundingRect(), Qt::KeepAspectRatio);
            event->accept();
        }

        QGraphicsView::keyPressEvent(event);
    }
    
    
    void View::mousePressEvent(QMouseEvent* event) 
    {
        if (event->button() == Qt::MiddleButton)
        {
            pan_ = true;
            panStartX_ = event->x();
            panStartY_ = event->y();
            viewport()->setCursor(Qt::ClosedHandCursor); // use viewport to workaround a refresh bug
            event->accept();
            return;
        }
        QGraphicsView::mousePressEvent(event);
    }
    
    
    void View::mouseMoveEvent(QMouseEvent* event) 
    {
        if (pan_)
        {
            horizontalScrollBar()->setValue(horizontalScrollBar()->value() - (event->x() - panStartX_));
            verticalScrollBar()->setValue(verticalScrollBar()->value() -     (event->y() - panStartY_));
            panStartX_ = event->x();
            panStartY_ = event->y();
            event->accept();
            return;
        }
        QGraphicsView::mouseMoveEvent(event);
    }
    
    
    void View::mouseReleaseEvent(QMouseEvent* event)
    {
        if (event->button() == Qt::MiddleButton)
        {
            pan_ = false;
            viewport()->setCursor(Qt::ArrowCursor); // use viewport to workaround a refresh bug
            event->accept();
            return;
        }
        QGraphicsView::mouseReleaseEvent(event);
    }
}
