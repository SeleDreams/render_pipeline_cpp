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

#include <render_pipeline/rpcore/render_stage.hpp>

#include <camera.h>
#include <orthographicLens.h>

namespace rpplugins {

/** This stage generates a depth map using Variance Shadow Maps for very distant objects. */
class PSSMDistShadowStage : public rpcore::RenderStage
{
public:
    PSSMDistShadowStage(rpcore::RenderPipeline& pipeline);

    RequireType& get_required_inputs() const final { return required_inputs; }
    RequireType& get_required_pipes() const final { return required_pipes; }
    ProduceType get_produced_inputs() const final;
    ProduceType get_produced_pipes() const final;

    RENDER_PIPELINE_STAGE_DOWNCAST();

    void create() final;
    void update() final;
    void reload_shaders() final;

    void set_shader_input(const ShaderInput& inp) final;

    LMatrix4 get_mvp() const;

    void set_resolution(int resolution);
    void set_clip_size(float clip_size);
    void set_sun_distance(float sun_distance);
    void set_sun_vector(const LVecBase3& sun_vector);

private:
    std::string get_plugin_id() const final;

    static RequireType required_inputs;
    static RequireType required_pipes;

    int _resolution = 2048;
    float _clip_size = 500.0f;
    float _sun_distance = 8000;
    LVecBase3 _sun_vector = LVecBase3(0, 0, 1);
    PTA_LMatrix4f _pta_mvp;

    PT(Camera) _camera;
    PT(OrthographicLens) _cam_lens;
    NodePath _cam_node;

    rpcore::RenderTarget* _target;
    rpcore::RenderTarget* _target_convert;
    rpcore::RenderTarget* _target_blur_v;
    rpcore::RenderTarget* _target_blur_h;
};

inline void PSSMDistShadowStage::set_resolution(int resolution)
{
    _resolution = resolution;
}

inline void PSSMDistShadowStage::set_clip_size(float clip_size)
{
    _clip_size = clip_size;
}

inline void PSSMDistShadowStage::set_sun_distance(float sun_distance)
{
    _sun_distance = sun_distance;
}

inline void PSSMDistShadowStage::set_sun_vector(const LVecBase3& sun_vector)
{
    _sun_vector = sun_vector;
}

}    // namespace rpplugins
