#include "EditorTab.h"
#include "EditorWidget.h"

#include <QDebug>
#include <QTabBar> 
#include <QMouseEvent>
#include <QPushButton>

namespace piper
{

    TabNameValidator::TabNameValidator(QObject* parent) 
        : QValidator(parent) 
        , editors_{}
    { 
        
    }
    
    QValidator::State TabNameValidator::validate(QString& input, int& pos) const
    {
        for (auto const& c : input)
        {
            // Refuse non plain ascii name.
            if (c.unicode() > 127)
            {
                return QValidator::Invalid;
            }
        }
        
        QStringList forbiddenStrings;
        for (auto const& editor : editors_)
        {
            if (editor->hasFocus())
            {
                // Do not forbidden editor to write himself
                continue;
            }
            forbiddenStrings << editor->text();
        }
        
        // Refuse already defined name.
        if (forbiddenStrings.contains(input))
        {
            return QValidator::Intermediate;
        }
        
        return QValidator::Acceptable;
    }
        
        
    void TabNameValidator::fixup(QString& input) const
    {
        QStringList forbiddenStrings;
        for (auto const& editor : editors_)
        {
            if (editor->hasFocus())
            {
                // Do not forbidden editor to write himself
                continue;
            }
            forbiddenStrings << editor->text();
        }
        
        QString fixedInput = input;
        for (int32_t i = 0; forbiddenStrings.contains(fixedInput); ++i)
        {
            fixedInput = input + " (" + QString::number(i) + ")";
        }
        input = fixedInput;
    }
        
        
    void TabNameValidator::addEditor(QLineEdit* editor)
    {
        editors_.append(editor);
        emit changed();
    }
    
    
    void TabNameValidator::removeEditor(QLineEdit* editor)
    {
        editors_.removeAll(editor);
        emit changed();
    }

    
    class TabHeaderEdit : public QLineEdit
    {
    public:
        TabHeaderEdit(QWidget* parent = nullptr) : QLineEdit(parent) { }
        virtual ~TabHeaderEdit() = default;
        
        void mousePressEvent(QMouseEvent *event) override
        {
            QLineEdit::mousePressEvent(event);
            QWidget::mousePressEvent(event); // let the parent do its work too !
        }
    };
    
    EditorTab::EditorTab(QWidget* parent)
        : QTabWidget(parent)
        , tabNameValidator_(new TabNameValidator(this))
    {
        // Create adder button
        QPushButton* tb = new QPushButton();
        tb->setFlat(true);
        tb->setIcon(QIcon(":/icon/add_tab.svg"));
        tb->setStyleSheet("* { icon-size: 30px 30px; }");
        setCornerWidget(tb, Qt::TopLeftCorner);

        // Ensure that button will be always visible.
        setStyleSheet("QTabBar::tab { height: 40px; }");
        tb->setMinimumSize(40, 40);
        tabBar()->setMinimumSize(40, 40);
        
        // Connect signal to manage tab creation/rename/deletion
        QObject::connect(tb,   &QPushButton::clicked,            this, &EditorTab::createNewEditorTab);
        QObject::connect(this, &QTabWidget::tabCloseRequested,   this, &EditorTab::closeEditorTab);
    }
    
    
    void EditorTab::createNewEditorTab()
    {
        EditorWidget* editor = new EditorWidget();
        int32_t index = addTab(editor, "");
        
        QString defaultText = "unamed pipeline";
        tabNameValidator_->fixup(defaultText);
        
        QLineEdit* edit = new TabHeaderEdit();
        edit->setText(defaultText);
        edit->setFrame(false);
        edit->setStyleSheet("QLineEdit { background:transparent; }");
        edit->setValidator(tabNameValidator_);
        tabNameValidator_->addEditor(edit);
        tabBar()->setTabButton(index, QTabBar::LeftSide, edit);
        QObject::connect(edit, &QLineEdit::editingFinished, this, &EditorTab::tabNameEdited);
    }

    
    void EditorTab::closeEditorTab(int32_t index)
    {
        tabNameValidator_->removeEditor(static_cast<QLineEdit*>(tabBar()->tabButton(index, QTabBar::LeftSide)));
        delete widget(index); // Note: removeTab do not destroy the widget.
    }
    
    
    void EditorTab::tabNameEdited()
    {
        currentWidget()->setFocus();
    }
}
