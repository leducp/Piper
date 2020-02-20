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
        // init() is called befre anything else.
        virtual void init(QString const& filename) = 0;
        
        // Stages are written from first to last.
        virtual void writeStage(QString const& stage) = 0;
        
        // Each node is composed from one call for the metadata, and possible multiples call for its attributes
        virtual void writeNodeMetadata(QString const& type, QString const& name, QString const& stage) = 0;
        virtual void writeNodeAttribute(QString const& nodeName, QString const& name, QVariant const& data) = 0;
        
        // one call per link
        virtual void writeLink(QString const& from, QString const& output, QString const& to, QString const& input) = 0;
        
        // finalize() is called when the export is finished.
        virtual void finalize() = 0;
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
