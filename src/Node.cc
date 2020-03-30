#include <QGraphicsScene>
#include <QTextDocument>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QDebug>

#include "Node.h"
#include "Link.h"
#include "AttributeMember.h"

namespace piper
{
    constexpr int attributeHeight = 30;
    constexpr int baseHeight = 35;
    constexpr int baseWidth  = 250;
    QColor const attribute_brush    {60, 60, 60, 255};
    QColor const attribute_brush_alt{70, 70, 70, 255};



    NodeName::NodeName(QGraphicsItem* parent) : QGraphicsTextItem(parent)
    {
        QTextOption options;
        options.setWrapMode(QTextOption::NoWrap);
        document()->setDefaultTextOption(options);
    }


    void NodeName::adjustPosition()
    {
        setPos(-(boundingRect().width() - parentItem()->boundingRect().width()) * 0.5, -boundingRect().height());
    }


    void NodeName::keyPressEvent(QKeyEvent* e)
    {
        if (e->key() == Qt::Key_Return)
        {
            clearFocus();
            return;
        }

        // Handle event (text change) and recompute position
        QGraphicsTextItem::keyPressEvent(e);
        adjustPosition();
    }


    Node::Node(QString const& type, QString const& name, QString const& stage)
        : QGraphicsItem(nullptr)
        , bounding_rect_{0, 0, baseWidth, baseHeight}
        , name_{new NodeName(this)}
        , type_{type}
        , stage_{stage}
        , mode_{Mode::enable}
        , width_{baseWidth}
        , height_{baseHeight}
        , attributes_{}
    {
        // Configure item behavior.
        setFlag(QGraphicsItem::ItemIsMovable);
        setFlag(QGraphicsItem::ItemIsSelectable);
        setFlag(QGraphicsItem::ItemIsFocusable);

        // Configure node name
        name_->setTextInteractionFlags(Qt::TextEditorInteraction);
        setName(name);

        createStyle();
    }


    Node::~Node()
    {
        Scene* pScene = static_cast<Scene*>(scene());
        pScene->removeNode(this);
    }


    void Node::highlight(Attribute* emitter)
    {
        for (auto& attr : attributes_)
        {
            if (attr == emitter)
            {
                // special case: do not change emitter mode.
                continue;
            }

            if (attr->accept(emitter))
            {
                attr->setMode(DisplayMode::highlight);
            }
            else
            {
                attr->setMode(DisplayMode::minimize);
            }
            attr->update();
        }
    }


    void Node::unhighlight()
    {
        for (auto& attr : attributes_)
        {
            attr->setMode(DisplayMode::normal);
            attr->update();
        }
    }


    Attribute* Node::addAttribute(AttributeInfo const& info)
    {
        constexpr QRect boundingRect{0, 0, baseWidth-2, attributeHeight};

        Attribute* attr;
        switch (info.type)
        {
            case AttributeInfo::Type::input:
            {
                attr = new AttributeInput(this, info, boundingRect);
                break;
            }
            case AttributeInfo::Type::output:
            {
                attr = new AttributeOutput(this, info, boundingRect);
                break;
            }
            case AttributeInfo::Type::member:
            {
                attr = new AttributeMember(this, info, boundingRect);
                break;
            }
        }
        attr->setPos(1, 17 + attributeHeight * attributes_.size());
        if (attributes_.size() % 2)
        {
            attr->setBackgroundBrush(attribute_brush_);
        }
        else
        {
            attr->setBackgroundBrush(attribute_alt_brush_);
        }
        height_ += attributeHeight;
        bounding_rect_ = QRectF(0, 0, width_, height_);
        bounding_rect_ += QMargins(1, 1, 1, 1);
        prepareGeometryChange();
        attributes_.append(attr);

        return attr;
    }


    void Node::createStyle()
    {
        qint32 border = 2;

        background_brush_.setStyle(Qt::SolidPattern);
        background_brush_.setColor(default_background);

        pen_.setStyle(Qt::SolidLine);
        pen_.setWidth(border);
        pen_.setColor({50, 50, 50, 255});

        pen_selected_.setStyle(Qt::SolidLine);
        pen_selected_.setWidth(border);
        pen_selected_.setColor({170, 80, 80, 255});

        name_->setFont({"Noto", 12, QFont::Bold});
        name_->setDefaultTextColor({255, 255, 255, 255});
        name_->adjustPosition();

        attribute_brush_.setStyle(Qt::SolidPattern);
        attribute_brush_.setColor(attribute_brush);
        attribute_alt_brush_.setStyle(Qt::SolidPattern);
        attribute_alt_brush_.setColor(attribute_brush_alt);
    }


    QRectF Node::boundingRect() const
    {
        return bounding_rect_;
    }


