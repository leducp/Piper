#include "MainEditor.h"
#include "NodeCreator.h"
#include "ThemeManager.h"
#include <QApplication>

using namespace piper;

int main(int argc, char *argv[])
{
    // Create node types for instance
    NodeCreator::instance().addItem("SinWave",
    {
        {"output", "float", AttributeInfo::Type::output},
        {"amplitude", "float", AttributeInfo::Type::member},
        {"frequency", "float", AttributeInfo::Type::member},
    });

    NodeCreator::instance().addItem("Random",
    {
        {"output", "float", AttributeInfo::Type::output},
        {"min", "float", AttributeInfo::Type::member},
        {"max", "float", AttributeInfo::Type::member},
    });

    NodeCreator::instance().addItem("Add",
    {
        {"inputA", "float", AttributeInfo::Type::input},
        {"inputB", "float", AttributeInfo::Type::input},
        {"output", "float", AttributeInfo::Type::output},
    });

    NodeCreator::instance().addItem("LowPass",
    {
        {"inputA", "float", AttributeInfo::Type::input},
        {"output", "float", AttributeInfo::Type::output},
        {"Fc", "float", AttributeInfo::Type::member},
    });

    NodeCreator::instance().addItem("cast<float, int>",
    {
        {"input", "float", AttributeInfo::Type::input},
        {"output", "int", AttributeInfo::Type::output}
    });

    NodeCreator::instance().addItem("cast<float, customType>",
    {
        {"input", "float", AttributeInfo::Type::input},
        {"output", "customType", AttributeInfo::Type::output}
    });

    NodeCreator::instance().addItem("probe<float>",
    {
        {"input", "float", AttributeInfo::Type::input},
    });

    NodeCreator::instance().addItem("probe<int>",
    {
        {"input", "int", AttributeInfo::Type::input},
    });

    NodeCreator::instance().addItem("probe<customType>",
    {
        {"input", "customType", AttributeInfo::Type::input},
    });

    // Load theme
    if (not ThemeManager::instance().load("data/theme.json"))
    {
        return 1;
    }

    QApplication app(argc, argv);
    Q_INIT_RESOURCE(resources);
    MainEditor editor;
    editor.show();

    return app.exec();
}
