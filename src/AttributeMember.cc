#include "AttributeMember.h"
#include "Scene.h"

#include <QDebug>
#include <QLineEdit>
#include <QSpinBox>

namespace piper
{
    MemberForm::MemberForm(QGraphicsItem* parent, QVariant& data, QRectF const& boundingRect, QBrush const& brush)
        : QGraphicsProxyWidget(parent)
        , data_{data}
        , bounding_rect_{boundingRect}
        , brush_{brush}
    {
    }

    void MemberForm::updateFormWidth(qreal width)
    {
        bounding_rect_.setWidth(width);
        widget()->resize(static_cast<int>(width), static_cast<int>(bounding_rect_.height()));
    }

    void MemberForm::paint(QPainter* painter, QStyleOptionGraphicsItem const* option, QWidget* widget)
    {
        painter->setPen(Qt::NoPen);
        painter->setBrush(brush_);
        painter->drawRoundedRect(bounding_rect_, 8, 8);

        QGraphicsProxyWidget::paint(painter, option, widget);
    }

    void MemberForm::mousePressEvent(QGraphicsSceneMouseEvent* event)
    {
        (void)event;
        Scene* pScene = static_cast<Scene*>(scene());

        for (auto& item : pScene->selectedItems())
        {
            item->setSelected(false);
        }
    }

    void MemberForm::onDataUpdated(QVariant const& data)
    {
        data_                   = data;
        int width               = widget()->fontMetrics().boundingRect(data.toString()).width();
        AttributeMember* member = qgraphicsitem_cast<AttributeMember*>(parentItem());
        member->setFormBaseWidth(static_cast<qreal>(width));
    }


    AttributeMember::AttributeMember(QGraphicsItem* parent, AttributeInfo const& info, const QRect& boundingRect)
        : Attribute(parent, info, boundingRect)
    {
        // Construct the form (area, background color, widget, widgets options etc).
        QRectF formRect{0, 0, bounding_rect_.width(), bounding_rect_.height() - 10};
        QBrush brush{{180, 180, 180, 255}, Qt::SolidPattern};
        form_ = new MemberForm(this, data_, formRect, brush);
        baseWidth_ = 0;

        QWidget* widget = createWidget();
        if (widget != nullptr)
        {
            widget->setFont(normal_font_);
            widget->resize(static_cast<int>(formRect.width()), static_cast<int>(formRect.height()));

            QFile File(":/style.qss");
            File.open(QFile::ReadOnly);
            QString StyleSheet = QLatin1String(File.readAll());
            widget->setStyleSheet(StyleSheet);
            form_->setWidget(widget);
        }
        form_->setPos(boundingRect.right() - formRect.width(), label_rect_.top() + 5);
    }

    void AttributeMember::setFormBaseWidth(qreal width)
    {
        baseWidth_ = width;
    }

    void AttributeMember::updateRectSize(QRectF rectangle)
    {
        bounding_rect_   = rectangle;
        background_rect_ = bounding_rect_;
        form_->updateFormWidth(bounding_rect_.width() - label_rect_.width() - 20);
        form_->setPos(bounding_rect_.right() - form_->boundingRect().width(), label_rect_.top() + 5);
    }

    void AttributeMember::setData(QVariant const& data)
    {
        switch (data.type())
        {
            //QJsonValue always returns QVariant::Double with numbers
            //send both signals, the widget with corresponding type will receive values
            case QVariant::Int:
            case QVariant::Double:
            {
                form_->dataUpdated(data.toInt());
                form_->dataUpdated(data.toDouble());
                break;
            }
            case QVariant::String:
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

        supportedTypes << "int"
                       << "integer"
                       << "int32_t"
                       << "int64_t";
        if (supportedTypes.contains(dataType()))
        {
            QSpinBox* box = new QSpinBox();
            data_         = box->value();
            box->setMaximum(std::numeric_limits<int>::max());
            box->setMinimum(std::numeric_limits<int>::min());
            QObject::connect(box, QOverload<int>::of(&QSpinBox::valueChanged), form_, &MemberForm::onDataUpdated);
            QObject::connect(form_, SIGNAL(dataUpdated(int)), box, SLOT(setValue(int)));
            return box;
        }

        supportedTypes.clear();
        supportedTypes << "float"
                       << "double"
                       << "real"
                       << "float32_t"
                       << "float64_t";
        if (supportedTypes.contains(dataType()))
        {
            QDoubleSpinBox* box = new QDoubleSpinBox();
            data_               = box->value();
            box->setMaximum(std::numeric_limits<double>::max());
            box->setMinimum(-std::numeric_limits<double>::max());
            box->setDecimals(10);
            QObject::connect(
                box, QOverload<double>::of(&QDoubleSpinBox::valueChanged), form_, &MemberForm::onDataUpdated);
            QObject::connect(form_, SIGNAL(dataUpdated(double)), box, SLOT(setValue(double)));
            return box;
        }

        supportedTypes.clear();
        supportedTypes << "string";
        if (supportedTypes.contains(dataType()))
        {
            QLineEdit* lineEdit = new QLineEdit();
            data_               = lineEdit->text();
            lineEdit->setFont(normal_font_);
            QObject::connect(lineEdit, &QLineEdit::textChanged, form_, &MemberForm::onDataUpdated);
            QObject::connect(form_, SIGNAL(dataUpdated(QString const&)), lineEdit, SLOT(setText(QString const&)));
            return lineEdit;
        }

        return nullptr;
    }
}
