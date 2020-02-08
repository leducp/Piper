#include "node.h"
#include "ui_node.h"

#include "NodeItem.h"
#include "NodePath.h"

Node::Node(QWidget *parent) 
    : QMainWindow(parent)
{
    ui_.setupUi(this);
    scene_ = new NodeScene(this);
    ui_.view->setScene(scene_);
    
    NodeItem* newNode = new NodeItem("Yolo");
    newNode->setPos(0, 0);
    newNode->addAttribute({"canard output", "torque", AttributeInfo::Type::output});
    scene_->addItem(newNode);
    
    newNode = new NodeItem("Yala");
    newNode->setPos(350, 0);
    newNode->addAttribute({"canard input", "torque", AttributeInfo::Type::input});
    newNode->addAttribute({"cygnus input", "middle", AttributeInfo::Type::input});
    scene_->addItem(newNode);
    
    newNode = new NodeItem("Cool");
    newNode->setPos(150, 150);
    (void) newNode->addAttribute({"data", "int", AttributeInfo::Type::member});
    scene_->addItem(newNode);
    /*
    NodePath* path = new NodePath;
    path->connectFrom(from);
    path->connectTo(to);
    path->updatePath();
    scene_->addItem(path);
    */
}
