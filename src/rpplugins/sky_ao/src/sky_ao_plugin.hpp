#pragma once

#include <render_pipeline/rpcore/pluginbase/base_plugin.h>

namespace rpplugins {

class SkyAOPlugin: public rpcore::BasePlugin
{
public:
    SkyAOPlugin(rpcore::RenderPipeline& pipeline);

    RequrieType& get_required_plugins(void) const;

    void on_stage_setup(void) override;
    void on_post_stage_setup(void) override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}
