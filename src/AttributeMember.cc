#include "AttributeMember.h"

#include <QDebug>
#include <QLineEdit>
#include <QSpinBox>

namespace piper
{    
    MemberForm::MemberForm(QGraphicsItem* parent, QVariant& data, QRectF const& boundingRect, QBrush const& brush)
        : QGraphicsProxyWidget(parent) 
        , data_{data}
        , boundingRect_{boundingRect}
        , brush_{brush}
    {

    }
    
    void MemberForm::paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget)
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(brush_);
        painter->drawRoundedRect(boundingRect_, 8, 8);
        
        QGraphicsProxyWidget::paint(painter, option, widget);
    }
    
    void MemberForm::onDataUpdated(QVariant const& data)
    {
        data_ = data;
    }

    
    AttributeMember::AttributeMember(QGraphicsItem* parent, const QString& name, const QString& dataType, const QRect& boundingRect)
        : Attribute (parent, name, dataType, boundingRect)
    {
        // Reduce the label area to add the form.
        labelRect_ = QRectF{boundingRect_.left() + 15, boundingRect_.top(), 
                            boundingRect_.width() / 3, boundingRect_.height()};
        
        // Construct the form (area, backgorund color, widget, widgets options etc).
        QRectF formRect{0, 0, boundingRect_.width() * 2 / 3 - 20, boundingRect_.height() - 10};
        QBrush brush {{180, 180, 180, 255}, Qt::SolidPattern};
        form_ = new MemberForm(this, data_, formRect, brush);
        
        QWidget* widget = createWidget();
        if (widget != nullptr)
        {
            widget->setFont(normalFont_);
            widget->setMaximumSize(formRect.width(), formRect.height());
            widget->resize(formRect.width(), formRect.height());
            widget->setStyleSheet("* { background-color: rgba(0, 0, 0, 0); }"); 
            form_->setWidget(widget);
        }
        form_->setPos(labelRect_.right(), labelRect_.top() + 5);
    }
    
    void AttributeMember::setData(QVariant const& data)
    {
        switch (data.type())
        {
            case QVariant::Type::Int:
            {
                form_->dataUpdated(data.toInt());
                break;
            }
            case QVariant::Type::Double:
            {
                form_->dataUpdated(data.toDouble());
                break;
            }
            case QVariant::Type::String:
            {
                form_->dataUpdated(data.toString());
                break;
            }
            default:
            {
                qDebug() << "Incompatible type: " << data << ". Do nothing";
            }
        }
    }
    
    QWidget* AttributeMember::createWidget()
    {
        QStringList supportedTypes;
        
        supportedTypes << "int" << "integer";
        if (supportedTypes.contains(dataType_))
        {
            QSpinBox* box = new QSpinBox();
            box->setMaximum(std::numeric_limits<int>::max());
            box->setMinimum(std::numeric_limits<int>::min());
            QObject::connect(box, QOverload<int>::of(&QSpinBox::valueChanged), form_, &MemberForm::onDataUpdated);
            QObject::connect(form_, SIGNAL(dataUpdated(int)), box, SLOT(setValue(int)));
            return box;
        }
        
        supportedTypes.clear();
        supportedTypes << "float" << "double" << "real";
        if (supportedTypes.contains(dataType_))
        {
            QDoubleSpinBox* box = new QDoubleSpinBox();
            box->setMaximum(std::numeric_limits<double>::max());
            box->setMinimum(std::numeric_limits<double>::min());
            box->setDecimals(3);
            QObject::connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged), form_, &MemberForm::onDataUpdated);
            QObject::connect(form_, SIGNAL(dataUpdated(double)), box, SLOT(setValue(double)));
            return box;
        }
        
        supportedTypes.clear();
        supportedTypes << "string";
        if (supportedTypes.contains(dataType_))
        {
            QLineEdit* lineEdit = new QLineEdit();
            lineEdit->setFont(normalFont_);
            QObject::connect(lineEdit, &QLineEdit::textChanged, form_, &MemberForm::onDataUpdated);
            QObject::connect(form_, SIGNAL(dataUpdated(QString const&)), lineEdit, SLOT(setText(QString const&)));
            return lineEdit;
        }
        
        return nullptr;
    }
}
