#ifndef PIPER_TAB_H
#define PIPER_TAB_H

#include <QTabWidget>
#include <QLineEdit>
#include <QValidator>
#include <QDebug>

namespace piper
{
    class EditorWidget;
    
    class TabHeaderEdit : public QLineEdit
    {
        Q_OBJECT
        
    public:
        TabHeaderEdit(QString const& text);
        virtual ~TabHeaderEdit() = default;
        
        void mousePressEvent(QMouseEvent *event) override;
    };
    
    
    class TabNameValidator : public QValidator
    {
        Q_OBJECT
        
    public:
        TabNameValidator(QObject* parent = nullptr);
        QValidator::State validate(QString& input, int& pos) const override;
    };
    
    
    class EditorTab : public QTabWidget
    {
        Q_OBJECT
    public:
        EditorTab(QWidget* parent = nullptr);
        virtual ~EditorTab() = default;
        
        QString name(int32_t index) const;
        void setName(int32_t index, QString const& name);
        
    public slots:
        EditorWidget* createNewEditorTab();
        void closeEditorTab(int32_t index);
        void tabNameEdited();
        
    private:
        TabNameValidator* tabNameValidator_;
    };
}

#endif
