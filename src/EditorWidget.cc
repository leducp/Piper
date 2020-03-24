#include "EditorWidget.h"
#include "ui_EditorWidget.h"
#include "Scene.h" 


namespace piper
{
    EditorWidget::EditorWidget(QWidget* parent)
        : QWidget(parent)
        , ui_(new Ui::EditorWidget)
        , scene_(new Scene(this))
    {
        ui_->setupUi(this);
        ui_->view->setScene(scene_);
    }
}
