#include "node.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Node w;
    w.show();

    return app.exec();
}

