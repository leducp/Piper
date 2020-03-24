#include "MainEditor.h"
#include "ui_MainEditor.h"


namespace piper
{
    MainEditor::MainEditor(QWidget* parent)
        : QMainWindow(parent)
        , ui_(new Ui::MainEditor)
    {
        ui_->setupUi(this);
    }
}
