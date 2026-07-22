#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>

#include "piper/app/autosave.h"

#include "piper/serialize_v2.h"

namespace piper::studio
{
    std::string autosave_dir()
    {
        char const* xdg = std::getenv("XDG_DATA_HOME");
        if (xdg != nullptr and *xdg != '\0')
        {
            return std::string(xdg) + "/piper/autosave";
        }
        char const* home = std::getenv("HOME");
        if (home != nullptr and *home != '\0')
        {
            return std::string(home) + "/.local/share/piper/autosave";
        }
        return std::string{};
    }

    void autosave_write(Document& doc)
    {
        std::string const dir = autosave_dir();
        if (dir.empty())
        {
            return;
        }
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
        if (ec)
        {
            return;
        }
        std::string const path = dir + "/session-" + std::to_string(doc.session_id) + ".piper";
        std::string const tmp  = path + ".tmp";
        std::string const json = piper::v2::serialize(doc.graph, doc.pipeline_name);
        {
            std::ofstream f(tmp);
            if (not f.is_open())
            {
                return;
            }
            f << json;
            // close() flushes; a failed flush (e.g. disk full) sets
            // failbit. Checking after close catches a truncated write
            // before it renames over the last good autosave.
            f.close();
            if (not f)
            {
                std::filesystem::remove(tmp, ec);
                return;
            }
        }
        std::filesystem::rename(tmp, path, ec);
        if (ec)
        {
            std::filesystem::remove(tmp, ec);
            return;
        }
        doc.autosave_path    = path;
        doc.last_autosave_at = std::chrono::steady_clock::now();
    }

    void autosave_remove(Document& doc)
    {
        if (doc.autosave_path.empty())
        {
            return;
        }
        std::error_code ec;
        std::filesystem::remove(doc.autosave_path, ec);
        doc.autosave_path.clear();
    }

    std::vector<std::string> scan_autosave_dir()
    {
        std::vector<std::string> out;
        std::string const dir = autosave_dir();
        if (dir.empty())
        {
            return out;
        }
        std::error_code ec;
        if (not std::filesystem::is_directory(dir, ec))
        {
            return out;
        }
        for (auto const& entry : std::filesystem::directory_iterator(dir, ec))
        {
            if (entry.is_regular_file() and entry.path().extension() == ".piper")
            {
                out.push_back(entry.path().string());
            }
        }
        return out;
    }
}
