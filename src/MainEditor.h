#ifndef PIPER_MAIN_EDITOR_H
#define PIPER_MAIN_EDITOR_H

#include <QMainWindow>

namespace Ui 
{
    class MainEditor;
}

namespace piper
{
    class MainEditor : public QMainWindow
    {
        Q_OBJECT

    public:
        explicit MainEditor(QWidget* parent = nullptr);
        virtual ~MainEditor() = default;

    private:
        Ui::MainEditor* ui_;
    };
}

#endif
