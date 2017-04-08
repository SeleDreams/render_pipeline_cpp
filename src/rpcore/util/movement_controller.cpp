#include "render_pipeline/rpcore/util/movement_controller.h"

#include <parametricCurveCollection.h>
#include <genericAsyncTask.h>
#include <mouseWatcher.h>
#include <buttonThrower.h>
#include <asyncTaskManager.h>
#include <graphicsWindow.h>
#include <curveFitter.h>

#include <boost/format.hpp>

#include "render_pipeline/rppanda/showbase/showbase.h"
#include "render_pipeline/rplibs/py_to_cpp.hpp"

namespace rpcore {

struct MovementController::Impl
{
    /** Internal update method. */
    static AsyncTask::DoneStatus update(GenericAsyncTask* task, void* user_data);

    static AsyncTask::DoneStatus camera_motion_update(GenericAsyncTask* task, void* user_data);

    Impl(MovementController& self, rppanda::ShowBase* showbase);

public:
    MovementController& self_;

    rppanda::ShowBase* showbase_;

    LVecBase3 movement_ = LVecBase3(0);
    LVecBase3 velocity_ = LVecBase3(0);
    LVecBase2 hpr_movement_ = LVecBase2(0);
    float speed_ = 0.4f;
    LVecBase3 initial_position_ = LVecBase3(0);
    LVecBase3 initial_destination_ = LVecBase3(0);
    LVecBase3 initial_hpr_ = LVecBase3(0);
    bool mouse_enabled_ = false;
    LVecBase2 last_mouse_pos_ = LVecBase2(0);
    float mouse_sensivity_ = 0.7f;
    float keyboard_hpr_speed_ = 0.4f;
    bool use_hpr_ = false;
    float smoothness_ = 6.0f;
    float bobbing_amount_ = 1.5f;
    float bobbing_speed_ = 0.5f;
    LVecBase2 current_mouse_pos_ = LVecBase2(0);

    PT(AsyncTask) update_task_;

