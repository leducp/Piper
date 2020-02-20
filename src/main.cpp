#include "Editor.h"
#include "JsonExport.h"
#include "NodeCreator.h"

#include <QApplication>

using namespace piper;

int main(int argc, char *argv[])
{
    // Create node types for instance
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
        {"testA", "string", AttributeInfo::Type::member},
        {"testB", "int", AttributeInfo::Type::member},
        {"testC", "float", AttributeInfo::Type::member}
    });
    
    QApplication app(argc, argv);
    piper::JsonExport jsonExport;
    piper::Editor ed(nullptr, &jsonExport);
    ed.show();

    return app.exec();
}

