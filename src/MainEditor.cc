#include "MainEditor.h"
#include "ui_MainEditor.h"
#include "EditorWidget.h"

#include <QFileDialog>

namespace piper
{
    MainEditor::MainEditor(QWidget* parent)
        : QMainWindow(parent)
        , ui_(new Ui::MainEditor)
    {
        ui_->setupUi(this);
        ui_->editor_tab->createNewEditorTab();
        
        QObject::connect(ui_->actionsave,    &QAction::triggered, this, &MainEditor::onSave);
        QObject::connect(ui_->actionsave_on, &QAction::triggered, this, &MainEditor::onSaveOn);
        QObject::connect(ui_->actionload,    &QAction::triggered, this, &MainEditor::onLoad);
        QObject::connect(ui_->actionexport,  &QAction::triggered, this, &MainEditor::onExport);
    }
    
    
    void MainEditor::onSaveOn()
    {
        QString filename = QFileDialog::getSaveFileName(this,tr("Save"), "", tr("Piper project (*.piper);;All Files (*)"));
        writeProjectFile(filename);
    }


    void MainEditor::onSave()
    {
        if (project_filename_.isEmpty())
        {
            project_filename_ = QFileDialog::getSaveFileName(this,tr("Save"), "", tr("Piper project (*.piper);;All Files (*)"));
        }
        writeProjectFile(project_filename_);
    }


    void MainEditor::onLoad()
    {
        project_filename_ = QFileDialog::getOpenFileName(this,tr("Load"), "", tr("Piper project (*.piper);;All Files (*)"));
        loadProjectFile(project_filename_);
    }
    
    
    void MainEditor::onExport()
    {
    }
    
    
    void MainEditor::loadProjectFile(const QString& filename)
    {
        QFile file(filename);
        file.open(QIODevice::ReadOnly);

        QDataStream in(&file);
        
        // reset editor - clear tabs
        while (ui_->editor_tab->count())
        {
            ui_->editor_tab->removeTab(ui_->editor_tab->currentIndex());
        }
        

        // load tab
        int tabCount;
        in >> tabCount;
        for (int i = 0; i < tabCount; ++i)
        {
            EditorWidget* editor = ui_->editor_tab->createNewEditorTab();
            QString tabName;
            in >> tabName >> *editor;
            ui_->editor_tab->setName(i, tabName);
        }

        /*

        // load nodes

        int nodes;
        in >> nodes;
        for (int i = 0; i < nodes; ++i)
        {


            Node* item = NodeCreator::instance().createItem(type, name, stage, pos);
            for (auto const& attr: item->attributes())
            {
               if (attributes.contains(attr->name()))
               {
                   attr->setData(attributes.value(attr->name()));
               }
            }

            scene_->addItem(item);
        }

        // load links
        int links;
        in >> links;
        for (int i = 0; i < links; ++i)
        {
            QString from, output;
            in >> from >> output;

            QString to, input;
            in >> to >> input;

            Link* link = piper::connect(from, output, to, input);
            scene_->addItem(link);
        }
        */

        // update display
        for (int i = 0; i < ui_->editor_tab->count(); ++i)
        {
            EditorWidget* editor = static_cast<EditorWidget*>(ui_->editor_tab->widget(i));
            editor->onStageUpdated();
        }
    }
    
    
    void MainEditor::writeProjectFile(const QString& filename)
    {
        QFile file(filename);
        file.open(QIODevice::WriteOnly | QIODevice::Truncate);

        QDataStream out(&file);
        
        // Save tab number
        out << ui_->editor_tab->count();
        for (int i = 0; i < ui_->editor_tab->count(); ++i)
        {
            // Save tab name.
            out << ui_->editor_tab->name(i);
            
            // Save tab content.
            EditorWidget* editor = static_cast<EditorWidget*>(ui_->editor_tab->widget(i));
            out << *editor;
        }

        /*
        // save links
        out << Link::items().size();
        for (auto const& link : Link::items())
        {
            Node* from = static_cast<Node*>(link->from()->parentItem());
            out << from->name() << link->from()->name();

            Node* to = static_cast<Node*>(link->to()->parentItem());
            out << to->name() << link->to()->name();
        }
        */
    }
}
