/**
 * Render Pipeline C++
 *
 * Copyright (c) 2014-2016 tobspr <tobias.springer1@gmail.com>
 * Copyright (c) 2016-2017 Center of Human-centered Interaction for Coexistence.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this software
 * and associated documentation files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT
 * LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "rpcore/util/display_shader_builder.hpp"

#include <texture.h>

#include <fmt/format.h>

#include "render_pipeline/rppanda/stdpy/file.hpp"
#include "render_pipeline/rpcore/loader.hpp"
#include "render_pipeline/rpcore/image.hpp"

namespace rpcore {

PT(Shader) DisplayShaderBuilder::build(Texture* texture, int view_width, int view_height)
{
    const std::string& cache_key = fmt::format("/$$rptemp/$$TEXDISPLAY-X{}-Y{}-Z{}-TT{}-CT{}-VW{}-VH{}.frag.glsl",
        texture->get_x_size(),
        texture->get_y_size(),
        texture->get_z_size(),
        texture->get_texture_type(),
        texture->get_component_type(),
        view_width,
        view_height);

    // Only regenerate the file when there is no cache entry for it
    if (!rppanda::isfile(cache_key))
    {
        const std::string& fragment_shader = build_fragment_shader(texture, view_width, view_height);

        try
        {
            (*rppanda::open_write_file(cache_key, false, true)) << fragment_shader;
        }
        catch (const std::exception& err)
        {
            RPObject::global_error("DisplayShaderBuilder", std::string("Error writing processed shader: ") + err.what());
        }
    }

    return RPLoader::load_shader({"/$$rp/shader/default_gui_shader.vert.glsl", cache_key});
}

std::string DisplayShaderBuilder::build_fragment_shader(Texture* texture, int view_width, int view_height)
{
    const auto& sampling_code_type = generate_sampling_code(texture, view_width, view_height);

    // Build actual shader
    const std::string& built = fmt::format(
        "#version 430\n"
        "#pragma include \"render_pipeline_base.inc.glsl\"\n"
        "in vec2 texcoord;\n"
        "out vec3 result;\n"
        "uniform int mipmap;\n"
        "uniform int slice;\n"
        "uniform float brightness;\n"
        "uniform bool tonemap;\n"
        "uniform {} p3d_Texture0;\n"
        "void main() {{\n"
        "    int view_width = {};\n"
        "    int view_height = {};\n"
        "    ivec2 display_coord = ivec2(texcoord * vec2(view_width, view_height));\n"
        "    int int_index = display_coord.x + display_coord.y * view_width;\n"
        "    {}\n"
        "    result *= brightness;\n"
        "    if (tonemap)\n"
        "        result = result / (1 + result);\n"
        "}}\n",
        sampling_code_type.second,
        view_width,
        view_height,
        sampling_code_type.first);

    // Strip trailing spaces


    return built;
}

#define IS_IN(VALUE, CONTAINER) (std::find(CONTAINER.begin(), CONTAINER.end(), VALUE) != CONTAINER.end())

std::pair<std::string, std::string> DisplayShaderBuilder::generate_sampling_code(Texture* texture, int view_width, int view_height)
{
    Texture::TextureType texture_type = texture->get_texture_type();
    Texture::ComponentType comp_type = texture->get_component_type();

    // Useful snippets
    const std::string int_coord = "ivec2 int_coord = ivec2(texcoord * textureSize(p3d_Texture0, mipmap).xy);";
    const std::string slice_count = "int slice_count = textureSize(p3d_Texture0, 0).z;";

    std::vector<Texture::ComponentType> float_types = { Texture::T_float, Texture::T_unsigned_byte };
    std::vector<Texture::ComponentType> int_types = { Texture::T_int, Texture::T_unsigned_short, Texture::T_unsigned_int_24_8 };

    std::pair<std::string, std::string> result = {"result = vec3(1, 0, 1);", "sampler2D"};

    if (std::find(float_types.begin(), float_types.end(), comp_type) == float_types.end() &&
        std::find(int_types.begin(), int_types.end(), comp_type) == int_types.end())
        RPObject::global_warn("DisplayShaderBuilder", std::string("Unkown texture component type: ") + std::to_string(comp_type));


    switch (texture_type)
    {
    // 2D Textures
    case Texture::TT_2d_texture:
        {
            if (IS_IN(comp_type, float_types))
                result = {"result = textureLod(p3d_Texture0, texcoord, mipmap).xyz;", "sampler2D"};
            else if (IS_IN(comp_type, int_types))
                result = {int_coord + "result = texelFetch(p3d_Texture0, int_coord, mipmap).xyz / 10.0;", "isampler2D"};
            break;
        }
    // Buffer Textures
    case Texture::TT_buffer_texture:
        {
            auto range_check = [](const std::string& code) {
                return std::string("if (int_index < textureSize(p3d_Texture0)) {") + code + "} else { result = vec3(1.0, 0.6, 0.2);};";
            };

            if (IS_IN(comp_type, float_types))
                result = {range_check("result = texelFetch(p3d_Texture0, int_index).xyz;"), "samplerBuffer"};
            else if (IS_IN(comp_type, int_types))
                result = {range_check("result = texelFetch(p3d_Texture0, int_index).xyz / 10.0;"), "isamplerBuffer"};
            break;
        }
    // 3D Textures
    case Texture::TT_3d_texture:
        {
            if (IS_IN(comp_type, float_types))
                result = {slice_count + "result = textureLod(p3d_Texture0, vec3(texcoord, (0.5 + slice) / slice_count), mipmap).xyz;", "sampler3D"};
            else if (IS_IN(comp_type, int_types))
                result = {int_coord + "result = texelFetch(p3d_Texture0, ivec3(int_coord, slice), mipmap).xyz / 10.0;", "isampler3D"};
            break;
        }
    // 2D Texture Array
    case Texture::TT_2d_texture_array:
        {
            if (IS_IN(comp_type, float_types))
                result = {"result = textureLod(p3d_Texture0, vec3(texcoord, slice), mipmap).xyz;", "sampler2DArray"};
            else if (IS_IN(comp_type, int_types))
                result = {int_coord + "result = texelFetch(p3d_Texture0, ivec3(int_coord, slice), mipmap).xyz / 10.0;", "isampler2DArray"};
            break;
        }
    // Cubemap
    case Texture::TT_cube_map:
        {
            std::string code("vec3 sample_dir = get_cubemap_coordinate(slice, texcoord*2-1);\n");
            code += "result = textureLod(p3d_Texture0, sample_dir, mipmap).xyz;";
            result = {code, "samplerCube"};
            break;
        }
    // Cubemap array
    case Texture::TT_cube_map_array:
        {
            std::string code("vec3 sample_dir = get_cubemap_coordinate(slice % 6, texcoord*2-1);\n");
            code += "result = textureLod(p3d_Texture0, vec4(sample_dir, slice / 6), mipmap).xyz;";
            result = {code, "samplerCubeArray"};
            break;
        }
    default:
        RPObject::global_warn("DisplayShaderBuilder", std::string("Unhandled texture type ") +  std::to_string(texture_type) + " in display shader builder");
    }

    return result;
}

}
