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
    {
        // Prepare stage model
        stages_ = new QStandardItemModel(this);
        stages_->insertColumns(0, 1);
        QObject::connect(stages_, &QStandardItemModel::itemChanged, this, &Scene::onStageUpdated);

        // Prepare mode model
        modes_ = new QStandardItemModel(this);
        modes_->insertColumns(0, 1);
        QObject::connect(modes_, &QStandardItemModel::rowsRemoved, this, &Scene::onModeRemoved);
    }


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


    void Scene::onStageUpdated()
    {
        int row = 0;
        QModelIndex index = stages_->index(row, 0);

        resetStagesColor();
        while (index.isValid())
        {
            QString stage = stages_->data(index, Qt::DisplayRole).toString();
            QColor color = stages_->data(index, Qt::DecorationRole).value<QColor>();
            updateStagesColor(stage, color);

            ++row;
            index = stages_->index(row, 0);
        }
    }



    void Scene::addNode(Node* node)
    {
        addItem(node);
        nodes_.append(node);
    }


    void Scene::removeNode(Node* node)
    {
        // Remove from mode
        for (int i = 0; i < modes_->rowCount(); ++i)
        {
            QStandardItem* mode = modes_->item(i, 0);
            QHash<QString, QVariant> nodeMode = mode->data(Qt::UserRole + 2).toHash();
            nodeMode.remove(node->name());
            mode->setData(nodeMode, Qt::UserRole + 2);
        }

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


    void Scene::onModeSelected(QModelIndex const& index)
    {
        // Reset select state.
        for (int i = 0; i < modes_->rowCount(); ++i)
        {
            modes_->item(i, 0)->setData(false, Qt::UserRole + 1);
        }

        QStandardItem* currentMode = modes_->itemFromIndex(index);
        currentMode->setData(true, Qt::UserRole + 1);

        // Update node display.
        QHash<QString, QVariant> nodeMode = currentMode->data(Qt::UserRole + 2).toHash();
        for (auto& node : nodes_)
        {
            auto it = nodeMode.find(node->name());
            if (it == nodeMode.end())
            {
                // Default mode is enabled.
                node->setMode(Mode::enable);
                continue;
            }

            node->setMode(static_cast<enum Mode>(it.value().toInt()));
        }
    }


    void Scene::onModeRemoved()
    {
        for (auto& node : nodes_)
        {
            node->setMode(Mode::enable);
        }
    }


    QDataStream& operator<<(QDataStream& out, Scene const& scene)
    {
        // save stages.
        out << scene.stages()->rowCount();
        for (int i = 0; i < scene.stages()->rowCount(); ++i)
        {
            out << *scene.stages()->item(i, 0);
        }

        // save modes.
        out << scene.modes()->rowCount();
        for (int i = 0; i < scene.modes()->rowCount(); ++i)
        {
            out << *scene.modes()->item(i, 0);
        }

        // save nodes.
        out << scene.nodes().size();
        for (auto const& node : scene.nodes())
        {
            out << *node;
        }

        // save links.
        out << scene.links().size();
        for (auto const& link : scene.links())
        {
            out << static_cast<Node*>(link->from()->parentItem())->name() << link->from()->name();
            out << static_cast<Node*>(link->to()->parentItem())->name()   << link->to()->name();
        }

        return out;
    }


    QDataStream& operator>>(QDataStream& in, Scene& scene)
    {
        // Load stages.
        int stageCount;
        in >> stageCount;
        for (int i = 0; i < stageCount; ++i)
        {
            QStandardItem* item = new QStandardItem();
            in >> *item;
            scene.stages()->setItem(i, item);
        }

        // Load modes.
        int modeCount;
        in >> modeCount;
        for (int i = 0; i < modeCount; ++i)
        {
            QStandardItem* item = new QStandardItem();
            in >> *item;
            scene.modes()->setItem(i, item);
        }

        // Load nodes.
        int nodeCount;
        in >> nodeCount;
        for (int i = 0; i < nodeCount; ++i)
        {
            Node* node = new Node();
            in >> *node;
            scene.addNode(node);
        }

        // Load links.
        int linkCount;
        in >> linkCount;
        for (int i = 0; i < linkCount; ++i)
        {
            QString from, output;
            in >> from >> output;

            QString to, input;
            in >> to >> input;

            scene.connect(from, output, to, input);
        }

        scene.onStageUpdated();

        return in;
    }
}
