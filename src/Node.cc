#include <QGraphicsScene>
#include <QTextDocument>
#include <QGraphicsSceneContextMenuEvent>
#include <QMenu>
#include <QDebug>

#include "Node.h"
#include "Link.h"
#include "AttributeMember.h"
#include "ThemeManager.h"

namespace piper
{
    constexpr int attributeHeight = 30;
    constexpr int baseHeight = 35;
    constexpr int baseWidth  = 250;


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

        type_rect_ = QRectF{1, 17, width_ - 2.0, attributeHeight};
        height_ += attributeHeight; // Give some space for type section
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


    void Node::createAttributes(QVector<AttributeInfo> const& attributesInfo)
    {
        if (not attributes_.empty())
        {
            qWarning() << "Creating attributes in multiples call is not supported.";
            return;
        }

        // Compute width.
        QFont attributeFont = ThemeManager::instance().getAttributeTheme().normal.font;
        QFontMetrics metrics(attributeFont);
        QRect boundingRect{0, 0, width_ - 32, attributeHeight}; // -30 to keep space for attribute custom display / -2 for node border
        for (auto const& info : attributesInfo)
        {
            boundingRect = boundingRect.united(metrics.boundingRect(info.name));
        }
        // Adjust bounding rect position / width / height.
        boundingRect.setTopLeft({0, 0});
        boundingRect.setWidth(boundingRect.width() + 30); // add space again
        boundingRect.setHeight(attributeHeight);

        // Adjust node width
        width_ = boundingRect.width() + 2;
        type_rect_.setWidth(boundingRect.width());

        // Create attributes
        for (auto const& info : attributesInfo)
        {
            Attribute* attr{nullptr};
            if (info.name == "stage")
            {
                qWarning() << "Stage is a reserved attribute: skipping";
                continue;
            }
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
            attr->setPos(1, 17 + attributeHeight * (attributes_.size() + 1));
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
            attributes_.append(attr);
        }

        prepareGeometryChange();
        // readjust name position.
        name_->adjustPosition();

    }


    void Node::createStyle()
    {
        NodeTheme node_theme = ThemeManager::instance().getNodeTheme();
        AttributeTheme attribute_theme = ThemeManager::instance().getAttributeTheme();

        qint32 border = 2;

        background_brush_.setStyle(Qt::SolidPattern);
        background_brush_.setColor(node_theme.background);

        pen_.setStyle(Qt::SolidLine);
        pen_.setWidth(border);
        pen_.setColor(node_theme.border);

        pen_selected_.setStyle(Qt::SolidLine);
        pen_selected_.setWidth(border);
        pen_selected_.setColor(node_theme.border_selected);

        name_->setFont(node_theme.name_font);
        name_->setDefaultTextColor(node_theme.name_color);
        name_->adjustPosition();

        attribute_brush_.setStyle(Qt::SolidPattern);
        attribute_brush_.setColor(attribute_theme.background);
        attribute_alt_brush_.setStyle(Qt::SolidPattern);
        attribute_alt_brush_.setColor(attribute_theme.background_alt);

        type_brush_.setStyle(Qt::SolidPattern);
        type_brush_.setColor(node_theme.type_background);
        type_pen_.setStyle(Qt::SolidLine);
        type_pen_.setColor(node_theme.type_color);
        type_font_ = node_theme.type_font;
    }


    QRectF Node::boundingRect() const
    {
        return bounding_rect_;
    }


    void Node::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*)
    {
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
        type_rect_.setWidth(width_ - 2.0);
        painter->drawRoundedRect(0, 0, width_, height_, radius, radius);

        // type background.
        painter->setBrush(type_brush_);
        painter->setPen(Qt::NoPen);
        painter->drawRect(type_rect_);

        // type label.
        painter->setFont(type_font_);
        painter->setPen(type_pen_);
        painter->drawText(type_rect_, Qt::AlignCenter, type_);

        updateWidth();
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

        for (auto& attribute : attributes_)
        {
            DataTypeTheme theme = ThemeManager::instance().getDataTypeTheme(attribute->dataType());

            if (attribute->isOutput())
            {
                switch (mode)
                {
                    case Mode::enable:  { attribute->setColor(theme.enable);  break; }
                    case Mode::disable: { attribute->setColor(theme.disable); break; }
                    case Mode::neutral: { attribute->setColor(theme.neutral); break; }
                }
            }
        }
    }

    void Node::updateWidth()
    {
        QList<qreal> widths{name_->sceneBoundingRect().width(), baseWidth};
        for (auto& attribute : attributes_)
        {
            if (attribute->isMember())
            {
                widths.append(attribute->getFormBaseWidth() + attribute->labelRect().right() + 20);
            }
        }

        std::sort(widths.begin(), widths.end(), std::greater<int>());
        width_ = static_cast<qint32>(widths[0]);
        for (auto& attribute : attributes_)
        {
            QRectF rectangle = attribute->boundingRect();
            QPointF pose = QPointF{1, rectangle.top()};
            rectangle.setTopLeft(pose);
            rectangle.setWidth(width_ - 3.0);
            attribute->updateRectSize(rectangle);
            attribute->updateConnectorPosition();
        }
        bounding_rect_ = QRectF(0, 0, width_, height_);
        name_->adjustPosition();
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

        menu.setStyleSheet(""
            "QMenu::separator"
            "{"
                "height: 1px;"
                "background-color: #505F69;"
                "color: #F0F0F0;"
            "}"
        );


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
            enable->setCheckable(true);
            if (mode_ == Mode::enable) { enable->setChecked(true); }
            QObject::connect(enable, &QAction::triggered,  std::bind(updateMode, Mode::enable));

            QAction* disable = menu.addAction("Disable");
            disable->setCheckable(true);
            if (mode_ == Mode::disable) { disable->setChecked(true); }
            QObject::connect(disable, &QAction::triggered, std::bind(updateMode, Mode::disable));

            QAction* neutral = menu.addAction("Neutral");
            neutral->setCheckable(true);
            if (mode_ == Mode::neutral) { neutral->setChecked(true); }
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
        QVector<AttributeInfo> attributesInfo;
        QVector<QVariant> attributesData;
        for (int j = 0; j < attributesSize; ++j)
        {
            AttributeInfo info;
            QVariant data;
            in >> info >> data;
            attributesInfo.append(info);
            attributesData.append(data);
        }

        node.createAttributes(attributesInfo);
        auto data = attributesData.constBegin();
        for (auto const& attribute : node.attributes())
        {
            attribute->setData(*data);
            ++data;
        }
        return in;
    }
}