    PT(ParametricCurveCollection) curve_;
    double curve_time_start_;
    double curve_time_end_;
    double delta_time_sum_;
    double delta_time_count_;
};

AsyncTask::DoneStatus MovementController::Impl::update(GenericAsyncTask* task, void* user_data)
{
    Impl* mc = reinterpret_cast<Impl*>(user_data);

    double delta = mc->self_.get_clock_obj()->get_dt();

    NodePath camera = mc->showbase_->get_camera();

    // Update mouse first
    if (mc->showbase_->get_mouse_watcher_node()->has_mouse())
    {
        const LVecBase2& mouse_pos = mc->showbase_->get_mouse_watcher_node()->get_mouse();

        mc->current_mouse_pos_ = LVecBase2(
            mouse_pos[0] * mc->showbase_->get_cam_lens()->get_fov().get_x(),
            mouse_pos[1] * mc->showbase_->get_cam_lens()->get_fov().get_y()) * mc->mouse_sensivity_;

        if (mc->mouse_enabled_)
        {
            float diffx = mc->last_mouse_pos_[0] - mc->current_mouse_pos_[0];
            float diffy = mc->last_mouse_pos_[1] - mc->current_mouse_pos_[1];

            // Don't move in the very beginning
            if (mc->last_mouse_pos_[0] == 0 && mc->last_mouse_pos_[1] == 0)
            {
                diffx = 0;
                diffy = 0;
            }

            camera.set_h(camera.get_h() + diffx);
            camera.set_p(camera.get_p() - diffy);
        }

        mc->last_mouse_pos_ = mc->current_mouse_pos_;
    }

    // Compute movement in render space
    const LVecBase3& movement_direction = LVecBase3(mc->movement_[1], mc->movement_[0], 0.0f) * mc->speed_ * delta * 100.0f;

    // transform by the camera direction
    const LQuaternionf camera_quaternion(camera.get_quat(mc->showbase_->get_render()));
    LVecBase3 translated_direction(camera_quaternion.xform(movement_direction));

    // z-force is inddpendent of camera direction
    translated_direction.add_z(mc->movement_[2] * delta * 120.0f * mc->speed_);

    mc->velocity_ += translated_direction * 0.15f;

    // apply the new position
    camera.set_pos(camera.get_pos() + mc->velocity_);

    // transform rotation (keyboard keys)
    float rotation_speed = mc->keyboard_hpr_speed_ * 100.0f;
    rotation_speed *= delta;
    camera.set_hpr(camera.get_hpr() +
        LVecBase3(mc->hpr_movement_[0], mc->hpr_movement_[1], 0.0f) * rotation_speed);

    // fade out velocity
    mc->velocity_ = mc->velocity_ * (std::max)(0.0, 1.0f - delta * 60.0f / (std::max)(0.01f, mc->smoothness_));

    // bobbing
    double ftime = mc->self_.get_clock_obj()->get_frame_time();
    float rotation = rplibs::py_fmod(float(ftime), mc->bobbing_speed_) / mc->bobbing_speed_;
    rotation = ((std::min)(rotation, 1.0f - rotation) * 2.0f - 0.5f) * 2.0f;
    if (mc->velocity_.length_squared() > 1e-5 && mc->speed_ > 1e-5)
    {
        rotation *= mc->bobbing_amount_;
        rotation *= (std::min)(1.0f, mc->velocity_.length()) / mc->speed_ * 0.5f;
    }
    else
    {
        rotation = 0.0f;
    }
    camera.set_r(rotation);

    return AsyncTask::DS_cont;
}

AsyncTask::DoneStatus MovementController::Impl::camera_motion_update(GenericAsyncTask* task, void* user_data)
{
    Impl* mc = reinterpret_cast<Impl*>(user_data);

    if (mc->self_.get_clock_obj()->get_frame_time() > mc->curve_time_end_)
    {
        std::cout << "Camera motion path finished" << std::endl;

        // Print performance stats
        double avg_ms = mc->delta_time_sum_ / mc->delta_time_count_;
        std::cout << (boost::format("Average frame time (ms): %4.1f") % (avg_ms * 1000.0)) << std::endl;
        std::cout << (boost::format("Average frame rate: %4.1f") % (1.0 / avg_ms)) << std::endl;

        mc->update_task_ = mc->showbase_->add_task(update, mc, "RP_UpdateMovementController", -50);
        mc->showbase_->get_render_2d().show();
        mc->showbase_->get_aspect_2d().show();

        return AsyncTask::DS_done;
    }

    double lerp = (mc->self_.get_clock_obj()->get_frame_time() - mc->curve_time_start_) / (mc->curve_time_end_ - mc->curve_time_start_);
    lerp *= mc->curve_->get_max_t();

    LPoint3 pos(0);
    LVecBase3 hpr(0);
    mc->curve_->evaluate_xyz(lerp, pos);
    mc->curve_->evaluate_hpr(lerp, hpr);

    mc->showbase_->get_camera().set_pos(pos);
    mc->showbase_->get_camera().set_hpr(hpr);

    mc->delta_time_sum_ += mc->self_.get_clock_obj()->get_dt();
    mc->delta_time_count_ += 1;

    return AsyncTask::DS_cont;
}

MovementController::Impl::Impl(MovementController& self, rppanda::ShowBase* showbase): self_(self), showbase_(showbase)
{
}

// ************************************************************************************************
MovementController::MovementController(rppanda::ShowBase* showbase): impl_(std::make_unique<Impl>(*this, showbase))
{
}

MovementController::~MovementController(void)
{
    impl_->showbase_->get_task_mgr()->remove(impl_->update_task_);
}

void MovementController::reset_to_initial(void)
{
    NodePath camera = impl_->showbase_->get_camera();
    camera.set_pos(impl_->initial_position_);

    if (impl_->use_hpr_)
    {
        camera.set_hpr(impl_->initial_hpr_);
    }
    else
    {
        camera.look_at(impl_->initial_destination_.get_x(), impl_->initial_destination_.get_y(), impl_->initial_destination_.get_z());
    }
}

ClockObject* MovementController::get_clock_obj(void)
{
    return impl_->showbase_->get_task_mgr()->get_clock();
}

void MovementController::setup(void)
{
    auto showbase = impl_->showbase_;

	// x
	showbase->accept("raw-w",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_movement(0, 1);
	}, this);
	showbase->accept("raw-w-up",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_movement(0, 0);
	}, this);
	showbase->accept("raw-s",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_movement(0, -1);
	}, this);
	showbase->accept("raw-s-up",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_movement(0, 0);
	}, this);

	// y
	showbase->accept("raw-a",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_movement(1, -1);
	}, this);
	showbase->accept("raw-a-up",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_movement(1, 0);
	}, this);
	showbase->accept("raw-d",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_movement(1, 1);
	}, this);
	showbase->accept("raw-d-up",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_movement(1, 0);
	}, this);

	// z
	showbase->accept("space",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_movement(2, 1);
	}, this);
	showbase->accept("space-up",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_movement(2, 0);
	}, this);
	showbase->accept("shift",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_movement(2, -1);
	}, this);
	showbase->accept("shift-up",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_movement(2, 0);
	}, this);

	// wireframe + debug + buffer viewer
	showbase->accept("f3",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->impl_->showbase_->toggle_wireframe();
	}, this);
	showbase->accept("f11",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->impl_->showbase_->get_win()->save_screenshot("screenshot.png");
	}, this);
	showbase->accept("j",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->print_position();
	}, this);

	// mouse
	showbase->accept("mouse1",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_mouse_enabled(true);
	}, this);
	showbase->accept("mouse1-up",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_mouse_enabled(false);
	}, this);

	// arrow mouse navigation
	showbase->accept("arrow_up",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_hpr_movement(1, 1);
	}, this);
	showbase->accept("arrow_up-up",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_hpr_movement(1, 0);
	}, this);
	showbase->accept("arrow_down",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_hpr_movement(1, -1);
	}, this);
	showbase->accept("arrow_down-up",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_hpr_movement(1, 0);
	}, this);
	showbase->accept("arrow_left",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_hpr_movement(0, 1);
	}, this);
	showbase->accept("arrow_left-up",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_hpr_movement(0, 0);
	}, this);
	showbase->accept("arrow_right",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_hpr_movement(0, -1);
	}, this);
	showbase->accept("arrow_right-up",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->set_hpr_movement(0, 0);
	}, this);

	// increase / decrease speed
	showbase->accept("+",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->increase_speed();
	}, this);
	showbase->accept("-",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->decrease_speed();
	}, this);

	// disable modifier buttons to be able to move while pressing shift for example
	showbase->get_mouse_watcher_node()->set_modifier_buttons(ModifierButtons());
	DCAST(ButtonThrower, showbase->get_button_thrower().node())->set_modifier_buttons(ModifierButtons());

	// disable pandas builtin mouse control
	showbase->disable_mouse();

	// add ourself as an update task which gets executed very early before the rendering
	impl_->update_task_ = showbase->add_task(&Impl::update, impl_.get(), "RP_UpdateMovementController", -40);

	// Hotkeys to connect to pstats and reset the initial position
	showbase->accept("1",
		[](const Event* ev, void* data) {
		PStatClient::connect();
	}, this);
	showbase->accept("3",
		[](const Event* ev, void* data) {
		reinterpret_cast<MovementController*>(data)->reset_to_initial();
	}, this);
}

