#ifndef PIPER_THEME_MANAGER_H
#define PIPER_THEME_MANAGER_H

#include <QString>
#include <QColor>
#include <QFont>

namespace piper
{
    struct NodeTheme
    {
        QFont name_font;
        QColor name_color;

        QColor background;

        QFont type_font;
        QColor type_color;
        QColor type_background;

        QColor border;
        QColor border_selected;
    };

    struct AttributeTheme
    {
        QColor background;
        QColor background_alt;

        struct Mode
        {
            QFont font;
            QColor font_color;
            struct
            {
                int border_width;
                QColor border_color;
            } connector;
        };

        Mode minimize;
        Mode normal;
        Mode highlight;
    };


    struct DataTypeTheme
    {
        QColor enable;
        QColor disable;
        QColor neutral;
    };


    class ThemeManager
    {
    public:
        static ThemeManager& instance();

        bool load(QString const& theme_filename);
        NodeTheme getNodeTheme() const;
        AttributeTheme getAttributeTheme() const;
        DataTypeTheme getDataTypeTheme(QString const& dataType);

    private:
        ThemeManager() = default;

        bool parseNode(QJsonObject const& json);
        bool parseAttribute(QJsonObject const& json);
        bool parseDataType(QJsonObject const& json);
        QColor parseColor(QJsonObject const& json);
        QFont  parseFont(QJsonObject const& json);

        NodeTheme node_theme_;
        AttributeTheme attribute_theme_;
        QHash<QString, DataTypeTheme> data_type_themes_;
    };
}


#endif

