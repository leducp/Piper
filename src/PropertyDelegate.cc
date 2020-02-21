#include "PropertyDelegate.h"

#include <QStandardItemModel>
#include <QComboBox>
#include <QDebug>

namespace piper
{
    QWidget* StagePropertyDelegate::createEditor(QWidget* parent, QStyleOptionViewItem const& option, QModelIndex const& index) const
    {
        QComboBox* cb = new QComboBox(parent);

        int row = 0;
        QModelIndex i = stages_->index(row, 0);
        while (i.isValid())
        {
            QString stage = stages_->data(i, Qt::DisplayRole).toString();   
            cb->addItem(stage);
        
            ++row;
            i = stages_->index(row, 0);
            qDebug() << i;
        }

        connect(cb, static_cast<void(QComboBox::*)(int)>(&QComboBox::currentIndexChanged),this, &StagePropertyDelegate::onIndexChange);
        return cb;
    }
    
    
    void StagePropertyDelegate::setEditorData(QWidget* editor, QModelIndex const& index) const
    {
        QComboBox* cb = qobject_cast<QComboBox*>(editor);
        QString value = index.data(Qt::EditRole).toString();
        int idx = cb->findText(value);
        if (idx >= 0)
        {
            cb->setCurrentIndex(idx);
        }

        cb->showPopup();
    }
    
    
    void StagePropertyDelegate::setModelData(QWidget* editor, QAbstractItemModel* model, QModelIndex const& index) const
    {
        QComboBox* cb = qobject_cast<QComboBox*>(editor);
        model->setData(index, cb->currentText(), Qt::EditRole);
    }

    
    void StagePropertyDelegate::onIndexChange()
    {
        QComboBox* cb = static_cast<QComboBox*>(sender());
        emit commitData(cb);
    }

}
