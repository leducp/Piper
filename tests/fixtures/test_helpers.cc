#include "test_helpers.h"

namespace piper::fixtures
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

    NodeType make_adder()
    {
        NodeType nt;
        nt.type     = "add";
        nt.help     = "a + b";
        nt.category = "arithmetic";
        nt.attributes = {
            { "a",   "float", AttributeSpec::Role::Input,  ""    },
            { "b",   "float", AttributeSpec::Role::Input,  ""    },
            { "out", "float", AttributeSpec::Role::Output, ""    },
            { "k",   "float", AttributeSpec::Role::Member, "1.0" },
        };
        return nt;
    }

    NodeType make_simple_type()
    {
        NodeType nt;
        nt.type = "Simple";
        nt.attributes = {
            { "in",  "float", AttributeSpec::Role::Input,  "" },
            { "out", "float", AttributeSpec::Role::Output, "" },
        };
        return nt;
    }
}
