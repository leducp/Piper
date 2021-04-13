#include "View.h"
#include "Scene.h"
#include "Node.h"
#include "Link.h"
#include "CreatorPopup.h"

#include <QWheelEvent>
#include <QKeyEvent>
#include <QScrollBar>
#include <QGraphicsItem>
#include <QDebug>

namespace piper
{
    QByteArray View::copy_{};


    View::View(QWidget* parent)
        : QGraphicsView(parent)
    {
        setFocusPolicy(Qt::ClickFocus);
        setDragMode(QGraphicsView::RubberBandDrag);

        creator_ = new CreatorPopup(this);
    }


    void View::goHome()
    {
        fitInView(scene()->itemsBoundingRect(), Qt::KeepAspectRatio);
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
            goHome();
            event->accept();
        }
        if ((event->key() == Qt::Key::Key_C) and (event->modifiers() & Qt::ControlModifier))
        {
            copy();
            event->accept();
        }
        if ((event->key() == Qt::Key::Key_V) and (event->modifiers() & Qt::ControlModifier))
        {
            paste();
            event->accept();
        }
        if ((event->key() == Qt::Key::Key_Z) and (event->modifiers() & Qt::ControlModifier))
        {
            undo();
            event->accept();
        }
        if ((event->key() == Qt::Key::Key_Y) and (event->modifiers() & Qt::ControlModifier))
        {
            redo();
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
            verticalScrollBar()->setValue(  verticalScrollBar()->value()   - (event->y() - panStartY_));
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


    void View::copy()
    {
        Scene* pScene = static_cast<Scene*>(scene());

        QList<Node*> nodes;
        for (Node* node : pScene->nodes())
        {
            if (node->isSelected())
            {
                nodes << node;
            }
        }

        QList<Link*> links;
        for (Link* link : pScene->links())
        {
            if (link->isSelected())
            {
                links << link;
            }
        }


        QDataStream stream(&copy_, QIODevice::WriteOnly | QIODevice::Truncate);
        stream << nodes.size();
        for (auto const& node : nodes)
        {
            stream << *node;
        }

        stream << links.size();
        for (auto const& link : links)
        {
            stream << static_cast<Node*>(link->from()->parentItem())->name() << link->from()->name();
            stream << static_cast<Node*>(link->to()->parentItem())->name()   << link->to()->name();
        }
    }


    void View::paste()
    {
        Scene* pScene = static_cast<Scene*>(scene());

        // deselect and move to back all current items in the scene
        for (auto& item : pScene->selectedItems())
        {
            item->setSelected(false);
            item->setZValue(-1);
        }

        struct LinkData
        {
            QString from;
            QString output;
            QString to;
            QString input;
        };

        QDataStream stream(copy_);
        QList<Node*> copies;
        QList<LinkData> links;

        // preload nodes and links
        int nodeCount;
        stream >> nodeCount;
        for (int i = 0; i < nodeCount; ++i)
        {
            Node* node = new Node();
            stream >> *node;
            copies << node;
        }

        int linkCount;
        stream >> linkCount;
        for (int i = 0; i < linkCount; ++i)
        {
            LinkData link;
            stream >> link.from >> link.output >> link.to >> link.input;
            links << link;
        }


        // compute unique name and insert nodes
        for (auto const& copy : copies)
        {
            for (auto const& node : pScene->nodes())
            {
                if (copy->name() == node->name())
                {
                    QString oldName = copy->name();
                    QString newName = copy->name() + "_" + QString::number(pScene->nodes().size());
                    copy->setName(newName);

                    for (auto& link : links)
                    {
                        if (link.from == oldName) { link.from = newName; }
                        if (link.to   == oldName) { link.to   = newName; }
                        qDebug() << "rename link !" << oldName << " " << newName;
                    }
                    break;
                }
            }
            copy->setSelected(true);
            copy->setZValue(1);
            pScene->addNode(copy);
        }

        // copy links
        for (auto const& link : links)
        {
            pScene->connect(link.from, link.output, link.to, link.input);
        }

        // get current curson position
        QPointF cursorScene = mapToScene(mapFromGlobal(QCursor::pos()));
        QPointF deltaToCursor;

        // get delta from cursor and pasted items
        QList<QGraphicsItem*> items = pScene->selectedItems();
        QRectF boundingRectangle;
        for (auto& item : items)
        {
            boundingRectangle = boundingRectangle.united(item->sceneBoundingRect());
        }

        deltaToCursor = cursorScene - boundingRectangle.topLeft();

        // move pasted item to cursor position
        for (auto& item : items)
        {
            item->moveBy(deltaToCursor.x(), deltaToCursor.y());
        }


        // refresh
        pScene->onStageUpdated();
    }


    void View::undo()
    {
        Scene* pScene = static_cast<Scene*>(scene());
        pScene->undo();
    }

    void View::redo()
    {
        Scene* pScene = static_cast<Scene*>(scene());
        pScene->redo();
    }
}
