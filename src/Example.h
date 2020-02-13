#ifndef NODE_H
#define NODE_H

#include <QMainWindow>
#include "Scene.h"
#include "ui_example.h"

using namespace piper;

class Example : public QMainWindow
{
    Q_OBJECT

public:
    explicit Example(QWidget *parent = nullptr);
    virtual ~Example() = default;

private:
    Ui::Example ui_;
    Scene* scene_;
};

#endif 
