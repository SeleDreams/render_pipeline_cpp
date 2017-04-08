#pragma once

#include <array>
#include <vector>
#include <string>
#include <memory>

#include <luse.h>

#include <render_pipeline/rpcore/rpobject.h>

namespace YAML {
class Node;
}

namespace rpcore {

class SmoothConnectedCurve;

/** DayBase setting type for all setting types. */
class DayBaseType: public RPObject
{
public:
	using ValueType = std::pair<LVecBase3f, int>;

public:
	DayBaseType(YAML::Node& data, const std::string& id="DayBaseType");
	virtual ~DayBaseType(void);

	virtual const std::string& get_glsl_type(void) const = 0;

	const std::string& get_type(void) const { return _type; }
	const std::string& get_label(void) const { return _label; }
	const std::string& get_description(void) const { return _description; }

	/** Returns the unscaled value at the given day time offset. */
	ValueType get_value_at(float offset) const;

	/** Returns the scaled value at a given day time offset. */
	ValueType get_scaled_value_at(float offset) const;

	/** Returns the scaled value from a given normalized value. */
	virtual ValueType get_scaled_value(const ValueType& values) const = 0;

	/** Sets the control points on the curves. */
	void set_control_points(const std::vector<std::vector<LVecBase2f>>& control_points);

	/** Serializes the setting to a yaml string. */
	std::string serialize(void) const;

protected:
	std::string _type;
	std::string _label;
	std::string _description;

	std::array<SmoothConnectedCurve*, 3> _curves;
};

/** Setting type storing a single scalar. */
class ScalarType: public DayBaseType
{
public:
	ScalarType(YAML::Node& data);

	virtual const std::string& get_glsl_type(void) const { return glsl_type; }

	/** Scales a linear value. */
	ValueType get_scaled_value(const ValueType& values) const final;

	/** Linearizes a scaled value. */
	float get_linear_value(float scaled_value);

private:
	static const std::string glsl_type;

	std::string _unit;
	float _minvalue;
	float _maxvalue;
	float _logarithmic_factor;
	float _default;
};

/** Setting type storing a RGB color triple. */
class ColorType: public DayBaseType
{
public:
	ColorType(YAML::Node& data);

	virtual const std::string& get_glsl_type(void) const { return glsl_type; }

	ValueType get_scaled_value(const ValueType& values) const final;

	std::vector<float> get_linear_value(const std::vector<float>& scaled_value);

private:
	static const std::string glsl_type;

	std::vector<float> _default;
};

/**
 * Constructs a new setting from a given dataset. This method will automatically
 * instantiate a new class matching the type of the given dataset. It will fill
 * all values provided by the dataset and then return the created instance.
 */
std::shared_ptr<DayBaseType> make_daysetting_from_data(YAML::Node& data);

}	// namespace rpcore
