#include "Editor.h"
#include "PropertyDelegate.h"
#include "ui_editor.h"

#include "Node.h"
#include "Link.h"
#include "NodeCreator.h"

#include <cmath>
#include <QDebug>
#include <QPushButton>
#include <QColorDialog>
#include <QFileDialog>

namespace piper
{
    Editor::Editor(QWidget *parent, ExportBackend* exportBackend)
        : QMainWindow(parent)
        , ui_(new Ui::Editor)
        , export_backend_{exportBackend}
    {

    }


    void Editor::onExport()
    {
        if (export_backend_ == nullptr)
        {
            qDebug() << "No export backend set. Aborting";
            return;
        }

        QString filename = QFileDialog::getSaveFileName(this,tr("Export"), "", tr("All Files (*)"));
        if (filename.isEmpty())
        {
            return;
        }

        export_backend_->init(filename);

        // Export stages
        for (int i = 0; i < stage_model_->rowCount(); ++i)
        {
            export_backend_->writeStage(stage_model_->item(i, 0)->data(Qt::DisplayRole).toString());
        }

        // Export nodes
        for (auto const& node : Node::items())
        {
            export_backend_->writeNodeMetadata(node->nodeType(), node->name(), node->stage());

            // Export node's attributes
            for (auto const& attr: node->attributes())
            {
                export_backend_->writeNodeAttribute(node->name(), attr->name(), attr->data());
            }
        }

        // Export links
        for (auto const& link : Link::items())
        {
            Node* from = static_cast<Node*>(link->from()->parentItem());
            Node* to = static_cast<Node*>(link->to()->parentItem());
            export_backend_->writeLink(from->name(), link->from()->name(), to->name(), link->to()->name());
        }

        export_backend_->finalize();
    }
}
