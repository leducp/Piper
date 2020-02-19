#ifndef PIPER_EDITOR_H
#define PIPER_EDITOR_H

#include <QStandardItemModel>

#include "Scene.h"
#include "ui_editor.h"


namespace piper
{
    class Node;
    class Link;
    
    class ExportBackend
    {
    public:
        virtual void init(QString const& filename) = 0;
        virtual void exportData(QStringList const& stages, QList<Node*> const& nodes, QList<Link*> const& links) = 0;
    };


    class Editor : public QMainWindow
    {
        Q_OBJECT

    public:
        explicit Editor(QWidget* parent = nullptr, ExportBackend* exportBackend = nullptr);
        virtual ~Editor() = default;

    public slots:
        void onAddStage();
        void onRmStage();
        void onColorStage();
        void onStageUpdated();
        
        void onNodeUpdated();
        void onNodePropertyUpdated();
        void onNodeSelected(QModelIndex const& index);
        
        void onSave();
        void onSaveOn();
        void onLoad();
        void onExport();

    private:
        void writeProjectFile(QString const& filename);
        void loadProjectFile(QString const& filename);
        
        Ui::Editor ui_;
        Scene* scene_;
        QString projectFile_;
        QStandardItemModel* stageModel_;
        QStandardItemModel* nodeModel_;
        QStandardItemModel* nodePropertyModel_;
        
        ExportBackend* exportBackend_;
    };
}

#endif 
