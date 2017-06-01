#include "render_pipeline/rpcore/util/generic.hpp"

#include "render_pipeline/rplibs/py_to_cpp.hpp"

namespace rpcore {

LVecBase3 rgb_from_string(const std::string& text, float min_brightness)
{
    const size_t ohash = std::hash<std::string>{}(text);

    LVecBase3f rgb(
        (ohash & 0xFF),
        (ohash >> 8) & 0xFF,
        (ohash >> 16) & 0xFF);
    const float neg_inf = 1.0f - min_brightness;

    return ((rgb / 255.0f) * neg_inf) + min_brightness;
}

void snap_shadow_map(const LMatrix4f& mvp, NodePath cam_node, int resolution)
{
    auto _mvp = mvp;
    const LVecBase4f& base_point = _mvp.xform(LPoint4f(0, 0, 0, 1)) * 0.5f + 0.5f;
    float texel_size = 1.0f / float(resolution);
    float offset_x = rplibs::py_fmod(base_point.get_x(), texel_size);
    float offset_y = rplibs::py_fmod(base_point.get_y(), texel_size);
    _mvp.invert_in_place();
    const LVecBase4f& new_base = _mvp.xform(LPoint4f(
        (base_point.get_x() - offset_x) * 2.0 - 1.0,
        (base_point.get_y() - offset_y) * 2.0 - 1.0,
        (base_point.get_z()) * 2.0 - 1.0, 1));
    cam_node.set_pos(cam_node.get_pos() - new_base.get_xyz());
}

}
