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

#pragma once

#include <memory>
#include <vector>

#include <render_pipeline/rpcore/rpobject.hpp>

namespace rpcore {

class RenderStage;
class RenderTarget;
class Image;

/**
 * Util class for filtering cubemaps, provides funcionality to generate
 * a specular and diffuse IBL cubemap.
 */
class RENDER_PIPELINE_DECL CubemapFilter : public RPObject
{
public:
    // Fixed size for the diffuse cubemap, since it does not contain much detail
    static const int DIFFUSE_CUBEMAP_SIZE = 10;
    static const int PREFILTER_CUBEMAP_SIZE = 32;

    CubemapFilter(RenderStage* stage, const std::string& name="Cubemap", int size=128);

    /**
     * Returns the generated specular cubemap. The specular cubemap is
     * mipmapped and provides the specular IBL components of the input cubemap.
     */
    Image* get_specular_cubemap() const;

    /**
     * Returns the generated diffuse cubemap. The diffuse cubemap has no
     * mipmaps and contains the filtered diffuse component of the input cubemap.
     */
    Image* get_diffuse_cubemap() const;

    /** Returns the target where the caller should write the initial cubemap data to. */
    Image* get_target_cubemap() const;

    /**
     * Returns the size of the created cubemap, previously passed to the
     * constructor of the filter.
     */
    int get_size() const;

    /**
     * Creates the filter. The input cubemap should be mipmapped, and will
     * get reused for the specular cubemap.
     */
    void create();

    /** Sets all required shaders on the filter. */
    void reload_shaders();

private:
    /** Internal method to create the cubemap storage. */
    void make_maps();

    /** Internal method to create the specular mip chain. */
    void make_specular_targets();

    /** Internal method to create the diffuse cubemap. */
    void make_diffuse_target();

    RenderStage* _stage;
    const std::string _name;
    int _size;

    std::unique_ptr<Image> _prefilter_map;
    std::unique_ptr<Image> _diffuse_map;
    std::unique_ptr<Image> _spec_pref_map;
    std::unique_ptr<Image> _specular_map;

    std::vector<RenderTarget*> _targets_spec;
    std::vector<RenderTarget*> _targets_spec_filter;

    RenderTarget* _diffuse_target;
    RenderTarget* _diff_filter_target;
};

// ************************************************************************************************
inline Image* CubemapFilter::get_specular_cubemap() const
{
    return _specular_map.get();
}

inline Image* CubemapFilter::get_diffuse_cubemap() const
{
    return _diffuse_map.get();
}

inline Image* CubemapFilter::get_target_cubemap() const
{
    return _specular_map.get();
}

inline int CubemapFilter::get_size() const
{
    return _size;
}

}
