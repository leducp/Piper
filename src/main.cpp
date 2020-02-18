#include "Editor.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    piper::Editor ed;
    ed.show();

    return app.exec();
}

