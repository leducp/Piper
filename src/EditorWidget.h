#ifndef PIPER_EDITOR_WIDGET_H
#define PIPER_EDITOR_WIDGET_H

#include <QWidget>

namespace Ui
{
    class EditorWidget;
}

namespace piper
{
    class Scene;
    
    class EditorWidget : public QWidget
    {
    public:
        EditorWidget(QWidget* parent = nullptr);
        virtual ~EditorWidget() = default;
        
    private:
        Ui::EditorWidget* ui_;
        Scene* scene_;
    };
    
}

#endif
