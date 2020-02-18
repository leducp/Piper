#include "Editor.h"
//#include "ui_node.h"

#include "Node.h"
#include "Link.h"
#include "NodeCreator.h"

#include <QDebug>
#include <QPushButton>
#include <QColorDialog>
#include <QFileDialog>
#include <QStringList>

namespace piper
{
    void Editor::onAddStage()
    {
        // Add item
        QStandardItem* item = new QStandardItem();
        item->setData(QColor("red"), Qt::DecorationRole);
        stageModel_->appendRow(item);
        
        // Enable item selection and put it edit mode
        QModelIndex index = stageModel_->indexFromItem(item);
        ui_.stages->setCurrentIndex(index);
        ui_.stages->edit(index);
    }


    void Editor::onRmStage()
    {
        int row = ui_.stages->currentIndex().row();
        stageModel_->removeRows(row, 1);
        
        onStageUpdated();
    }


    void Editor::onColorStage()
    {
        QModelIndex index = ui_.stages->currentIndex();
        QColor current = stageModel_->data(index, Qt::DecorationRole).value<QColor>();
        
        QColor newColor = QColorDialog::getColor(current);
        stageModel_->setData(index, newColor, Qt::DecorationRole);
    }


    void Editor::onStageUpdated()
    {
        int row = 0;
        QModelIndex index = stageModel_->index(row, 0);

        Node::resetStagesColor();
        while (index.isValid())
        {
            QString stage = stageModel_->data(index, Qt::DisplayRole).toString();   
            QColor color = stageModel_->data(index, Qt::DecorationRole).value<QColor>();
            Node::updateStagesColor(stage, color);
        
            ++row;
            index = stageModel_->index(row, 0);
        }
    }


    void Editor::onNodeUpdated()
    {
        // disconnect signal during auto populate
        QObject::disconnect(nodePropertyModel_, &QStandardItemModel::itemChanged, this, &Editor::onNodePropertyUpdated);
        
        nodeModel_->clear();
        for (auto const& node : Node::items())
        {
            QStandardItem* item = new QStandardItem();
            item->setData(node->name(), Qt::DisplayRole);
            nodeModel_->appendRow(item);
            
            QModelIndex index = nodeModel_->indexFromItem(item);
            if (node->isSelected())
            {
                ui_.nodes->setCurrentIndex(index);
                
                // populate property            
                index = nodePropertyModel_->index(0, 1);
                nodePropertyModel_->setData(index, node->name(), Qt::DisplayRole);  
                
                index = nodePropertyModel_->index(1, 1);
                nodePropertyModel_->setData(index, node->nodeType(), Qt::DisplayRole);  
                nodePropertyModel_->itemFromIndex(index)->setFlags(Qt::ItemIsSelectable);
                
                index = nodePropertyModel_->index(2, 1);
                nodePropertyModel_->setData(index, node->stage(), Qt::DisplayRole);  
            }
        }
        
        QObject::connect(nodePropertyModel_, &QStandardItemModel::itemChanged, this, &Editor::onNodePropertyUpdated);
    }


    void Editor::onNodePropertyUpdated()
    {
        QModelIndex index = nodePropertyModel_->index(0, 1);    
        QString name = nodePropertyModel_->data(index, Qt::DisplayRole).toString();
        
        index = nodePropertyModel_->index(2, 1);    
        QString stage = nodePropertyModel_->data(index, Qt::DisplayRole).toString();
        
        for (auto const& node : Node::items())
        {
            if (node->isSelected())
            {
                node->setName(name);
                node->stage() = stage;
            }
        }
        
        onStageUpdated();
    }


    void Editor::onNodeSelected(QModelIndex const& index)
    {
        QString name = nodeModel_->data(index, Qt::DisplayRole).toString();
        
        // deslect all nodes
        for (auto const& node : Node::items())
        {
            node->setSelected(false);
        }
        
        // select the first matching node.
        for (auto const& node : Node::items())
        {
            if (node->name() == name)
            {
                node->setSelected(true);
                break;
            }
        }
    }


