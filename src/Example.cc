#include "Example.h"
//#include "ui_node.h"

#include "Node.h"
#include "Link.h"
#include "NodeCreator.h"

Example::Example(QWidget *parent) 
    : QMainWindow(parent)
{
    ui_.setupUi(this);
    scene_ = new Scene (this);
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
    
    Node* item;
    item = creator.createItem("PID", "PID", "controller", {});
    scene_->addItem(item);
    item = creator.createItem("SimpleTransmission", "jointToMotor", "tr", {});
    scene_->addItem(item);
}
