#pragma once

#include <glm/glm.hpp>

#include <map>

namespace Modification {
enum class Shape : int {
    VOXEL = 0,
    SPHERE = 1,
    CUBE = 2,
    CUBOID = 3,
    MAX_SHAPE,
};

enum class Type : int {
    ERASE = 0,
    PLACE = 1,
    REPLACE = 2,
    MAX_TYPE,
};

struct ShapeInfo {
    Shape shape;
    glm::vec4 additional;

    ShapeInfo(Shape s) : shape(s), additional(0.f) { }
    ShapeInfo(Shape s, glm::vec3 colour) : shape(s), additional(glm::vec4(colour, 0.f)) { }
    ShapeInfo(Shape s, glm::vec4 additional) : shape(s), additional(additional) { }
};

static std::map<Shape, const char*> shapeToString {
    { Shape::VOXEL,  "Single Voxel" },
    { Shape::SPHERE, "Sphere"       },
    { Shape::CUBE,   "Cube"         },
    { Shape::CUBOID, "Cuboid"       },
};

}
