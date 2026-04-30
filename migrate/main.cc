#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <stdexcept>
#include <string>

#include <argparse/argparse.hpp>

#include "piper/builtin_nodes.h"
#include "piper/migrate/v1_reader.h"
#include "piper/registry.h"
#include "piper/serialize_v2.h"

namespace fs = std::filesystem;

namespace
{
    std::string read_file(fs::path const& path)
    {
        std::ifstream in(path, std::ios::binary);
        if (not in)
        {
            throw std::runtime_error("cannot open: " + path.string());
        }
        std::stringstream buf;
        buf << in.rdbuf();
        return buf.str();
    }

    void write_file(fs::path const& path, std::string_view data)
    {
        std::ofstream out(path, std::ios::binary | std::ios::trunc);
        if (not out)
        {
            throw std::runtime_error("cannot write: " + path.string());
        }
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
    }

    fs::path default_output(fs::path const& input)
    {
        fs::path out = input;
        out.replace_extension(".piper");
        return out;
    }

    char const* kind_str(piper::Diagnostic::Kind k)
    {
        switch (k)
        {
            case piper::Diagnostic::Kind::SchemaError:           { return "schema-error"; }
            case piper::Diagnostic::Kind::DuplicateNodeId:       { return "duplicate-node-id"; }
            case piper::Diagnostic::Kind::DuplicateLinkId:       { return "duplicate-link-id"; }
            case piper::Diagnostic::Kind::DuplicateStageName:    { return "duplicate-stage-name"; }
            case piper::Diagnostic::Kind::DuplicateProfileName:  { return "duplicate-profile-name"; }
            case piper::Diagnostic::Kind::DuplicateTypeName:     { return "duplicate-type-name"; }
            case piper::Diagnostic::Kind::UnknownNodeType:       { return "unknown-node-type"; }
            case piper::Diagnostic::Kind::AttributeMissing:      { return "attribute-missing"; }
            case piper::Diagnostic::Kind::AttributeAdded:        { return "attribute-added"; }
            case piper::Diagnostic::Kind::AttributeDrift:        { return "attribute-drift"; }
            case piper::Diagnostic::Kind::LinkOrphanedNode:      { return "link-orphaned-node"; }
            case piper::Diagnostic::Kind::LinkOrphanedAttribute: { return "link-orphaned-attribute"; }
            case piper::Diagnostic::Kind::LinkTypeMismatch:      { return "link-type-mismatch"; }
            case piper::Diagnostic::Kind::OrphanModeReference:   { return "orphan-mode-reference"; }
            case piper::Diagnostic::Kind::UnknownStageReference: { return "unknown-stage-reference"; }
        }
        return "unknown";
    }
}

int main(int argc, char* argv[])
{
    argparse::ArgumentParser cli("piper-migrate", "0.1.0");
    cli.add_description("Convert V1 Piper JSON to the current "
                        ".piper file format.");

    cli.add_argument("input")
        .help("input V1 JSON file");

    cli.add_argument("-o", "--output")
        .help("output .piper file (defaults to input with .piper extension)")
        .default_value(std::string{});

    cli.add_argument("--dry-run")
        .help("parse and report only; do not write the output file")
        .flag();

    cli.add_argument("--strict")
        .help("upgrade reader warnings into errors")
        .flag();

    try
    {
        cli.parse_args(argc, argv);
    }
    catch (std::exception const& e)
    {
        std::fprintf(stderr, "piper-migrate: %s\n", e.what());
        std::fprintf(stderr, "%s", cli.help().str().c_str());
        return 2;
    }

    fs::path const input{ cli.get<std::string>("input") };
    fs::path output;
    {
        std::string const raw = cli.get<std::string>("--output");
        if (raw.empty())
        {
            output = default_output(input);
        }
        else
        {
            output = raw;
        }
    }
    bool const dry_run = cli.get<bool>("--dry-run");
    bool const strict  = cli.get<bool>("--strict");

    piper::NodeRegistry registry;
    piper::register_builtin_nodes(registry);

    piper::migrate::Options opts;
    opts.strict = strict;

    try
    {
        std::string const v1_json = read_file(input);
        auto              bundle  = piper::migrate::read_v1(v1_json, registry, opts);

        std::size_t total_diags     = bundle.diagnostics.size();
        std::size_t critical_diags  = 0;
        auto const  is_critical = [](piper::Diagnostic::Kind k)
        {
            // Unknown node types lose data: the node and all its
            // attributes/links are dropped. Refuse to write a partial
            // V2 file rather than silently emit something the engine
            // cannot reconstruct.
            return k == piper::Diagnostic::Kind::UnknownNodeType;
        };

        for (auto const& d : bundle.diagnostics)
        {
            std::fprintf(stderr, "warning [%s]: %s\n",
                         kind_str(d.kind), d.message.c_str());
            if (is_critical(d.kind))
            {
                ++critical_diags;
            }
        }
        for (auto const& p : bundle.pipelines)
        {
            for (auto const& d : p.diagnostics)
            {
                std::fprintf(stderr, "warning [%s] (%s): %s\n",
                             kind_str(d.kind), p.name.c_str(), d.message.c_str());
                ++total_diags;
                if (is_critical(d.kind))
                {
                    ++critical_diags;
                }
            }
        }

        if (critical_diags > 0)
        {
            std::fprintf(stderr,
                         "piper-migrate: %zu critical diagnostic(s) -- aborting (no output written)\n",
                         critical_diags);
            return 1;
        }
        if (strict and total_diags > 0)
        {
            std::fprintf(stderr,
                         "piper-migrate: --strict and %zu diagnostic(s) -- aborting (no output written)\n",
                         total_diags);
            return 2;
        }

        if (dry_run)
        {
            std::fprintf(stdout, "piper-migrate: dry-run, %zu pipeline(s), %zu diagnostic(s)\n",
                         bundle.pipelines.size(), total_diags);
            return 0;
        }

        std::vector<piper::v2::PipelineRef> refs;
        refs.reserve(bundle.pipelines.size());
        for (auto const& p : bundle.pipelines)
        {
            refs.push_back({ p.name, &p.graph });
        }
        std::string const v2_json = piper::v2::serialize_bundle(refs);
        write_file(output, v2_json);
        std::fprintf(stdout, "piper-migrate: wrote %s (%zu pipeline(s))\n",
                     output.string().c_str(), bundle.pipelines.size());
        return 0;
    }
    catch (std::exception const& e)
    {
        std::fprintf(stderr, "piper-migrate: %s\n", e.what());
        return 1;
    }
}
