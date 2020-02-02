#include "node.h"
#include "ui_node.h"

#include "NodeItem.h"

Node::Node(QWidget *parent) 
    : QMainWindow(parent)
{
    ui_.setupUi(this);
    scene_ = new NodeScene(this);
    ui_.view->setScene(scene_);
    
    scene_->addItem(new NodeItem);
}
