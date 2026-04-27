#include <gtest/gtest.h>

#include "piper/theme.h"

using namespace piper;

namespace
{
    bool any_of_kind(std::vector<Diagnostic> const& diags, DiagnosticKind k)
    {
        for (auto const& d : diags)
        {
            if (d.kind == k)
            {
                return true;
            }
        }
        return false;
    }
}

TEST(Theme, DefaultsAreSane)
{
    Theme t;
    EXPECT_GT(t.grid_spacing, 0.0f);
    EXPECT_GT(t.link_thickness, 0.0f);
    EXPECT_GE(t.node_body_alpha_disabled, 0.0f);
    EXPECT_LE(t.node_body_alpha_disabled, 1.0f);
    EXPECT_GE(t.pin_alpha_inactive, 0.0f);
    EXPECT_LE(t.pin_alpha_inactive, 1.0f);
}

TEST(Theme, BuiltInModeColorsAlwaysPresent)
{
    auto loaded = load_theme_from_string(R"({"version": 2})");
    EXPECT_TRUE(loaded.diagnostics.empty());
    EXPECT_NE(loaded.theme.mode_colors.find("enable"),  loaded.theme.mode_colors.end());
    EXPECT_NE(loaded.theme.mode_colors.find("disable"), loaded.theme.mode_colors.end());
}

TEST(Theme, LoadFromStringPopulatesFields)
{
    std::string text = R"({
        "version": 2,
        "canvas": {
            "bg":   "#101010FF",
            "grid": { "color": "#202020FF", "spacing": 50 }
        },
        "node": {
            "header_default":     "#404040FF",
            "body_default":       "#303030FF",
            "rounding":            6.0,
            "body_alpha_disabled": 0.5,
            "pin_alpha_inactive":  0.3
        },
        "link": {
            "thickness":       3.0,
            "bezier_strength": 70.0,
            "invalid":         "#FF8080FF"
        },
        "types": {
            "float": "#80C0FFFF",
            "vec3":  "#FF80FFFF"
        },
        "modes": {
            "neutral": "#808080FF"
        }
    })";

    auto loaded = load_theme_from_string(text);
    EXPECT_TRUE(loaded.diagnostics.empty());

    Theme const& t = loaded.theme;
    EXPECT_EQ(t.canvas_bg.value,  0x101010FFu);
    EXPECT_EQ(t.grid_line.value,  0x202020FFu);
    EXPECT_EQ(t.grid_spacing,     50.0f);
    EXPECT_EQ(t.node_rounding,    6.0f);
    EXPECT_EQ(t.link_thickness,   3.0f);
    EXPECT_EQ(t.link_invalid.value, 0xFF8080FFu);

    ASSERT_NE(t.type_colors.find("float"), t.type_colors.end());
    EXPECT_EQ(t.type_colors.at("float").value, 0x80C0FFFFu);
    EXPECT_EQ(t.type_colors.at("vec3").value,  0xFF80FFFFu);

    // Built-ins still present, custom label added.
    EXPECT_NE(t.mode_colors.find("enable"),  t.mode_colors.end());
    EXPECT_NE(t.mode_colors.find("disable"), t.mode_colors.end());
    ASSERT_NE(t.mode_colors.find("neutral"), t.mode_colors.end());
    EXPECT_EQ(t.mode_colors.at("neutral").value, 0x808080FFu);
}

TEST(Theme, ThrowsOnMalformedJson)
{
    EXPECT_THROW(load_theme_from_string("not json"),       std::runtime_error);
    EXPECT_THROW(load_theme_from_string("{}"),             std::runtime_error);   // version 0
}

TEST(Theme, ThrowsOnUnsupportedVersion)
{
    EXPECT_THROW(load_theme_from_string(R"({"version": 1})"),  std::runtime_error);
    EXPECT_THROW(load_theme_from_string(R"({"version": 99})"), std::runtime_error);
}

TEST(Theme, MalformedColorFiresSchemaError)
{
    std::string text = R"({
        "version": 2,
        "canvas": { "bg": "not_a_color" }
    })";
    auto loaded = load_theme_from_string(text);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::SchemaError));
}

TEST(Theme, TypeMapEntryWithBadColorFiresSchemaError)
{
    std::string text = R"({
        "version": 2,
        "types": { "float": "#80C0FFFF", "vec3": "bogus" }
    })";
    auto loaded = load_theme_from_string(text);
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::SchemaError));
    // The good entry still loads.
    EXPECT_NE(loaded.theme.type_colors.find("float"), loaded.theme.type_colors.end());
}

TEST(Theme, MissingFieldsKeepDefaults)
{
    auto loaded = load_theme_from_string(R"({"version": 2})");
    Theme const& t = loaded.theme;
    Theme defaults;
    EXPECT_EQ(t.canvas_bg,    defaults.canvas_bg);
    EXPECT_EQ(t.grid_spacing, defaults.grid_spacing);
    EXPECT_EQ(t.link_invalid, defaults.link_invalid);
}

TEST(Theme, ThrowsOnMissingFile)
{
    EXPECT_THROW(load_theme("/nonexistent/path/theme.json"), std::runtime_error);
}

TEST(Theme, DataThemeJsonLoadsWithoutDiagnostics)
{
    std::string path = std::string(PIPER_SOURCE_DIR) + "/data/theme.json";
    auto loaded = load_theme(path);
    EXPECT_TRUE(loaded.diagnostics.empty()) << "data/theme.json has schema issues";
    // Spot-check that builtin mode colors are populated and stock
    // type colors are loaded.
    EXPECT_NE(loaded.theme.mode_colors.find("enable"),  loaded.theme.mode_colors.end());
    EXPECT_NE(loaded.theme.mode_colors.find("disable"), loaded.theme.mode_colors.end());
    EXPECT_NE(loaded.theme.type_colors.find("float"),   loaded.theme.type_colors.end());
    EXPECT_NE(loaded.theme.type_colors.find("vec3"),    loaded.theme.type_colors.end());
}

TEST(Theme, WrongTypeColorValueFiresSchemaError)
{
    auto loaded = load_theme_from_string(R"({"version": 2, "canvas": {"bg": 255}})");
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::SchemaError));
}

TEST(Theme, WrongTypeSectionFiresSchemaError)
{
    auto loaded = load_theme_from_string(R"({"version": 2, "types": "oops"})");
    EXPECT_TRUE(any_of_kind(loaded.diagnostics, DiagnosticKind::SchemaError));
}
