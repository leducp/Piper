#include "JsonExport.h"

#include <QDebug>

namespace piper
{
    void JsonExport::init(QString const& filename)
    {
        qDebug() << __FUNCTION__ << filename;
    }
    
    void JsonExport::writeStage(QString const& stage)
    {
        qDebug() << __FUNCTION__ << stage;
    }
    
    void JsonExport::writeNodeMetadata(QString const& type, QString const& name, QString const& stage)
    {
        qDebug() << __FUNCTION__ << type << name << stage;
    }
    
    void JsonExport::writeNodeAttribute(QString const& nodeName, QString const& name, QVariant const& data) 
    {
        qDebug() << __FUNCTION__ << nodeName << name << data;
    }
    
    void JsonExport::writeLink(QString const& from, QString const& output, QString const& to, QString const& input) 
    {
        qDebug() << __FUNCTION__ << from << output << to << input;
    }
        
    void JsonExport::finalize()
    {
        qDebug() << __FUNCTION__;
    }
    
}
