#include "Editor.h"
#include "ui_editor.h"

#include "Node.h"
#include "Link.h"
#include "NodeCreator.h"

#include <cmath>
#include <QDebug>
#include <QPushButton>
#include <QColorDialog>
#include <QFileDialog>

namespace piper
{
    Editor::Editor(QWidget *parent, ExportBackend* exportBackend) 
        : QMainWindow(parent)
        , ui_(new Ui::Editor)
        , export_backend_{exportBackend}
    {
        ui_->setupUi(this);
        scene_ = new Scene(this);
        ui_->view->setScene(scene_);

        QObject::connect(ui_->stage_add,   &QPushButton::clicked, this, &Editor::onAddStage);
        QObject::connect(ui_->stage_rm,    &QPushButton::clicked, this, &Editor::onRmStage);
        QObject::connect(ui_->stage_color, &QPushButton::clicked, this, &Editor::onColorStage);
        
        QObject::connect(ui_->actionsave,    &QAction::triggered, this, &Editor::onSave);
        QObject::connect(ui_->actionsave_on, &QAction::triggered, this, &Editor::onSaveOn);
        QObject::connect(ui_->actionload,    &QAction::triggered, this, &Editor::onLoad);
        QObject::connect(ui_->actionexport,  &QAction::triggered, this, &Editor::onExport);
        
        // Prepare stage model
        stage_model_ = new QStandardItemModel(this);
        stage_model_->insertColumns(0, 1);
        QObject::connect(stage_model_, &QStandardItemModel::itemChanged, this, &Editor::onStageUpdated);

        ui_->stages->setModel(stage_model_);
        ui_->stages->setEditTriggers(QAbstractItemView::AnyKeyPressed |
                                    QAbstractItemView::DoubleClicked);
        ui_->stages->setDragDropMode(QAbstractItemView::InternalMove);
        
        // prepare node model
        node_model_ = new QStandardItemModel(this);
        node_model_->insertColumns(0, 1);
        
        ui_->nodes->setModel(node_model_);
        ui_->nodes->setEditTriggers(QAbstractItemView::NoEditTriggers);
        QObject::connect(ui_->nodes, &QAbstractItemView::clicked, this, &Editor::onNodeSelected);
        
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
        QObject::connect(node_property_model_, &QStandardItemModel::itemChanged, this, &Editor::onNodePropertyUpdated);
        
        
        // connect scene to MMI
        QObject::connect(scene_, &QGraphicsScene::changed, this, &Editor::onNodeUpdated);
    }
    
    
    void Editor::onAddStage()
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


    void Editor::onRmStage()
    {
        int row = ui_->stages->currentIndex().row();
        stage_model_->removeRows(row, 1);
        
        onStageUpdated();
    }


    void Editor::onColorStage()
    {
        QModelIndex index = ui_->stages->currentIndex();
        QColor current = stage_model_->data(index, Qt::DecorationRole).value<QColor>();
        
        QColor newColor = QColorDialog::getColor(current);
        stage_model_->setData(index, newColor, Qt::DecorationRole);
    }


    void Editor::onStageUpdated()
    {
        int row = 0;
        QModelIndex index = stage_model_->index(row, 0);

        Node::resetStagesColor();
        while (index.isValid())
        {
            QString stage = stage_model_->data(index, Qt::DisplayRole).toString();   
            QColor color = stage_model_->data(index, Qt::DecorationRole).value<QColor>();
            Node::updateStagesColor(stage, color);
        
            ++row;
            index = stage_model_->index(row, 0);
        }
    }


    void Editor::onNodeUpdated()
    {
        // disconnect signal during auto populate
        QObject::disconnect(node_property_model_, &QStandardItemModel::itemChanged, this, &Editor::onNodePropertyUpdated);
        
        node_model_->clear();
        for (auto const& node : Node::items())
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
        
        QObject::connect(node_property_model_, &QStandardItemModel::itemChanged, this, &Editor::onNodePropertyUpdated);
    }


    void Editor::onNodePropertyUpdated()
    {
        QModelIndex index = node_property_model_->index(1, 1);    
        QString name = node_property_model_->data(index, Qt::DisplayRole).toString();
        
        index = node_property_model_->index(2, 1);    
        QString stage = node_property_model_->data(index, Qt::DisplayRole).toString();
        
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
        QString name = node_model_->data(index, Qt::DisplayRole).toString();
        
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
        if (project_filename_.isEmpty())
        {
            project_filename_ = QFileDialog::getSaveFileName(this,tr("Save"), "", tr("Piper project (*.piper);;All Files (*)"));
        }
        writeProjectFile(project_filename_);
    }


    void Editor::onLoad()
    {
        project_filename_ = QFileDialog::getOpenFileName(this,tr("Load"), "", tr("Piper project (*.piper);;All Files (*)"));
        loadProjectFile(project_filename_);
    }


    void Editor::onExport()
    {
        if (export_backend_ == nullptr)
        {
            qDebug() << "No export backend set. Aborting";
            return;
        }
        
        QString filename = QFileDialog::getSaveFileName(this,tr("Export"), "", tr("All Files (*)"));
        export_backend_->init(filename);
        
        // Export stages
        for (int i = 0; i < stage_model_->rowCount(); ++i)
        {
            export_backend_->writeStage(stage_model_->item(i, 0)->data(Qt::DisplayRole).toString());
        }
        
        // Export nodes
        for (auto const& node : Node::items())
        {
            export_backend_->writeNodeMetadata(node->nodeType(), node->name(), node->stage());
            
            // Export node's attributes
            for (auto const& attr: node->attributes())
            {
                export_backend_->writeNodeAttribute(node->name(), attr->name(), attr->data());
            }
        }
        
        // Export links
        for (auto const& link : Link::items())
        {
            Node* from = static_cast<Node*>(link->from()->parentItem());
            Node* to = static_cast<Node*>(link->to()->parentItem());
            export_backend_->writeLink(from->name(), link->from()->name(), to->name(), link->to()->name());
        }   
    }
    
    
    void Editor::writeProjectFile(QString const& filename)
    {
        QFile file(filename);
        file.open(QIODevice::WriteOnly | QIODevice::Truncate);
        
        QDataStream out(&file);
        
        // save stages
        out << stage_model_->rowCount();
        for (int i = 0; i < stage_model_->rowCount(); ++i)
        {
            out << *stage_model_->item(i, 0);
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
        stage_model_->clear();
        for (int i = 0; i < row; ++i)
        {
            QStandardItem* item = new QStandardItem();
            in >> *item;
            stage_model_->setItem(i, item);
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
