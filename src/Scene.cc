#include "Scene.h"
#include "Node.h"
#include "Link.h"
#include "ExportBackend.h"
#include "NodeCreator.h"
#include "ThemeManager.h"

#include <QDebug>

#include <QPainter>
#include <QBrush>
#include <QKeyEvent>
#include <algorithm>
#include <QJsonArray>
#include <QMap>
#include <QMessageBox>
#include <cmath>

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
        QVector<Node*> deleteNodes = nodes_;
        for (auto& node : deleteNodes)
        {
            delete node;
        }

        QVector<Link*> deleteLinks = links_;
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
        QVector<Link*> deleteLinks = links_;
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
        QColor default_background = ThemeManager::instance().getNodeTheme().background;
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
        auto const nodeFrom = std::find_if(nodes().begin(), nodes().end(),
            [&](Node const* node) { return (node->name() == from); }
        );
        auto const nodeTo = std::find_if(nodes().begin(), nodes().end(),
            [&](Node const* node) { return (node->name() == to); }
        );

        if (nodeFrom == nodes().end())
        {
            QString error = "Node" + from + "(from) not found";
            links_import_errors_.append(error);
            return;
        }

        if (nodeTo == nodes().end())
        {
            QString error = "Node" + to + "(to) not found";
            links_import_errors_.append(error);
            return;
        }

        Attribute* attrOut{nullptr};
        for (auto& attr : (*nodeFrom)->attributes())
        {
            if (attr->isOutput() and (attr->name() == out))
            {
                attrOut = attr;
                break;
            }
        }

        Attribute* attrIn{nullptr};
        for (auto& attr : (*nodeTo)->attributes())
        {
            if (attr->isInput() and (attr->name() == in))
            {
                attrIn = attr;
                break;
            }
        }

        if (attrIn == nullptr)
        {
            QString error = "Cannot find attribute" + in + "(in) in the node" + to;
            links_import_errors_.append(error);
            return;
        }

        if (attrOut == nullptr)
        {
            QString error = "Cannot find attribute" + out + "(out) in the node" + from;
            links_import_errors_.append(error);
            return;
        }

        if (not attrIn->accept(attrOut))
        {
            QString error = "Cannot connect node" + from + "to node" + to + ". Type mismatch";
            return;
        }

        Link* link = new Link;
        link->connectFrom(attrOut);
        link->connectTo(attrIn);
        addLink(link);
    }


    QModelIndex Scene::addMode(QString const& name)
    {
        // Add item
        QStandardItem* item = new QStandardItem();
        item->setData(name, Qt::DisplayRole);
        item->setDropEnabled(false);;
        modes_->appendRow(item);

        // Enable item selection and put it edit mode
        QModelIndex index = modes_->indexFromItem(item);
        onModeSelected(index);
        return index;
    }


    void Scene::onExport(ExportBackend& backend)
    {
        // -------- stages -------- //
        QVector<QString> stagesArray;
         for (int i = 0; i < stages()->rowCount(); ++i)
        {
            QString stage = stages()->item(i, 0)->data(Qt::DisplayRole).toString();
            stagesArray.append(stage);
        }
        backend.writeStages(stagesArray);

        // -------- nodes -------- //
        for (auto const& node : nodes_)
        {
            QHash<QString, QVariant> attr;
            for (auto const& attribute : node->attributes())
            {
                if (not attribute->isMember())
                {
                    continue;
                }

                attr.insert(attribute->name(), attribute->data());
            }

            backend.writeNode(node->nodeType(), node->name(), node->stage(), attr);
        }

        // -------- links -------- //
        for (auto const& link : links_)
        {
            Node const* from = static_cast<Node const*>(link->from()->parentItem());
            Node const* to   = static_cast<Node const*>(link->to()->parentItem());
            backend.writeLink(from->name(), link->from()->name(), to->name(), link->to()->name(), link->from()->dataType());
        }

        // -------- modes -------- //
        for (int i = 0; i < modes()->rowCount(); ++i)
        {
            QStandardItem* mode = modes()->item(i, 0);
            QString modeName = mode->data(Qt::DisplayRole).toString();
            if (mode->data(Qt::DecorationRole).isValid())
            {
                backend.writeDefaultMode(modeName);
            }

            QHash<QString, QVariant> config = mode->data(Qt::UserRole + 2).toHash();
            QHash<QString, Mode> exportConfig;
            for (auto it = config.constBegin(); it != config.constEnd(); ++it)
            {
                exportConfig[it.key()] = static_cast<enum Mode>(it.value().toInt());
            }

            backend.writeMode(modeName, exportConfig);
        }
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


    void Scene::onModeSetDefault(QModelIndex const& index)
    {
        // Reset select state.
        for (int i = 0; i < modes_->rowCount(); ++i)
        {
            modes_->item(i, 0)->setData(QVariant(), Qt::DecorationRole);
        }

        QStandardItem* currentMode = modes_->itemFromIndex(index);
        currentMode->setData(QIcon(":/icon/star.svg"), Qt::DecorationRole);
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


    void Scene::onImportJson(QJsonObject& json)
    {
        // load stages
        QJsonArray stages= json["Stages"].toArray();
        for (int i = 0; i < stages.size(); ++i)
        {
            QStandardItem* item = new QStandardItem();
            item->setData(generateRandomColor(), Qt::DecorationRole);
            item->setData(stages[i].toString(), Qt::DisplayRole);
            item->setDropEnabled(false);;
            stages_->appendRow(item);
        }

        QJsonObject steps = json["Nodes"].toObject();
        loadNodesJson(steps);

        QJsonArray links= json["Links"].toArray();
        loadLinksJson(links);

        QJsonObject modes = json["Modes"].toObject();
        loadModesJson(modes);

        placeNodesDefaultPosition();
        onStageUpdated();

        // Display import report if something wrong happened.
        if (nodes_import_errors_.isEmpty() and links_import_errors_.isEmpty())
        {
            return;
        }

        QString errors;
        errors += "Nodes:\n";
        for (auto const& err : nodes_import_errors_) { errors += (err + "\n"); }
        errors += "Links:\n";
        for (auto const& err : links_import_errors_) { errors += (err + "\n"); }
        QMessageBox::warning(nullptr, "Import report", errors);
    }


    void Scene::loadNodesJson(QJsonObject& steps)
    {
        QPointF scenePos(0.0, 0.0);
        for (QString stepName : steps.keys())
        {
            QJsonObject step = steps[stepName].toObject();
            QString type = step["type"].toString();

            Node* node = NodeCreator::instance().createItem(type, stepName, "", scenePos);
            if (node == nullptr)
            {
                QString error = "Cannot create node" + stepName + ": type " + type + " is unknwon.";
                nodes_import_errors_.append(error);
                continue;
            }

            for (QString member : step.keys())
            {
                if (member == "type")
                {
                    continue;
                }
                if (member == "stage")
                {
                    node->stage() = step["stage"].toString();
                    continue;
                }

                QVector<Attribute*>& attributes = node->attributes();

                auto cmp = [&member](Attribute* attr) { return attr->name() == member; };

                auto itObj = std::find_if(attributes.begin(), attributes.end(), cmp);
                if (itObj != attributes.end())
                {
                    (*itObj)->setData(QVariant(step[member].toVariant()));
                }
                else
                {
                    QString error = "Attribute " + member + " not found: version mismatch ?";
                    nodes_import_errors_.append(error);
                }
            }

            addNode(node);
        }
    }


    void Scene::loadLinksJson(QJsonArray& links)
    {
        for (int i = 0; i < links.size(); ++i)
        {
            QJsonObject l = links[i].toObject();
            QString from = l["from"].toString();
            QString output = l["out"].toString();
            QString to = l["to"].toString();
            QString input = l["in"].toString();
            connect(from, output, to, input);
        }
    }


    void Scene::loadModesJson(QJsonObject& modes)
    {
        QString defaultMode;
        for (auto mode : modes.keys())
        {
            if (mode == "default")
            {
                // special case: the default key define the default mode to use at startup.
                defaultMode = modes.value(mode).toString();
                continue;
            }

            QStandardItem* item = new QStandardItem();
            item->setData(mode, Qt::DisplayRole);
            item->setDropEnabled(false);

            // Store mode configuration
            QHash<QString, QVariant> nodeMode = item->data(Qt::UserRole + 2).toHash();
            QJsonObject modeObject = modes[mode].toObject();
            QJsonObject modeConfig = modeObject["configuration"].toObject();
            for (auto node : modeConfig.keys())
            {
                auto fromString = [](QString const& mode)
                {
                    if (mode == "Neutral")
                    {
                        return static_cast<int>(Mode::neutral);
                    }
                    if (mode == "Disable")
                    {
                        return static_cast<int>(Mode::disable);
                    }
                    return static_cast<int>(Mode::enable);
                };

                nodeMode[node] = fromString(modeConfig[node].toString());
            }
            item->setData(nodeMode, Qt::UserRole + 2);

            modes_->appendRow(item);
        }

        // apply default mode value.
        for (int i = 0; i < modes_->rowCount(); ++i)
        {
            QStandardItem* item = modes_->item(i, 0);
            if (item->data(Qt::DisplayRole).toString() == defaultMode)
            {
                onModeSetDefault(modes_->indexFromItem(item));
                break;
            }
        }
    }


    void Scene::placeNodesDefaultPosition()
    {
        struct stageInfo {int column; int size;};
        QMap<QString, stageInfo> stageIndex;

        int row = 0;
        QModelIndex index = stages_->index(row, 0);

        while (index.isValid())
        {
            QString stage = stages_->data(index, Qt::DisplayRole).toString();
            stageIndex.insert(stage, {row, 0});

            ++row;
            index = stages_->index(row, 0);
        }

        // Handle steps without stage
        stageIndex.insert("", {row, 0});

        for (auto n : nodes_)
        {
            qreal x = 300 * stageIndex[n->stage()].column;
            qreal y = 200 * stageIndex[n->stage()].size;

            stageIndex[n->stage()].size++;

            n->setPos(x ,y);
            x+= 300;
            y += 150;
        }
    }


    QColor generateRandomColor()
    {
        // procedural color generator: the gold ratio
        static double nextColorHue = 1.0 / (rand() % 100); // don't need a proper random here
        constexpr double golden_ratio_conjugate = 0.618033988749895; // 1 / phi
        nextColorHue += golden_ratio_conjugate;
        nextColorHue = std::fmod(nextColorHue, 1.0);

        QColor nextColor;
        nextColor.setHsvF(nextColorHue, 0.5, 0.99);
        return nextColor;
    }
}
