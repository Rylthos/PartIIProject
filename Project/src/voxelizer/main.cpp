#include <CLI/CLI.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"

#include "parser.hpp"

int main(int argc, char** argv)
{
    CLI::App app { "Convert Meshes into Voxel formats" };
    argv = app.ensure_utf8(argv);

    ParserArgs args;

    app.add_option("filename", args.filename, "The file to parse");
    app.add_option("output", args.output, "Output directory");
    app.add_option("-r,--resolution", args.voxels_per_unit, "Number of voxels per unit");
    app.add_option("-n,--name", args.name, "Output name (Defaults to filename)");
    app.add_option("-u,--units", args.units, "Number of units the model should reside over");
    app.add_option("-f,--frames", args.frames, "Number of frames for animations");

    app.add_flag("-a", args.flag_all, "Enable all generators. Equivalent to -gtocb");
    app.add_flag("-g", args.flag_grid, "Enable grid generator");
    app.add_flag("-t", args.flag_texture, "Enable texture generator");
    app.add_flag("-o", args.flag_octree, "Enable octree generator");
    app.add_flag("-c", args.flag_contree, "Enable contree generator");
    app.add_flag("-b", args.flag_brickmap, "Enable brickmap generator");
    app.add_flag("--anim", args.animation, "Enable animation");

    CLI11_PARSE(app, argc, argv);

    Parser parser(args);

    return 0;
}
