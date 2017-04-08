#pragma once

#include <render_pipeline/rpcore/render_stage.h>

namespace rpplugins {

class ColorCorrectionStage: public rpcore::RenderStage
{
public:
	ColorCorrectionStage(rpcore::RenderPipeline& pipeline): RenderStage(pipeline, "ColorCorrectionStage") {}

	virtual RequireType& get_required_inputs(void) const override { return required_inputs; }
	virtual RequireType& get_required_pipes(void) const override { return required_pipes; }
	virtual ProduceType get_produced_pipes(void) const override;

	virtual void create(void) override;
	virtual void reload_shaders(void) override;

private:
	virtual std::string get_plugin_id(void) const override;

	static RequireType required_inputs;
	static RequireType required_pipes;

    bool stereo_mode_ = false;

	std::shared_ptr<rpcore::RenderTarget> target_ = nullptr;
};

}	// namespace rpplugins
