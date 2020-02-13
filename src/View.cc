#include "View.h"

#include <QWheelEvent>
#include <QKeyEvent>
#include <QDebug>

namespace piper
{
    View::View(QWidget* parent)
        : QGraphicsView(parent)
    {

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

        QGraphicsView::keyPressEvent(event);
    }
}
