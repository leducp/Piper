#include "node.h"
#include "ui_node.h"

#include "NodeItem.h"
#include "NodePath.h"
#include "NodeCreator.h"

Node::Node(QWidget *parent) 
    : QMainWindow(parent)
{
    ui_.setupUi(this);
    scene_ = new NodeScene(this);
    ui_.view->setScene(scene_);
    
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
    
    NodeItem* item;
    item = creator.createItem("PID", "PID", "controller", {});
    scene_->addItem(item);
    item = creator.createItem("SimpleTransmission", "jointToMotor", "tr", {});
    scene_->addItem(item);
    
    /*
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
    */
}
