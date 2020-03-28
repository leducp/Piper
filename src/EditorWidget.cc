#include "EditorWidget.h"
#include "PropertyDelegate.h"
#include "ui_EditorWidget.h"

#include "Scene.h" 
#include "Node.h"
#include "Link.h"

#include <cmath>
#include <QColorDialog>

namespace piper
{
    EditorWidget::EditorWidget(QWidget* parent)
        : QWidget(parent)
        , ui_(new Ui::EditorWidget)
        , scene_(new Scene(this))
    {
        ui_->setupUi(this);
        ui_->view->setScene(scene_);
        
        QObject::connect(ui_->stage_add,   &QPushButton::clicked, this, &EditorWidget::onAddStage);
        QObject::connect(ui_->stage_rm,    &QPushButton::clicked, this, &EditorWidget::onRmStage);
        QObject::connect(ui_->stage_color, &QPushButton::clicked, this, &EditorWidget::onColorStage);
        
        // Prepare stage model
        stage_model_ = new QStandardItemModel(this);
        stage_model_->insertColumns(0, 1);
        QObject::connect(stage_model_, &QStandardItemModel::itemChanged, this, &EditorWidget::onStageUpdated);

        ui_->stages->setModel(stage_model_);
        ui_->stages->setEditTriggers(QAbstractItemView::AnyKeyPressed |
                                     QAbstractItemView::DoubleClicked);
        ui_->stages->setDragDropMode(QAbstractItemView::InternalMove);

        // prepare node model
        node_model_ = new QStandardItemModel(this);
        node_model_->insertColumns(0, 1);

        ui_->nodes->setModel(node_model_);
        ui_->nodes->setEditTriggers(QAbstractItemView::NoEditTriggers);
        QObject::connect(ui_->nodes, &QAbstractItemView::clicked, this, &EditorWidget::onNodeSelected);

        // prepare node property model
        node_property_model_ = new QStandardItemModel(this);
        node_property_model_->insertColumns(0, 2);
        node_property_model_->insertRows(0, 3);
        node_property_model_->setHeaderData(0, Qt::Horizontal, "Property", Qt::DisplayRole);
        node_property_model_->setHeaderData(1, Qt::Horizontal, "Value", Qt::DisplayRole);

        QModelIndex index = node_property_model_->index(0, 0);
        node_property_model_->setData(index, "type", Qt::DisplayRole);
        node_property_model_->itemFromIndex(index)->setFlags(Qt::ItemIsSelectable);

        index = node_property_model_->index(1, 0);
        node_property_model_->setData(index, "name", Qt::DisplayRole);
        node_property_model_->itemFromIndex(index)->setFlags(Qt::ItemIsSelectable);

        index = node_property_model_->index(2, 0);
        node_property_model_->setData(index, "stage", Qt::DisplayRole);
        node_property_model_->itemFromIndex(index)->setFlags(Qt::ItemIsSelectable);

        ui_->nodeProperty->setModel(node_property_model_);

        StagePropertyDelegate* delegate = new StagePropertyDelegate();
        delegate->setStageModel(stage_model_);
        ui_->nodeProperty->setItemDelegateForRow(2, delegate);
        QObject::connect(node_property_model_, &QStandardItemModel::itemChanged, this, &EditorWidget::onNodePropertyUpdated);


        // connect scene to MMI
        QObject::connect(scene_, &QGraphicsScene::changed, this, &EditorWidget::onNodeUpdated);
    }
    
    
    void EditorWidget::onAddStage()
    {
        // procedural color generator: the gold ratio
        static double nextColorHue = 1.0 / rand(); // don't need a proper random here
        constexpr double golden_ratio_conjugate = 0.618033988749895; // 1 / phi
        nextColorHue += golden_ratio_conjugate;
        nextColorHue = std::fmod(nextColorHue, 1.0);

        QColor nextColor;
        nextColor.setHsvF(nextColorHue, 0.5, 0.99);

        QString nextName = "stage" + QString::number(stage_model_->rowCount());

        // Add item
        QStandardItem* item = new QStandardItem();
        item->setData(nextColor, Qt::DecorationRole);
        item->setData(nextName, Qt::DisplayRole);
        item->setDropEnabled(false);;
        stage_model_->appendRow(item);

        // Enable item selection and put it edit mode
        QModelIndex index = stage_model_->indexFromItem(item);
        ui_->stages->setCurrentIndex(index);
        ui_->stages->edit(index);
    }


    void EditorWidget::onRmStage()
    {
        int row = ui_->stages->currentIndex().row();
        stage_model_->removeRows(row, 1);

        onStageUpdated();
    }


