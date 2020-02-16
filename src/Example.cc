#include "Example.h"
//#include "ui_node.h"

#include "Node.h"
#include "Link.h"
#include "NodeCreator.h"

#include <QDebug>
#include <QPushButton>
#include <QColorDialog>


void Example::addStage()
{
    qDebug() << "Add stage !";
    
    // Add item
    QStandardItem* item = new QStandardItem();
    item->setData(QColor("red"), Qt::DecorationRole);
    model_->appendRow(item);
    
    // Enable item selection and put it edit mode
    QModelIndex index = model_->indexFromItem(item);
    ui_.stages->setCurrentIndex(index);
    ui_.stages->edit(index);
}


void Example::rmStage()
{
    int row = ui_.stages->currentIndex().row();
    model_->removeRows(row, 1);
    
    stagesUpdated();
}


void Example::colorStage()
{
    QModelIndex index = ui_.stages->currentIndex();
    QColor current = model_->data(index, Qt::DecorationRole).value<QColor>();
    
    QColor newColor = QColorDialog::getColor(current);
    model_->setData(index, newColor, Qt::DecorationRole);
}


void Example::stagesUpdated()
{
    int row = 0;
    QModelIndex index = model_->index(row, 0);

    Node::resetStagesColor();
    while (index.isValid())
    {
        QString stage = model_->data(index, Qt::DisplayRole).toString();   
        QColor color = model_->data(index, Qt::DecorationRole).value<QColor>();
        Node::updateStagesColor(stage, color);
    
        ++row;
        index = model_->index(row, 0);
    }
}


Example::Example(QWidget *parent) 
    : QMainWindow(parent)
{
    ui_.setupUi(this);
    scene_ = new Scene (this);
    ui_.view->setScene(scene_);
    
    QPixmap pixmap(100,100);
    pixmap.fill(QColor("red"));
    QIcon redIcon(pixmap);

    QObject::connect(ui_.stage_add,   &QPushButton::clicked, this, &Example::addStage);
    QObject::connect(ui_.stage_rm,    &QPushButton::clicked, this, &Example::rmStage);
    QObject::connect(ui_.stage_color, &QPushButton::clicked, this, &Example::colorStage);
    //QObject::connect(ui_.stage_up,   &QPushButton::clicked, this, &Example::addStage);
    //QObject::connect(ui_.stage_down, &QPushButton::clicked, this, &Example::addStage);
    
    model_ = new QStandardItemModel(this);
    model_->insertColumns(0, 1);
    QObject::connect(model_, &QStandardItemModel::itemChanged, this, &Example::stagesUpdated);

    ui_.stages->setModel(model_);
    ui_.stages->setEditTriggers(QAbstractItemView::AnyKeyPressed |
                                QAbstractItemView::DoubleClicked);
    ui_.stages->setDragDropMode(QAbstractItemView::InternalMove);
    
    NodeCreator creator;
    creator.addItem("PID", 
                    { 
                        {"target", "Kinematic", AttributeInfo::Type::input}, 
                        {"measurements", "Kinematic", AttributeInfo::Type::input},
                        {"output", "torque", AttributeInfo::Type::output},
                        {"Kp", "float", AttributeInfo::Type::member},
                        {"Ki", "float", AttributeInfo::Type::member},
                        {"Kd", "float", AttributeInfo::Type::member}
                    });
    creator.addItem("SimpleTransmission", 
                    { 
                        {"input", "torque", AttributeInfo::Type::input}, 
                        {"output", "torque", AttributeInfo::Type::output},
                        {"zero", "float", AttributeInfo::Type::member}
                    });
    
    Node* item;
    item = creator.createItem("PID", "PID", "controller", {});
    scene_->addItem(item);
    item = creator.createItem("SimpleTransmission", "jointToMotor", "tr", {});
    scene_->addItem(item);
}
