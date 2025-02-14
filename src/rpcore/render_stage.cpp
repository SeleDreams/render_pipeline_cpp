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

#include "render_pipeline/rpcore/render_stage.hpp"

#include <graphicsWindow.h>

#include <fmt/format.h>
#include <fmt/ostream.h>

#include "render_pipeline/rpcore/loader.hpp"
#include "render_pipeline/rpcore/render_target.hpp"
#include "render_pipeline/rpcore/render_pipeline.hpp"
#include "render_pipeline/rpcore/pluginbase/manager.hpp"
#include "render_pipeline/rpcore/pluginbase/base_plugin.hpp"
#include "render_pipeline/rpcore/globals.hpp"
#include "render_pipeline/rpcore/stage_manager.hpp"
#include "render_pipeline/rpcore/image.hpp"
#include "render_pipeline/rpcore/util/shader_input_blocks.hpp"
#include "render_pipeline/rppanda/showbase/showbase.hpp"

namespace rpcore {

RenderStage::RenderStage(RenderPipeline& pipeline, boost::string_view stage_id): RPObject(stage_id), pipeline_(pipeline), stage_id_(stage_id)
{
}

RenderStage::RenderStage(RenderStage&&) = default;

RenderStage::~RenderStage() = default;

RenderStage::ProduceType RenderStage::get_produced_inputs() const
{
    return {};
}

RenderStage::ProduceType RenderStage::get_produced_pipes() const
{
    return {};
}

RenderStage::DefinesType RenderStage::get_produced_defines() const
{
    return {};
}

void RenderStage::set_shader_input(const ShaderInput& inp)
{
    for (const auto& target: targets_)
        target.second->set_shader_input(inp);
}

void RenderStage::set_active(bool state)
{
    if (active_ != state)
    {
        active_ = state;
        for (const auto& target: targets_)
            target.second->set_active(active_);
    }
}

RenderTarget* RenderStage::create_target(boost::string_view name)
{
    const std::string& target_name = fmt::format("{}:{}:{}", get_plugin_id(), stage_id_, name);

    if (targets_.find(target_name) != targets_.end())
    {
        error("Overriding existing target: " + target_name);
        return nullptr;
    }

    return targets_.emplace(target_name, std::make_unique<RenderTarget>(target_name)).first->second.get();
}

void RenderStage::remove_target(RenderTarget* target)
{
    target->remove();
    for (const auto& key_value: targets_)
    {
        if (target == key_value.second.get())
        {
            targets_.erase(key_value.first);
            break;
        }
    }
}

PT(Shader) RenderStage::load_shader(const std::vector<Filename>& args, bool stereo_post, bool use_post_gs) const
{
    return get_shader_handle("/$$rp/shader/", args, stereo_post, use_post_gs);
}

PT(Shader) RenderStage::load_plugin_shader(const std::vector<Filename>& args, bool stereo_post, bool use_post_gs) const
{
    const Filename& shader_path = pipeline_.get_plugin_mgr()->get_instance(get_plugin_id())->get_shader_resource("");
    return get_shader_handle(shader_path, args, stereo_post, use_post_gs);
}

void RenderStage::handle_window_resize()
{
    set_dimensions();
    for (const auto& target: targets_)
        target.second->consider_resize();
}

std::pair<std::unique_ptr<Image>, std::unique_ptr<Image>> RenderStage::prepare_upscaler(int max_invalid_pixels) const
{
    std::unique_ptr<Image> counter = Image::create_counter(get_stage_id() + "-BadPixelsCounter");
    counter->set_clear_color(LColor(0));
    std::unique_ptr<Image> buf = Image::create_buffer(get_stage_id() + "-BadPixels", max_invalid_pixels, "R32I");
    return std::make_pair(std::move(counter), std::move(buf));
}

PT(Shader) RenderStage::get_shader_handle(const Filename& base_path, const std::vector<Filename>& args, bool stereo_post, bool use_post_gs) const
{
    static const char* prefix[] ={"/$$rpconfig", "/$$rp/shader", "/$$rptemp"};

    if (args.size() <= 0 || args.size() > 3)
        throw std::runtime_error("args.size() <= 0 || args.size() > 3");

    std::vector<Filename> path_args;

    for (const auto& source: args)
    {
        bool found = false;
        for (size_t k = 0; k < std::extent<decltype(prefix)>::value; k++)
        {
            if (source.to_os_generic().find(prefix[k]) != std::string::npos)
            {
                found = true;
                path_args.push_back(source);
                break;
            }
        }

        if (!found)
            path_args.push_back(base_path / source);
    }

    if (args.size() == 1)
    {
        if (stereo_post)
        {
            path_args.insert(path_args.begin(), "/$$rp/shader/default_post_process_stereo.vert.glsl");

            // NVIDIA single stereo rendering extension OR
            // force layered rendering.
            if (pipeline_.get_stage_mgr()->get_defines().at("NVIDIA_STEREO_VIEW") == "0" || use_post_gs)
                path_args.insert(path_args.end(), "/$$rp/shader/default_post_process_stereo.geom.glsl");
        }
        else
        {
            path_args.insert(path_args.begin(), "/$$rp/shader/default_post_process.vert.glsl");
        }
    }

    return RPLoader::load_shader(path_args);
}

}
