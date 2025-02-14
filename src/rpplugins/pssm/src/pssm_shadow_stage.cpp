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

#include "pssm_shadow_stage.hpp"

#include <graphicsBuffer.h>

#include <render_pipeline/rpcore/render_target.hpp>
#include <render_pipeline/rpcore/globals.hpp>

namespace rpplugins {

PSSMShadowStage::RequireType PSSMShadowStage::required_inputs;
PSSMShadowStage::RequireType PSSMShadowStage::required_pipes;

PSSMShadowStage::ProduceType PSSMShadowStage::get_produced_pipes() const
{
    return {
        ShaderInput("PSSMShadowAtlas", _target->get_depth_tex()),
        ShaderInput("PSSMShadowAtlasPCF", _target->get_depth_tex(), make_pcf_state()),
    };
}

SamplerState PSSMShadowStage::make_pcf_state() const
{
    SamplerState state;
    state.set_minfilter(SamplerState::FT_shadow);
    state.set_magfilter(SamplerState::FT_shadow);
    return state;
}

Texture* PSSMShadowStage::get_shadow_tex() const
{
    return _target->get_depth_tex();
}

void PSSMShadowStage::create()
{
    _target = create_target("ShadowMap");
    _target->set_size(_split_resolution * _num_splits, _split_resolution);
    _target->add_depth_attachment(32);
    _target->prepare_render(NodePath());

    // Remove all unused display regions
    GraphicsBuffer* internal_buffer = _target->get_internal_buffer();
    internal_buffer->remove_all_display_regions();
    internal_buffer->get_display_region(0)->set_active(false);
    internal_buffer->disable_clears();

    // Set a clear on the buffer instead on all regions
    internal_buffer->set_clear_depth(1);
    internal_buffer->set_clear_depth_active(true);

    // Prepare the display regions
    for (int i=0; i < _num_splits; ++i)
    {
        PT(DisplayRegion) region = internal_buffer->make_display_region(
            i / float(_num_splits),
            i / float(_num_splits) + 1 / float(_num_splits), 0, 1);
        region->set_sort(25 + i);
        region->disable_clears();
        region->set_active(true);
        _split_regions.push_back(region);
    }
}

void PSSMShadowStage::set_shader_input(const ShaderInput& inp)
{
    rpcore::Globals::render.set_shader_input(inp);
}

std::string PSSMShadowStage::get_plugin_id() const
{
    return RPPLUGINS_ID_STRING;
}

}    // namespace rpplugins