    void Editor::onSave()
    {
        QString filename = QFileDialog::getSaveFileName(this,tr("Save"), "", tr("Piper project (*.piper);;All Files (*)"));
        QFile file(filename);
        file.open(QIODevice::WriteOnly | QIODevice::Truncate);
        
        QDataStream out(&file);
        
        // save stages
        out << stageModel_->rowCount();
        for (int i = 0; i < stageModel_->rowCount(); ++i)
        {
            out << *stageModel_->item(i, 0);
        }

        // save nodes
        out << Node::items().size();
        for (auto const& node : Node::items())
        {
            out << node->nodeType();
            out << node->name();
            out << node->stage();
            out << node->pos();
        }
        
        // save links
        out << Link::items().size();
        for (auto const& link : Link::items())
        {
            Node* from = static_cast<Node*>(link->from()->parentItem());
            Node* to = static_cast<Node*>(link->to()->parentItem());
            out << from->name();
            out << link->from()->name();
            out << to->name();
            out << link->to()->name();
        }
    }


    void Editor::onLoad()
    {
        QString filename = QFileDialog::getOpenFileName(this,tr("Load"), "", tr("Piper project (*.piper);;All Files (*)"));
        QFile file(filename);
        file.open(QIODevice::ReadOnly);
        
        QDataStream in(&file);
        
        // load stages
        int row;
        in >> row;
        stageModel_->clear();
        for (int i = 0; i < row; ++i)
        {
            QStandardItem* item = new QStandardItem();
            in >> *item;
            stageModel_->setItem(i, item);
        }
        
        // load nodes
        scene_->clear();
        
        int nodes;
        in >> nodes;
        for (int i = 0; i < nodes; ++i)
        {
            QString type;
            in >> type;
            QString name;
            in >> name;
            QString stage;
            in >> stage;
            QPointF pos;
            in >> pos;
            
            Node* item = NodeCreator::instance().createItem(type, name, stage, pos);
            scene_->addItem(item);
        }
        
        // load links
        int links;
        in >> links;
        for (int i = 0; i < links; ++i)
        {
            QString from;
            in >> from;
            QString output;
            in >> output;
            QString to;
            in >> to;
            QString input;
            in >> input;
            
            Link* link = piper::connect(from, output, to, input);
            scene_->addItem(link);
        }
        
        // update display
        onStageUpdated();
    }


    void Editor::onExport()
    {
        if (exportBackend_ == nullptr)
        {
            qDebug() << "No export backend set. Aborting";
            return;
        }
        
        QString filename = QFileDialog::getSaveFileName(this,tr("Export"), "", tr("All Files (*)"));
        exportBackend_->init(filename);
        
        // Prepare stages
        QStringList stages;
        for (int i = 0; i < stageModel_->rowCount(); ++i)
        {
            stages << stageModel_->item(i, 0)->data(Qt::DisplayRole).toString();
        }
        
        exportBackend_->exportData(stages, Node::items(), Link::items());
    }


