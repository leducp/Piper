#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

#include "piper/app/settings.h"

namespace piper::studio
{
    std::string settings_path()
    {
        char const* xdg = std::getenv("XDG_CONFIG_HOME");
        if (xdg != nullptr and *xdg != '\0')
        {
            return std::string(xdg) + "/piper/settings.json";
        }
        char const* home = std::getenv("HOME");
        if (home != nullptr and *home != '\0')
        {
            return std::string(home) + "/.config/piper/settings.json";
        }
        return std::string{};
    }

    Settings load_settings()
    {
        Settings out;
        std::string const path = settings_path();
        if (path.empty())
        {
            return out;
        }
        std::ifstream in{path};
        if (not in.is_open())
        {
            return out;
        }
        std::ostringstream buf;
        buf << in.rdbuf();
        try
        {
            auto const doc = nlohmann::json::parse(buf.str());
            if (auto it = doc.find("font_path"); it != doc.end() and it->is_string())
            {
                out.font_path = it->get<std::string>();
            }
            if (auto it = doc.find("font_size"); it != doc.end() and it->is_number())
            {
                // Mirror the UI drag range; out-of-range values assert
                // in ImGui's font baking.
                float size = it->get<float>();
                if (size < 8.0f)  { size = 8.0f; }
                if (size > 48.0f) { size = 48.0f; }
                out.font_size = size;
            }
            if (auto it = doc.find("window_x"); it != doc.end() and it->is_number_integer())
            {
                out.window_x = it->get<int>();
            }
            if (auto it = doc.find("window_y"); it != doc.end() and it->is_number_integer())
            {
                out.window_y = it->get<int>();
            }
            if (auto it = doc.find("window_w"); it != doc.end() and it->is_number_integer())
            {
                // < 1 would make every launch fail; leave unset so the
                // caller's default applies.
                int const w = it->get<int>();
                if (w >= 1)
                {
                    out.window_w = w;
                }
            }
            if (auto it = doc.find("window_h"); it != doc.end() and it->is_number_integer())
            {
                int const h = it->get<int>();
                if (h >= 1)
                {
                    out.window_h = h;
                }
            }
            if (auto it = doc.find("recent_files"); it != doc.end() and it->is_array())
            {
                std::vector<std::string> v;
                for (auto const& entry : *it)
                {
                    if (entry.is_string())
                    {
                        v.push_back(entry.get<std::string>());
                    }
                }
                out.recent_files = std::move(v);
            }
        }
        catch (std::exception const& e)
        {
            std::fprintf(stderr, "settings: parse failed (%s)\n", e.what());
        }
        return out;
    }

    void save_settings(Settings const& s)
    {
        std::string const path = settings_path();
        if (path.empty())
        {
            return;
        }
        std::error_code ec;
        std::filesystem::create_directories(
            std::filesystem::path(path).parent_path(), ec);
        if (ec)
        {
            std::fprintf(stderr, "settings: cannot create dir (%s)\n", ec.message().c_str());
            return;
        }

        nlohmann::json doc = nlohmann::json::object();
        // Read-modify-write so partial saves don't clobber other keys.
        std::ifstream existing{path};
        if (existing.is_open())
        {
            std::ostringstream buf;
            buf << existing.rdbuf();
            try
            {
                auto parsed = nlohmann::json::parse(buf.str());
                if (parsed.is_object())
                {
                    doc = std::move(parsed);
                }
            }
            catch (std::exception const&)
            {
            }
        }

        if (s.font_path.has_value())
        {
            doc["font_path"] = *s.font_path;
        }
        if (s.font_size.has_value())
        {
            doc["font_size"] = *s.font_size;
        }
        if (s.window_x.has_value()) { doc["window_x"] = *s.window_x; }
        if (s.window_y.has_value()) { doc["window_y"] = *s.window_y; }
        if (s.window_w.has_value()) { doc["window_w"] = *s.window_w; }
        if (s.window_h.has_value()) { doc["window_h"] = *s.window_h; }
        if (s.recent_files.has_value())
        {
            doc["recent_files"] = *s.recent_files;
        }

        std::string const tmp = path + ".tmp";
        {
            std::ofstream out{tmp};
            if (not out.is_open())
            {
                std::fprintf(stderr, "settings: cannot write %s\n", tmp.c_str());
                return;
            }
            out << doc.dump(2) << '\n';
            out.flush();
            if (not out)
            {
                std::fprintf(stderr, "settings: write to %s failed\n", tmp.c_str());
                std::error_code wec;
                std::filesystem::remove(tmp, wec);
                return;
            }
        }
        std::error_code rec;
        std::filesystem::rename(tmp, path, rec);
        if (rec)
        {
            std::fprintf(stderr, "settings: rename failed: %s\n", rec.message().c_str());
            std::filesystem::remove(tmp, rec);
        }
    }
}
