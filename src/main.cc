#include "MainEditor.h"
#include "NodeCreator.h"
#include <QApplication>

using namespace piper;

int main(int argc, char *argv[])
{
    // Create node types for instance
    NodeCreator::instance().addItem("PID",
    {
        {"target", "kinematic", AttributeInfo::Type::input},
        {"measurements", "kinematic", AttributeInfo::Type::input},
        {"output", "torque", AttributeInfo::Type::output},
        {"Kp", "float", AttributeInfo::Type::member},
        {"Ki", "float", AttributeInfo::Type::member},
        {"Kd", "float", AttributeInfo::Type::member}
    });

    NodeCreator::instance().addItem("ExoskeletonSimpleTransmission",
    {
        {"input", "torque", AttributeInfo::Type::input},
        {"output", "torque", AttributeInfo::Type::output},
        {"zero", "float", AttributeInfo::Type::member}
    });

    NodeCreator::instance().addItem("TestingNode",
    {
        {"inputA", "torque", AttributeInfo::Type::input},
        {"inputB", "kinematic", AttributeInfo::Type::input},
        {"outputA", "torque", AttributeInfo::Type::output},
        {"outputB", "kinematic", AttributeInfo::Type::output},
        {"testA", "string", AttributeInfo::Type::member},
        {"testB", "int", AttributeInfo::Type::member},
        {"testC", "float", AttributeInfo::Type::member}
    });

    NodeCreator::instance().addItem("Motor",
    {
        {"input", "torque", AttributeInfo::Type::input},
        {"output", "torque", AttributeInfo::Type::output}
    });

    QApplication app(argc, argv);
    Q_INIT_RESOURCE(resources);
    MainEditor editor;
    editor.show();

    return app.exec();
}
