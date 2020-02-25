#include "JsonExport.h"

#include <QDebug>
#include <QJsonDocument>
#include <QFile>

namespace piper
{
    void JsonExport::init()
    {

    }
    
    void JsonExport::writeStage(QString const& stage)
    {
        QJsonObject newEntry;
        newEntry["order"] = stage_number_;
        stage_number_++;

        QJsonObject stages = object_["Stages"].toObject();
        stages[stage] = newEntry;
        stages["number"] = stage_number_;
        object_["Stages"] = stages;
    }
    
    void JsonExport::writeNodeMetadata(QString const& type, QString const& name, QString const& stage)
    {
        QJsonObject step;
        step["type"] = type;
        step["stage"] = stage;
        
        QJsonObject steps = object_["Steps"].toObject();
        steps[name] = step;
        object_["Steps"] = steps;
        
        qDebug() << object_;
    }
    
    void JsonExport::writeNodeAttribute(QString const& node_name, QString const& name, QVariant const& data) 
    {
        if ((not data.isValid()) or (data.type() == QVariant::Bool))
        {
            return;
        }
        
        QJsonObject steps = object_["Steps"].toObject();
        QJsonObject step = steps[node_name].toObject();
        step[name] = QJsonValue::fromVariant(data);
        steps[node_name] = step;
        object_["Steps"] = steps;
    }
    
    void JsonExport::writeLink(QString const& from, QString const& output, QString const& to, QString const& input) 
    {
        QJsonObject link;
        link["from"] = from;
        link["output"] = output;
        link["to"] = to;
        link["input"] = input;

        QJsonObject links = object_["Links"].toObject();
        QString link_name = from + "_" + output + "_" + to + "_" + input;
        links[link_name] = link;
        object_["Links"] = links;
    }
        
    void JsonExport::finalize(QString const& filename)
    {
        QJsonDocument doc(object_);
        QFile io(filename);
        io.open(QIODevice::WriteOnly);
        io.write(doc.toJson());
    }
    
}