    void Node::paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget)
    {
        Q_UNUSED(option);
        Q_UNUSED(widget);

        // Base shape.
        painter->setBrush(background_brush_);

        if (isSelected())
        {
            painter->setPen(pen_selected_);
        }
        else
        {
            painter->setPen(pen_);
        }

        qint32 radius = 10;
        painter->drawRoundedRect(0, 0, width_, height_, radius, radius);
    }


    void Node::mousePressEvent(QGraphicsSceneMouseEvent* event)
    {
        // Force selected node on top layer
        for (auto& item : scene()->items())
        {
            if (item->zValue() > 1)
            {
                item->setZValue(1);
            }
        }
        setZValue(2);

        QGraphicsItem::mousePressEvent(event);
    }

    void Node::mouseMoveEvent(QGraphicsSceneMouseEvent* event)
    {
        for (auto& attr : attributes_)
        {
            attr->refresh(); // let the attribute refresh their data if required.
        }

        QGraphicsItem::mouseMoveEvent(event);
    }


    QString Node::name() const
    {
        return name_->toPlainText();
    }


    void Node::setName(QString const& name)
    {
        name_->setPlainText(name);

        // Compute position
        name_->adjustPosition();
    }


    void Node::setMode(Mode mode)
    {
        mode_ = mode;

        QColor color;
        switch (mode)
        {
            case Mode::enable:  { color = color_enable;  break; }
            case Mode::disable: { color = color_disable; break; }
            case Mode::neutral: { color = color_neutral; break; }
        }

        for (auto& attribute : attributes_)
        {
            if (attribute->isOutput())
            {
                attribute->setColor(color);
            }
        }
    }


    void Node::keyPressEvent(QKeyEvent* event)
    {
        if (isSelected())
        {
            constexpr qreal moveFactor = 5;
            if ((event->key() == Qt::Key::Key_Up) and (event->modifiers() == Qt::NoModifier))
            {
                moveBy(0, -moveFactor);
            }
            if ((event->key() == Qt::Key::Key_Down) and (event->modifiers() == Qt::NoModifier))
            {
                moveBy(0, moveFactor);
            }
            if ((event->key() == Qt::Key::Key_Left) and (event->modifiers() == Qt::NoModifier))
            {
                moveBy(-moveFactor, 0);
            }
            if ((event->key() == Qt::Key::Key_Right) and (event->modifiers() == Qt::NoModifier))
            {
                moveBy(moveFactor, 0);
            }

            return;
        }

        QGraphicsItem::keyPressEvent(event);
    }


    void Node::contextMenuEvent(QGraphicsSceneContextMenuEvent* event)
    {
        Scene* pScene = static_cast<Scene*>(scene());
        QMenu menu;

        // Create stage menu entries.
        menu.addSection("Stage");
        for (int i = 0; i < pScene->stages()->rowCount(); ++i)
        {
            QString stage = pScene->stages()->item(i, 0)->data(Qt::DisplayRole).toString();
            QAction* stageAction = menu.addAction(stage);
            stageAction->setCheckable(true);
            stageAction->setData(stage);
            if (stage == stage_)
            {
                stageAction->setChecked(true);
            }

            QObject::connect(stageAction, &QAction::triggered,
                            [this, pScene, stage](bool isChecked)
                            {
                                if (isChecked)
                                {
                                    stage_ = stage;
                                }
                                else
                                {
                                    stage_ = "";
                                }

                                pScene->onStageUpdated();
                            });
        }


        QStandardItem* currentMode = nullptr;
        for (int i = 0; i < pScene->modes()->rowCount(); ++i)
        {
            QStandardItem* mode = pScene->modes()->item(i, 0);

            // Search the current mode.
            if (mode->data(Qt::UserRole + 1).toBool() == false)
            {
                continue;
            }

            currentMode = mode;
        }

        if (currentMode != nullptr)
        {
            menu.addSection(currentMode->data(Qt::DisplayRole).toString());

            auto updateMode = [this, currentMode](enum Mode mode)
            {
                // Apply mode on display
                this->setMode(mode);

                // Save mode
                QHash<QString, QVariant> nodeMode = currentMode->data(Qt::UserRole + 2).toHash();
                nodeMode[this->name()] = mode;
                currentMode->setData(nodeMode, Qt::UserRole + 2);
            };

            QAction* enable = menu.addAction("Enable");
            QObject::connect(enable, &QAction::triggered,  std::bind(updateMode, Mode::enable));

            QAction* disable = menu.addAction("Disable");
            QObject::connect(disable, &QAction::triggered, std::bind(updateMode, Mode::disable));

            QAction* neutral = menu.addAction("Neutral");
            QObject::connect(neutral, &QAction::triggered, std::bind(updateMode, Mode::neutral));
        }

        (void) menu.exec(event->screenPos());
    }


    QDataStream& operator<<(QDataStream& out, Node const& node)
    {
        // Save node data
        out << node.type_ << node.name() << node.stage_ << node.pos();

        // save node attributes
        out << node.attributes().size();
        for (auto const& attr: node.attributes())
        {
            out << attr->info() << attr->data();
        }

        return out;
    }


    QDataStream& operator>>(QDataStream& in, Node& node)
    {
        // load node data
        QPointF pos;
        QString name;
        in >> node.type_ >> name >>node.stage_ >> pos;
        node.setPos(pos);
        node.setName(name);

        // load node attributes
        int attributesSize;
        in >> attributesSize;
        for (int j = 0; j < attributesSize; ++j)
        {
            AttributeInfo info;
            QVariant data;
            in >> info >> data;
            node.addAttribute(info)->setData(data);
        }

        return in;
    }
}
