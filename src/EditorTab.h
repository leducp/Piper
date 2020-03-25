#ifndef PIPER_TAB_H
#define PIPER_TAB_H

#include <QTabWidget>
#include <QLineEdit>
#include <QValidator>

namespace piper
{
    class TabNameValidator : public QValidator
    {
        Q_OBJECT
        
    public:
        TabNameValidator(QObject* parent = nullptr);
        QValidator::State validate(QString& input, int& pos) const override;
        void fixup(QString& input) const override;
        
        void addEditor(QLineEdit* editor);
        void removeEditor(QLineEdit* editor);
        
    private:
        QList<QLineEdit*> editors_;
    };
    
    
    class EditorTab : public QTabWidget
    {
        Q_OBJECT
    public:
        EditorTab(QWidget* parent = nullptr);
        virtual ~EditorTab() = default;
        
    public slots:
        void createNewEditorTab();
        void closeEditorTab(int32_t index);
        void tabNameEdited();
        
    private:
        TabNameValidator* tabNameValidator_;
    };
    
}

#endif
