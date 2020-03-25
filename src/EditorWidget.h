#ifndef PIPER_EDITOR_WIDGET_H
#define PIPER_EDITOR_WIDGET_H

#include <QWidget>
#include <QStandardItemModel>

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
        
    public slots:
        void onAddStage();
        void onRmStage();
        void onColorStage();
        void onStageUpdated();

        void onNodeUpdated();
        void onNodePropertyUpdated();
        void onNodeSelected(QModelIndex const& index);
        
    private:
        Ui::EditorWidget* ui_;
        Scene* scene_;
        
        QStandardItemModel* stage_model_;
        QStandardItemModel* node_model_;
        QStandardItemModel* node_property_model_;
    };
    
}

#endif