void MovementController::print_position(void)
{
    const LVecBase3f& pos = impl_->showbase_->get_cam().get_pos(impl_->showbase_->get_render());
    const LVecBase3f& hpr = impl_->showbase_->get_cam().get_hpr(impl_->showbase_->get_render());
    printf("(Vec3(%f, %f, %f), Vec3(%f, %f, %f))\n", pos.get_x(), pos.get_y(), pos.get_z(), hpr.get_x(), hpr.get_y(), hpr.get_z());
}

void MovementController::play_motion_path(const MotionPathType& points, float point_duration)
{
	CurveFitter fitter;
	for (size_t k=0, k_end=points.size(); k < k_end; ++k)
		fitter.add_xyz_hpr(k, points[k].first, points[k].second);

	fitter.compute_tangents(1.0f);
	PT(ParametricCurveCollection) curve = fitter.make_hermite();
	std::cout << "Starting motion path with " << points.size() << " CVs" << std::endl;

    impl_->showbase_->get_render_2d().hide();
    impl_->showbase_->get_aspect_2d().hide();

	impl_->curve_ = curve;
	impl_->curve_time_start_ = get_clock_obj()->get_frame_time();
	impl_->curve_time_end_ = get_clock_obj()->get_frame_time() + points.size() * point_duration;
	impl_->delta_time_sum_ = 0;
	impl_->delta_time_count_ = 0;
	impl_->showbase_->add_task(&Impl::camera_motion_update, impl_.get(), "RP_CameraMotionPath", -50);
	impl_->showbase_->get_task_mgr()->remove(impl_->update_task_);
}

void MovementController::set_initial_position(const LVecBase3& pos, const LVecBase3& target)
{
    impl_->initial_position_ = pos;
    impl_->initial_destination_ = target;
    impl_->use_hpr_ = false;
    reset_to_initial();
}

void MovementController::set_initial_position_hpr(const LVecBase3& pos, const LVecBase3& hpr)
{
    impl_->initial_position_ = pos;
    impl_->initial_hpr_ = hpr;
    impl_->use_hpr_ = true;
    reset_to_initial();
}

float MovementController::get_speed(void) const
{
    return impl_->speed_;
}

void MovementController::set_movement(int direction, float amount)
{
    impl_->movement_[direction] = amount;
}

void MovementController::set_hpr_movement(int direction, float amount)
{
    impl_->hpr_movement_[direction] = amount;
}

void MovementController::set_mouse_enabled(bool enabled)
{
    impl_->mouse_enabled_ = enabled;
}

void MovementController::increase_speed(void)
{
    impl_->speed_ *= 1.4f;
}

void MovementController::decrease_speed(void)
{
    impl_->speed_ *= 0.6f;
}

void MovementController::set_speed(float speed)
{
    impl_->speed_ = speed;
}

void MovementController::increase_bobbing_amount(void)
{
    impl_->bobbing_amount_ *= 1.4f;
}

void MovementController::decrease_bobbing_amount(void)
{
    impl_->bobbing_amount_ *= 0.6f;
}

void MovementController::increase_bobbing_speed(void)
{
    impl_->bobbing_speed_ *= 1.4f;
}

void MovementController::decrease_bobbing_speed(void)
{
    impl_->bobbing_speed_ *= 0.6f;
}

void MovementController::set_bobbing_amount(float amount)
{
    impl_->bobbing_amount_ = amount;
}

void MovementController::set_bobbing_speed(float speed)
{
    impl_->bobbing_speed_ = speed;
}

}
