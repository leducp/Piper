#include "Editor.h"

#include "Node.h"
#include "Link.h"
#include "NodeCreator.h"

#include <QDebug>
#include <QPushButton>
#include <QColorDialog>
#include <QFileDialog>

namespace piper
{
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
        
        QObject::connect(ui_.actionsave,    &QAction::triggered, this, &Editor::onSave);
        QObject::connect(ui_.actionsave_on, &QAction::triggered, this, &Editor::onSaveOn);
        QObject::connect(ui_.actionload,    &QAction::triggered, this, &Editor::onLoad);
        QObject::connect(ui_.actionexport,  &QAction::triggered, this, &Editor::onExport);
        
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
        nodePropertyModel_->setData(index, "type", Qt::DisplayRole);  
        nodePropertyModel_->itemFromIndex(index)->setFlags(Qt::ItemIsSelectable);
        
        index = nodePropertyModel_->index(1, 0);
        nodePropertyModel_->setData(index, "name", Qt::DisplayRole);  
        nodePropertyModel_->itemFromIndex(index)->setFlags(Qt::ItemIsSelectable);
        
        index = nodePropertyModel_->index(2, 0);
        nodePropertyModel_->setData(index, "stage", Qt::DisplayRole);  
        nodePropertyModel_->itemFromIndex(index)->setFlags(Qt::ItemIsSelectable);
        
        ui_.nodeProperty->setModel(nodePropertyModel_);
        QObject::connect(nodePropertyModel_, &QStandardItemModel::itemChanged, this, &Editor::onNodePropertyUpdated);
        
        
        // connect scene to MMI
        QObject::connect(scene_, &QGraphicsScene::changed, this, &Editor::onNodeUpdated);
    }
    
    
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
                nodePropertyModel_->setData(index, node->nodeType(), Qt::DisplayRole);  
                nodePropertyModel_->itemFromIndex(index)->setFlags(Qt::ItemIsSelectable);
                
                index = nodePropertyModel_->index(1, 1);
                nodePropertyModel_->setData(index, node->name(), Qt::DisplayRole);  
                
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
    
    
    void Editor::onSaveOn()
    {
        QString filename = QFileDialog::getSaveFileName(this,tr("Save"), "", tr("Piper project (*.piper);;All Files (*)"));
        writeProjectFile(filename);
    }


    void Editor::onSave()
    {
        if (projectFile_.isEmpty())
        {
            projectFile_ = QFileDialog::getSaveFileName(this,tr("Save"), "", tr("Piper project (*.piper);;All Files (*)"));
        }
        writeProjectFile(projectFile_);
    }


    void Editor::onLoad()
    {
        projectFile_ = QFileDialog::getOpenFileName(this,tr("Load"), "", tr("Piper project (*.piper);;All Files (*)"));
        loadProjectFile(projectFile_);
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
        
        // Export stages
        for (int i = 0; i < stageModel_->rowCount(); ++i)
        {
            exportBackend_->writeStage(stageModel_->item(i, 0)->data(Qt::DisplayRole).toString());
        }
        
        // Export nodes
        for (auto const& node : Node::items())
        {
            exportBackend_->writeNodeMetadata(node->nodeType(), node->name(), node->stage());
            
            // Export node's attributes
            for (auto const& attr: node->attributes())
            {
                exportBackend_->writeNodeAttribute(node->name(), attr->name(), attr->data());
            }
        }
        
        // Export links
        for (auto const& link : Link::items())
        {
            Node* from = static_cast<Node*>(link->from()->parentItem());
            Node* to = static_cast<Node*>(link->to()->parentItem());
            exportBackend_->writeLink(from->name(), link->from()->name(), to->name(), link->to()->name());
        }   
    }
    
    
    void Editor::writeProjectFile(QString const& filename)
    {
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
            out << node->nodeType() << node->name() << node->stage() << node->pos();
            
            // save node's attributes
            out << node->attributes().size();
            for (auto const& attr: node->attributes())
            {
                out << attr->name() << attr->data();
            }
        }
        
        // save links
        out << Link::items().size();
        for (auto const& link : Link::items())
        {
            Node* from = static_cast<Node*>(link->from()->parentItem());
            out << from->name() << link->from()->name();

            Node* to = static_cast<Node*>(link->to()->parentItem());
            out << to->name() << link->to()->name();
        }
    }
    
    
    void Editor::loadProjectFile(QString const& filename)
    {
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
            QString type, name, stage;
            QPointF pos;
            in >> type >> name >> stage >> pos;
            
            QHash<QString, QVariant> attributes;
            int attributesSize;
            in >> attributesSize;
            for (int j = 0; j < attributesSize; ++j)
            {
                QString attributeName;
                QVariant attributeData;
                in >> attributeName >> attributeData;
                attributes.insert(attributeName, attributeData);
            }
            
            Node* item = NodeCreator::instance().createItem(type, name, stage, pos);
            for (auto const& attr: item->attributes())
            {
               if (attributes.contains(attr->name()))
               {
                   attr->setData(attributes.value(attr->name()));
               }
            }
            
            scene_->addItem(item);
        }
        
        // load links
        int links;
        in >> links;
        for (int i = 0; i < links; ++i)
        {
            QString from, output;
            in >> from >> output;
            
            QString to, input;
            in >> to >> input;

            Link* link = piper::connect(from, output, to, input);
            scene_->addItem(link);
        }
        
        // update display
        onStageUpdated();
    }
}
