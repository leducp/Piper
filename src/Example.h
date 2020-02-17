#ifndef NODE_H
#define NODE_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QPair>
#include "NodeCreator.h"
#include "Scene.h"
#include "ui_example.h"

using namespace piper;

class Example : public QMainWindow
{
    Q_OBJECT

public:
    explicit Example(QWidget *parent = nullptr);
    virtual ~Example() = default;
    
public slots:
    void addStage();
    void rmStage();
    void colorStage();
    void stagesUpdated();
    
    void nodesUpdated();
    void nodePropertyUpdated();
    void nodeSelected(QModelIndex const& index);
    
    void save();
    void load();

private:
    Ui::Example ui_;
    Scene* scene_;
    QStandardItemModel* stageModel_;
    QStandardItemModel* nodeModel_;
    QStandardItemModel* nodePropertyModel_;
    
    NodeCreator creator_;
};

#endif 
