#include "Scene.h"
#include "Node.h"
#include "Link.h"

#include <QDebug>

#include <QPainter>
#include <QBrush>
#include <QKeyEvent>
#include <algorithm>

namespace piper
{
    Scene::Scene (QObject* parent)
        : QGraphicsScene(parent)
    { }
    
    Scene::~Scene()
    {
        // Manually delete nodes and links because order are important
        QList<Node*> deleteNodes = nodes_;
        for (auto& node : deleteNodes)
        {
            delete node;
        }
        
        QList<Link*> deleteLinks = links_;
        for (auto& link : deleteLinks)
        {
            delete link;
        }
    }

    void Scene::drawBackground(QPainter* painter, QRectF const& rect) 
    {    
        QBrush brush(Qt::SolidPattern);
        brush.setColor({40, 40, 40}),
        painter->fillRect(rect, brush);
        
        QPen pen;
        pen.setColor({100, 100, 100});
        pen.setWidth(2);
        painter->setPen(pen);
        
        constexpr int gridSize = 20;
        qreal left = int(rect.left()) - (int(rect.left()) % gridSize);
        qreal top = int(rect.top()) - (int(rect.top()) % gridSize);
        QVector<QPointF> points;
        for (qreal x = left; x < rect.right(); x += gridSize)
        {
            for (qreal y = top; y < rect.bottom(); y += gridSize)
            {
                points.append(QPointF(x,y));
            }
        }
        
        painter->drawPoints(points.data(), points.size());
    }

    
    void Scene::keyReleaseEvent(QKeyEvent* keyEvent) 
    {
        if (keyEvent->key() == Qt::Key::Key_Delete)
        {
            for (auto& item : selectedItems())
            {
                delete item;
            }
        }
        
        // destroy orphans link
        QList<Link*> deleteLinks = links_;
        for (auto& link : deleteLinks)
        {
            if (not link->isConnected())
            {
                delete link;
            }
        }
    }
    
    
    void Scene::resetStagesColor()
    {
        for (auto& node : nodes_)
        {
            node->setBackgroundColor(default_background);
        }
    }
    
    
    void Scene::updateStagesColor(QString const& stage, QColor const& color)
    {
        for (auto& node : nodes_)
        {
            if (node->stage() == stage)
            {
                node->setBackgroundColor(color);
            }
        }
    }
    
    
    void Scene::addNode(Node* node)
    {
        addItem(node);
        nodes_.append(node);
    }
    
    
    void Scene::removeNode(Node* node)
    {
        removeItem(node);
        nodes_.removeAll(node);
    }
    
    
    void Scene::addLink(Link* link)
    {
        addItem(link);
        links_.append(link);
    }
    
    
    void Scene::removeLink(Link* link)
    {
        removeItem(link);
        links_.removeAll(link);
    }
    
    
    void Scene::connect(QString const& from, QString const& out, QString const& to, QString const& in)
    {
        Node const* nodeFrom = *std::find_if(nodes().begin(), nodes().end(),
            [&](Node const* node) { return (node->name() == from); }
        );
        Node const* nodeTo = *std::find_if(nodes().begin(), nodes().end(),
            [&](Node const* node) { return (node->name() == to); }
        );

        Attribute* attrOut{nullptr};
        for (auto& attr : nodeFrom->attributes())
        {
            if (attr->isOutput() and (attr->name() == out))
            {
                attrOut = attr;
                break;
            }
        }

        Attribute* attrIn{nullptr};
        for (auto& attr : nodeTo->attributes())
        {
            if (attr->isInput() and (attr->name() == in))
            {
                attrIn = attr;
                break;
            }
        }

        if (attrIn == nullptr)
        {
            qDebug() << "Can't find attribute" << in << "(in) in the node" << to;
            std::abort();
        }

        if (attrOut == nullptr)
        {
            qDebug() << "Can't find attribute" << out << "(out) in the node" << from;
            std::abort();
        }

        if (not attrIn->accept(attrOut))
        {
            qDebug() << "Can't connect attribute" << from << "to attribute" << to;
            std::abort();
        }

        Link* link= new Link;
        link->connectFrom(attrOut);
        link->connectTo(attrIn);
        addLink(link);
    }
}
