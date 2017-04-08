#pragma once

#include <render_pipeline/rpcore/pluginbase/base_plugin.h>

#include "apply_clouds_stage.h"

namespace rpplugins {

class Plugin: public rpcore::BasePlugin
{
public:
    Plugin(rpcore::RenderPipeline& pipeline);

    RequrieType& get_required_plugins(void) const final { return require_plugins; }

    void on_stage_setup(void) final;
    void on_pipeline_created(void) final;

private:
    static RequrieType require_plugins;

    std::shared_ptr<ApplyCloudsStage> _apply_stage;
};

}
