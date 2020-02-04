#ifndef NODE_ITEM_H
#define NODE_ITEM_H

#include <QGraphicsItem>
#include <QPainter>
#include <QHash>



class Attribute : public QGraphicsItem
{
public:
    Attribute(QGraphicsItem* parent, QString const& name, QRect const& boundingRect)
        : QGraphicsItem(parent)
        , name_{name}
        , boundingRect_{boundingRect}
    { 
        pen_.setStyle(Qt::SolidLine);
        pen_.setColor({0, 0, 0, 0});
    }
    
    void setBrush(QBrush const& brush) { brush_ = brush; }
    void setFont(QFont const& font, QColor const& color)    
    { 
        font_ = font;   
        fontPen_.setStyle(Qt::SolidLine);
        fontPen_.setColor(color);
    }
    
protected:
    QRectF boundingRect() const override { return boundingRect_; }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    
    QString name_;
    
    QFont font_;
    QPen fontPen_;
    QBrush brush_;
    QPen pen_;
    
    QRect boundingRect_;
};


class NodeItem : public QGraphicsItem
{
    //Q_OBJECT

public:
    NodeItem(QGraphicsItem* parent = nullptr);
    virtual ~NodeItem() = default;
    
    //void addAttribute(QString const& name, Attribute const& info);
    
protected:
    QRectF boundingRect() const override;
    void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;
    
private:
    void createStyle();
    
    qint32 width_;
    qint32 height_;
    QString name_;

    QBrush brush_;
    QPen pen_;
    QPen penSel_;
    
    QFont textFont_;
    QPen textPen_;
    QRect textRect_;
    
    QFont attrFont_;
    QPen attrFontPen_;
    QBrush attrBrush_;
    QBrush attrAltBrush_;
    QPen attrPen_;
    
    //QHash<QString, Attribute> attributes_;
    QGraphicsTextItem testItem_;
    QList<Attribute*> attributes_;
};

#endif 
  
