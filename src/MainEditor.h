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
        
    public slots:
        void onSave();
        void onSaveOn();
        void onLoad();
        void onExport();

    private:
        void writeProjectFile(QString const& filename);
        void loadProjectFile(QString const& filename);
        
        Ui::MainEditor* ui_;
        QString project_filename_;
    };
}

#endif