    void EditorWidget::onColorStage()
    {
        QModelIndex index = ui_->stages->currentIndex();
        QColor current = stage_model_->data(index, Qt::DecorationRole).value<QColor>();

        QColor newColor = QColorDialog::getColor(current);
        stage_model_->setData(index, newColor, Qt::DecorationRole);
    }
    
    
    void EditorWidget::onStageUpdated()
    {
        int row = 0;
        QModelIndex index = stage_model_->index(row, 0);

        scene_->resetStagesColor();
        while (index.isValid())
        {
            QString stage = stage_model_->data(index, Qt::DisplayRole).toString();
            QColor color = stage_model_->data(index, Qt::DecorationRole).value<QColor>();
            scene_->updateStagesColor(stage, color);

            ++row;
            index = stage_model_->index(row, 0);
        }
    }


    void EditorWidget::onNodeUpdated()
    {
        // disconnect signal during auto populate
        QObject::disconnect(node_property_model_, &QStandardItemModel::itemChanged, this, &EditorWidget::onNodePropertyUpdated);

        node_model_->clear();
        for (auto const& node : scene_->nodes())
        {
            QStandardItem* item = new QStandardItem();
            item->setData(node->name(), Qt::DisplayRole);
            node_model_->appendRow(item);

            if (node->isSelected())
            {
                QModelIndex index = node_model_->indexFromItem(item);
                ui_->nodes->setCurrentIndex(index);

                // populate property
                index = node_property_model_->index(0, 1);
                node_property_model_->setData(index, node->nodeType(), Qt::DisplayRole);
                node_property_model_->itemFromIndex(index)->setFlags(Qt::ItemIsSelectable);

                index = node_property_model_->index(1, 1);
                node_property_model_->setData(index, node->name(), Qt::DisplayRole);

                index = node_property_model_->index(2, 1);
                node_property_model_->setData(index, node->stage(), Qt::DisplayRole);
            }
        }

        QObject::connect(node_property_model_, &QStandardItemModel::itemChanged, this, &EditorWidget::onNodePropertyUpdated);
    }


    void EditorWidget::onNodePropertyUpdated()
    {
        QModelIndex index = node_property_model_->index(1, 1);
        QString name = node_property_model_->data(index, Qt::DisplayRole).toString();

        index = node_property_model_->index(2, 1);
        QString stage = node_property_model_->data(index, Qt::DisplayRole).toString();

        for (auto const& node : scene_->nodes())
        {
            if (node->isSelected())
            {
                node->setName(name);
                node->stage() = stage;
            }
        }

        onStageUpdated();
    }


    void EditorWidget::onNodeSelected(QModelIndex const& index)
    {
        QString name = node_model_->data(index, Qt::DisplayRole).toString();

        // deslect all nodes
        for (auto const& node : scene_->nodes())
        {
            node->setSelected(false);
        }

        // select the first matching node.
        for (auto const& node : scene_->nodes())
        {
            if (node->name() == name)
            {
                node->setSelected(true);
                break;
            }
        }
    }
    
    
    QDataStream& operator<<(QDataStream& out, EditorWidget const& editor)
    {
        // save stages.
        out << editor.stage_model_->rowCount();
        for (int i = 0; i < editor.stage_model_->rowCount(); ++i)
        {
            out << *editor.stage_model_->item(i, 0);
        }
        
        // save nodes.
        out << editor.scene_->nodes().size();
        for (auto const& node : editor.scene_->nodes())
        {
            out << *node;
        }
        
        // save links.
        out << editor.scene_->links().size();
        for (auto const& link : editor.scene_->links())
        {
            out << static_cast<Node*>(link->from()->parentItem())->name() << link->from()->name();
            out << static_cast<Node*>(link->to()->parentItem())->name()   << link->to()->name();
        }
        
        return out;
    }
    
    
    QDataStream& operator>>(QDataStream& in, EditorWidget& editor)
    {   
        // Load stages.
        int stageCount;
        in >> stageCount;
        for (int i = 0; i < stageCount; ++i)
        {
            QStandardItem* item = new QStandardItem();
            in >> *item;
            editor.stage_model_->setItem(i, item);
        }
        
        // Load nodes.
        int nodeCount;
        in >> nodeCount;
        for (int i = 0; i < nodeCount; ++i)
        {
            Node* node = new Node();
            in >> *node;
            editor.scene_->addNode(node);
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
            
            editor.scene_->connect(from, output, to, input);
        }
        
        return in;
    }
}
