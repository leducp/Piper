#ifndef PIPER_TAB_H
#define PIPER_TAB_H

#include <QTabWidget>
#include <QLineEdit>
#include <QValidator>
#include <QDebug>

namespace piper
{    
    class TabHeaderEdit : public QLineEdit
    {
        Q_OBJECT
        
    public:
        TabHeaderEdit(QString const& text);
        virtual ~TabHeaderEdit() = default;
        
        void mousePressEvent(QMouseEvent *event) override;
        
    signals: 
        void getFocus(TabHeaderEdit* me);
    };
    
    
    class TabNameValidator : public QValidator
    {
        Q_OBJECT
        
    public:
        TabNameValidator(QObject* parent = nullptr);
        QValidator::State validate(QString& input, int& pos) const override;
        void fixup(QString& input) const override;
        
        void addEditor(TabHeaderEdit* editor);
        void removeEditor(TabHeaderEdit* editor);
        
    public slots:
        void updateHeaderEditFilter(TabHeaderEdit* editor) { filteredEditor_ = editor; }
        
    private:
        QList<TabHeaderEdit*> editors_;
        TabHeaderEdit* filteredEditor_{nullptr};
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
