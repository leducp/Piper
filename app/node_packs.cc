#include "piper/app/node_packs.h"

#include <algorithm>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "piper/registry.h"
#include "piper/serialize_v2.h"

namespace piper::studio
{
    std::string nodes_dir()
    {
        char const* xdg = std::getenv("XDG_CONFIG_HOME");
        if (xdg != nullptr and *xdg != '\0')
        {
            return std::string(xdg) + "/piper/nodes";
        }
        char const* home = std::getenv("HOME");
        if (home != nullptr and *home != '\0')
        {
            return std::string(home) + "/.config/piper/nodes";
        }
        return std::string{};
    }

    NodePackLoadResult load_node_pack(std::string const& path,
                                       piper::NodeRegistry& reg)
    {
        NodePackLoadResult r;
        std::ifstream in{path};
        if (not in.is_open())
        {
            r.errors.push_back(path + ": cannot open");
            return r;
        }
        std::ostringstream buf;
        buf << in.rdbuf();
        try
        {
            piper::v2::RegistryLoadResult const loaded =
                piper::v2::deserialize_registry(buf.str());
            for (auto const& d : loaded.diagnostics)
            {
                r.warnings.push_back(path + ": " + d.message);
            }
            for (auto const* nt : loaded.registry.all())
            {
                std::string const lib{ loaded.registry.library_of(nt->type) };
                bool ok = false;
                if (lib.empty()) { ok = reg.add(*nt); }
                else             { ok = reg.add(lib, *nt); }
                if (ok) { ++r.added; }
                else    { ++r.skipped; }
            }
        }
        catch (std::exception const& e)
        {
            r.errors.push_back(path + ": " + e.what());
        }
        return r;
    }

    NodePackLoadResult auto_load_node_packs(piper::NodeRegistry& reg)
    {
        NodePackLoadResult r;
        std::string const dir = nodes_dir();
        if (dir.empty()) { return r; }
        std::error_code ec;
        if (not std::filesystem::is_directory(dir, ec)) { return r; }

        std::vector<std::filesystem::path> files;
        for (auto const& entry :
             std::filesystem::directory_iterator{dir, ec})
        {
            if (ec) { break; }
            if (not entry.is_regular_file()) { continue; }
            auto const ext = entry.path().extension().string();
            if (ext == ".json" or ext == ".JSON")
            {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end());

        for (auto const& p : files)
        {
            NodePackLoadResult sub = load_node_pack(p.string(), reg);
            r.added   += sub.added;
            r.skipped += sub.skipped;
            r.errors.insert(r.errors.end(),
                            sub.errors.begin(), sub.errors.end());
            r.warnings.insert(r.warnings.end(),
                              sub.warnings.begin(), sub.warnings.end());
        }
        return r;
    }
}