    Editor::Editor(QWidget *parent, ExportBackend* exportBackend) 
        : QMainWindow(parent)
        , exportBackend_{exportBackend}
    {
        ui_.setupUi(this);
        scene_ = new Scene (this);
        ui_.view->setScene(scene_);

        QObject::connect(ui_.stage_add,   &QPushButton::clicked, this, &Editor::onAddStage);
        QObject::connect(ui_.stage_rm,    &QPushButton::clicked, this, &Editor::onRmStage);
        QObject::connect(ui_.stage_color, &QPushButton::clicked, this, &Editor::onColorStage);
        //QObject::connect(ui_.stage_up,   &QPushButton::clicked, this, &Editor::addStage);
        //QObject::connect(ui_.stage_down, &QPushButton::clicked, this, &Editor::addStage);
        
        QObject::connect(ui_.actionsave,   &QAction::triggered, this, &Editor::onSave);
        QObject::connect(ui_.actionload,   &QAction::triggered, this, &Editor::onLoad);
        QObject::connect(ui_.actionexport, &QAction::triggered, this, &Editor::onExport);
        
        // Prepare stage model
        stageModel_ = new QStandardItemModel(this);
        stageModel_->insertColumns(0, 1);
        QObject::connect(stageModel_, &QStandardItemModel::itemChanged, this, &Editor::onStageUpdated);

        ui_.stages->setModel(stageModel_);
        ui_.stages->setEditTriggers(QAbstractItemView::AnyKeyPressed |
                                    QAbstractItemView::DoubleClicked);
        ui_.stages->setDragDropMode(QAbstractItemView::InternalMove);
        
        // prepare node model
        nodeModel_ = new QStandardItemModel(this);
        nodeModel_->insertColumns(0, 1);
        
        ui_.nodes->setModel(nodeModel_);
        ui_.nodes->setEditTriggers(QAbstractItemView::NoEditTriggers);
        QObject::connect(ui_.nodes, &QAbstractItemView::clicked, this, &Editor::onNodeSelected);
        
        // prepare node property model
        nodePropertyModel_ = new QStandardItemModel(this);
        nodePropertyModel_->insertColumns(0, 2);
        nodePropertyModel_->insertRows(0, 3);
        nodePropertyModel_->setHeaderData(0, Qt::Horizontal, "Property", Qt::DisplayRole);
        nodePropertyModel_->setHeaderData(1, Qt::Horizontal, "Value", Qt::DisplayRole);
        
        QModelIndex index = nodePropertyModel_->index(0, 0);
        nodePropertyModel_->setData(index, "name", Qt::DisplayRole);  
        nodePropertyModel_->itemFromIndex(index)->setFlags(Qt::ItemIsSelectable);
        
        index = nodePropertyModel_->index(1, 0);
        nodePropertyModel_->setData(index, "type", Qt::DisplayRole);  
        nodePropertyModel_->itemFromIndex(index)->setFlags(Qt::ItemIsSelectable);
        
        index = nodePropertyModel_->index(2, 0);
        nodePropertyModel_->setData(index, "stage", Qt::DisplayRole);  
        nodePropertyModel_->itemFromIndex(index)->setFlags(Qt::ItemIsSelectable);
        
        ui_.nodeProperty->setModel(nodePropertyModel_);
        QObject::connect(nodePropertyModel_, &QStandardItemModel::itemChanged, this, &Editor::onNodePropertyUpdated);
        
        
        // connect scene to MMI
        QObject::connect(scene_, &QGraphicsScene::changed, this, &Editor::onNodeUpdated);
        
        NodeCreator::instance().addItem("PID", 
                        { 
                            {"target", "Kinematic", AttributeInfo::Type::input}, 
                            {"measurements", "Kinematic", AttributeInfo::Type::input},
                            {"output", "torque", AttributeInfo::Type::output},
                            {"Kp", "float", AttributeInfo::Type::member},
                            {"Ki", "float", AttributeInfo::Type::member},
                            {"Kd", "float", AttributeInfo::Type::member}
                        });
        NodeCreator::instance().addItem("SimpleTransmission", 
                        { 
                            {"input", "torque", AttributeInfo::Type::input}, 
                            {"output", "torque", AttributeInfo::Type::output},
                            {"zero", "float", AttributeInfo::Type::member}
                        });
        
        NodeCreator::instance().addItem("Yolo", 
                        { 
                            {"input", "torque", AttributeInfo::Type::input}, 
                            {"output", "torque", AttributeInfo::Type::output},
                            {"zero", "float", AttributeInfo::Type::member}
                        });
            
        NodeCreator::instance().addItem("Yala", 
                        { 
                            {"input", "torque", AttributeInfo::Type::input}, 
                            {"output", "torque", AttributeInfo::Type::output},
                            {"zero", "float", AttributeInfo::Type::member}
                        });
    }
}
