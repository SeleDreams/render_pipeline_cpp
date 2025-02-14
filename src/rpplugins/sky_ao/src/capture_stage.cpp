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

#include "capture_stage.hpp"

#include <render_pipeline/rpcore/render_target.hpp>
#include <render_pipeline/rpcore/globals.hpp>
#include <render_pipeline/rppanda/showbase/showbase.hpp>
#include <render_pipeline/rpcore/render_pipeline.hpp>
#include <render_pipeline/rpcore/native/tag_state_manager.h>
#include <render_pipeline/rplibs/py_to_cpp.hpp>

namespace rpplugins {

SkyAOCaptureStage::RequireType SkyAOCaptureStage::required_inputs;
SkyAOCaptureStage::RequireType SkyAOCaptureStage::required_pipes;

SkyAOCaptureStage::ProduceType SkyAOCaptureStage::get_produced_pipes() const
{
    return {
        ShaderInput("SkyAOHeight", target_convert_->get_color_tex()),
    };
}

SkyAOCaptureStage::ProduceType SkyAOCaptureStage::get_produced_inputs() const
{
    return {
        ShaderInput("SkyAOCapturePosition", pta_position_),
    };
}

SkyAOCaptureStage::SkyAOCaptureStage(rpcore::RenderPipeline& pipeline): RenderStage(pipeline, "SkyAOCaptureStage")
{
    pta_position_ = PTA_LVecBase3f::empty_array(1);
}

void SkyAOCaptureStage::create()
{
    _camera = new Camera("SkyAOCaptureCam");
    _cam_lens = new OrthographicLens;
    _cam_lens->set_film_size(max_radius_, max_radius_);
    _cam_lens->set_near_far(0.0f, capture_height_);
    _camera->set_lens(_cam_lens);
    _cam_node = rpcore::Globals::base->get_render().attach_new_node(_camera);
    _cam_node.look_at(0, 0, -1);
    _cam_node.set_r(0);

    target_ = create_target("SkyAOCapture");
    target_->set_size(resolution_);
    target_->add_depth_attachment(16);
    target_->prepare_render(_cam_node);

    target_convert_ = create_target("ConvertDepth");
    target_convert_->set_size(resolution_);
    target_convert_->add_color_attachment(LVecBase4i(16, 0, 0, 0));
    target_convert_->prepare_buffer();

    target_convert_->set_shader_input(ShaderInput("DepthTex", target_->get_depth_tex()));
    target_convert_->set_shader_input(ShaderInput("position", pta_position_));

    // Register camera
    pipeline_.get_tag_mgr()->register_camera("shadow", _camera);
}

void SkyAOCaptureStage::update()
{
    static const float snap_size = max_radius_ / float(resolution_);
    const LPoint3& cam_pos = rpcore::Globals::base->get_cam().get_pos(rpcore::Globals::base->get_render());
    _cam_node.set_pos(
        cam_pos.get_x() - rplibs::py_fmod(cam_pos.get_x(), snap_size),
        cam_pos.get_y() - rplibs::py_fmod(cam_pos.get_y(), snap_size),
        capture_height_ / 2.0f);
    pta_position_[0] = _cam_node.get_pos();
}

void SkyAOCaptureStage::reload_shaders()
{
    target_convert_->set_shader(load_plugin_shader({"convert_depth.frag.glsl"}));
}

std::string SkyAOCaptureStage::get_plugin_id() const
{
    return RPPLUGINS_ID_STRING;
}

}    // namespace rpplugins
