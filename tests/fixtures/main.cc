#include <fstream>
#include <iostream>
#include <stdexcept>

#include "build_motor_simple.h"

#include "piper/builtin_nodes.h"
#include "piper/serialize_v2.h"

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: " << argv[0] << " <output_path>\n";
        return 1;
    }

    using namespace piper;

    NodeRegistry reg;
    register_builtin_nodes(reg);

    Graph graph;
    try
    {
        graph = fixtures::build_motor_simple(reg);
    }
    catch (std::exception const& e)
    {
        std::cerr << "build failed: " << e.what() << "\n";
        return 1;
    }

    std::ofstream out{argv[1]};
    if (not out.is_open())
    {
        std::cerr << "cannot open output: " << argv[1] << "\n";
        return 1;
    }
    out << v2::serialize(graph);

    return 0;
}
