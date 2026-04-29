#include <cstdio>
#include <filesystem>
#include <fstream>
#include <ios>
#include <sstream>
#include <stdexcept>
#include <string>

#include <argparse/argparse.hpp>

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

    char const* kind_str(piper::DiagnosticKind k)
    {
        switch (k)
        {
            case piper::DiagnosticKind::SchemaError:           { return "schema-error"; }
            case piper::DiagnosticKind::DuplicateNodeId:       { return "duplicate-node-id"; }
            case piper::DiagnosticKind::DuplicateLinkId:       { return "duplicate-link-id"; }
            case piper::DiagnosticKind::DuplicateStageName:    { return "duplicate-stage-name"; }
            case piper::DiagnosticKind::DuplicateProfileName:  { return "duplicate-profile-name"; }
            case piper::DiagnosticKind::DuplicateTypeName:     { return "duplicate-type-name"; }
            case piper::DiagnosticKind::UnknownNodeType:       { return "unknown-node-type"; }
            case piper::DiagnosticKind::AttributeMissing:      { return "attribute-missing"; }
            case piper::DiagnosticKind::AttributeAdded:        { return "attribute-added"; }
            case piper::DiagnosticKind::AttributeDrift:        { return "attribute-drift"; }
            case piper::DiagnosticKind::LinkOrphanedNode:      { return "link-orphaned-node"; }
            case piper::DiagnosticKind::LinkOrphanedAttribute: { return "link-orphaned-attribute"; }
            case piper::DiagnosticKind::LinkTypeMismatch:      { return "link-type-mismatch"; }
            case piper::DiagnosticKind::OrphanModeReference:   { return "orphan-mode-reference"; }
            case piper::DiagnosticKind::UnknownStageReference: { return "unknown-stage-reference"; }
        }
        return "unknown";
    }
}

int main(int argc, char* argv[])
{
    argparse::ArgumentParser cli("piper-migrate", "0.1.0");
    cli.add_description("Convert legacy Qt5-era Piper JSON to the current "
                        ".piper file format.");

    cli.add_argument("input")
        .help("input legacy JSON file");

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
    // PR 3.2 wires `register_builtin_nodes(registry)` here so the reader
    // can map V1's flat attribute key/value pairs onto typed AttributeSpecs.

    piper::migrate::Options opts;
    opts.strict = strict;

    try
    {
        std::string const  v1_json = read_file(input);
        auto               result  = piper::migrate::read_v1(v1_json, registry, opts);

        for (auto const& d : result.diagnostics)
        {
            std::fprintf(stderr, "warning [%s]: %s\n",
                         kind_str(d.kind), d.message.c_str());
        }

        if (dry_run)
        {
            std::fprintf(stdout, "piper-migrate: dry-run, %zu diagnostic(s)\n",
                         result.diagnostics.size());
            return 0;
        }

        std::string const v2_json = piper::v2::serialize(result.graph);
        write_file(output, v2_json);
        std::fprintf(stdout, "piper-migrate: wrote %s\n", output.string().c_str());
        return 0;
    }
    catch (std::exception const& e)
    {
        std::fprintf(stderr, "piper-migrate: %s\n", e.what());
        return 1;
    }
}
