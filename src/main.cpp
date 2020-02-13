#include "Example.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    Example w;
    w.show();

    return app.exec();
}

