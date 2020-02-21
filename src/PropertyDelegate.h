#ifndef PIPER_PROPERTY_DELEGATE_H
#define PIPER_PROPERTY_DELEGATE_H

#include <QStyledItemDelegate>

class QStandardItemModel;

namespace piper
{
    class StagePropertyDelegate : public QStyledItemDelegate
    {
        Q_OBJECT
        
    public:
        QWidget* createEditor(QWidget *parent, const QStyleOptionViewItem &option, QModelIndex const& index) const override;
        void setEditorData(QWidget* editor, QModelIndex const& index) const override;
        void setModelData(QWidget* editor, QAbstractItemModel* model, QModelIndex const& index) const override;
        
        void setStageModel(QStandardItemModel const* stages) { stages_ = stages; }

    public slots:
        void onIndexChange();
        
    private:
        QStandardItemModel const* stages_;
    };
}

#endif

