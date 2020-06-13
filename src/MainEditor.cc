#include "MainEditor.h"
#include "ui_MainEditor.h"
#include "EditorWidget.h"
#include "JsonExport.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QJsonParseError>

namespace piper
{
    MainEditor::MainEditor(QWidget* parent)
        : QMainWindow(parent)
        , ui_(new Ui::MainEditor)
    {
        ui_->setupUi(this);
        ui_->editor_tab->createNewEditorTab();

        QObject::connect(ui_->actionsave,      &QAction::triggered, this, &MainEditor::onSave);
        QObject::connect(ui_->actionsave_on,   &QAction::triggered, this, &MainEditor::onSaveOn);
        QObject::connect(ui_->actionload,      &QAction::triggered, this, &MainEditor::onLoad);
        QObject::connect(ui_->actionexport,    &QAction::triggered, this, &MainEditor::onExport);
        QObject::connect(ui_->actionshow,      &QAction::triggered, this, &MainEditor::onShowHelp);
        QObject::connect(ui_->actionload_json, &QAction::triggered, this, &MainEditor::onLoadJson);
    }


    void MainEditor::onSaveOn()
    {
        QString filename = QFileDialog::getSaveFileName(this,tr("Save"), "", tr("Piper project (*.piper);;All Files (*)"));
        if (filename.isEmpty())
        {
            return; // nothing to do: user abort.
        }

        writeProjectFile(filename);
    }


    void MainEditor::onSave()
    {
        if (project_filename_.isEmpty())
        {
            project_filename_ = QFileDialog::getSaveFileName(this,tr("Save"), "", tr("Piper project (*.piper);;All Files (*)"));
        }

        if (project_filename_.isEmpty())
        {
            return; // nothing to do: user abort.
        }

        writeProjectFile(project_filename_);
    }


    void MainEditor::onLoad()
    {
        project_filename_ = QFileDialog::getOpenFileName(this,tr("Load"), "", tr("Piper project (*.piper);;All Files (*)"));
        if (project_filename_.isEmpty())
        {
            return; // nothing to do: user abort.
        }

        loadProjectFile(project_filename_);
    }


    void MainEditor::onExport()
    {
        QString filename = QFileDialog::getSaveFileName(this,tr("Export"), "", tr("JSON (*.json);;All Files (*)"));
        if (filename.isEmpty())
        {
            return; // nothing to do: user abort.
        }

        JsonExport backend;
        backend.init(filename);

        for (int i = 0; i < ui_->editor_tab->count(); ++i)
        {
            QString pipeline = ui_->editor_tab->name(i);
            backend.startPipeline(pipeline);

            // Export tab content.
            EditorWidget* editor = static_cast<EditorWidget*>(ui_->editor_tab->widget(i));
            editor->onExport(backend);

            backend.endPipeline(pipeline);
        }

        backend.finalize(filename);
    }


    void MainEditor::onShowHelp()
    {
        QMessageBox msgBox;
        msgBox.setText("Press \"=\" to add a new step.");
        msgBox.exec();
    }


    void MainEditor::onLoadJson()
    {
        QString jsonFile = QFileDialog::getOpenFileName(this,tr("Load"), "", tr("Piper json (*.json);;All Files (*)"));
        if (jsonFile.isEmpty())
        {
            return; // nothing to do: user abort.
        }

        loadJson(jsonFile);
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
        int tab_count;
        in >> tab_count;
        for (int i = 0; i < tab_count; ++i)
        {
            EditorWidget* editor = ui_->editor_tab->createNewEditorTab();
            QString tab_name;
            in >> tab_name >> *editor;
            ui_->editor_tab->setName(i, tab_name);
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
    }


    void MainEditor::loadJson(const QString& filename)
    {
        QFile file(filename);
        file.open(QIODevice::ReadOnly | QIODevice::Text);

        QByteArray dataJson = file.readAll();
        file.close();

        QJsonParseError errorPtr;
        QJsonDocument doc = QJsonDocument::fromJson(dataJson, &errorPtr);
        if (doc.isNull()) {
            qDebug() << "Parse failed of " << filename;
        }

        QJsonObject rootObj = doc.object();

        qDebug() << "size " << rootObj.size();
        qDebug() << rootObj.keys();

        // reset editor - clear tabs
        while (ui_->editor_tab->count())
        {
            ui_->editor_tab->removeTab(ui_->editor_tab->currentIndex());
        }


        int pipelineNumber = rootObj.size();
        for (int i = 0; i < pipelineNumber; ++i)
        {
            EditorWidget* editor = ui_->editor_tab->createNewEditorTab();
            ui_->editor_tab->setName(i, rootObj.keys()[i]);

            QJsonObject pipelineJson = rootObj[rootObj.keys()[i]].toObject();
            editor->loadJson(pipelineJson);
        }
    }
}
