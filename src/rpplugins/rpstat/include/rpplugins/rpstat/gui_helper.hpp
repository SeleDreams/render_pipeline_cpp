/**
 * MIT License
 *
 * Copyright (c) 2018 Younguk Kim (bluekyu)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <render_pipeline/rpcore/pluginbase/setting_types.hpp>

#include <fmt/format.h>

#include <imgui.h>

#include "gui_interface.hpp"

namespace rpplugins {

inline void draw_help_marker(const char* desc)
{
    ImGui::TextDisabled("(?)");
    if (ImGui::IsItemHovered())
    {
        ImGui::BeginTooltip();
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
        ImGui::TextUnformatted(desc);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

inline bool draw_slider(GUIInterface* gui_interface, rpcore::FloatType* base_type, float& value)
{
    const auto mgr = gui_interface->get_plugin_mgr();
    bool is_shown = true;
    for (const auto& key_value : base_type->get_display_conditions())
        is_shown = is_shown && (mgr->get_setting_handle(gui_interface->get_plugin_id(), key_value.first)->get_value_as_string() == key_value.second);
    if (!is_shown)
        return false;

    bool changed = ImGui::SliderFloat(base_type->get_label().c_str(), &value, base_type->get_min(), base_type->get_max());
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::Selectable(fmt::format("Reset###{}", base_type->get_debug_name()).c_str()))
            value = base_type->get_default();
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    draw_help_marker(fmt::format("{}\nDefault: {}", base_type->get_description(), base_type->get_default()).c_str());
    return changed;
}

inline bool draw_slider(GUIInterface* gui_interface, rpcore::IntType* base_type, int& value)
{
    const auto mgr = gui_interface->get_plugin_mgr();
    bool is_shown = true;
    for (const auto& key_value : base_type->get_display_conditions())
        is_shown = is_shown && (mgr->get_setting_handle(gui_interface->get_plugin_id(), key_value.first)->get_value_as_string() == key_value.second);
    if (!is_shown)
        return false;

    bool changed = ImGui::SliderInt(base_type->get_label().c_str(), &value, base_type->get_min(), base_type->get_max());
    if (ImGui::BeginPopupContextItem())
    {
        if (ImGui::Selectable(fmt::format("Reset###{}", base_type->get_debug_name()).c_str()))
            value = base_type->get_default();
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    draw_help_marker(fmt::format("{}\nDefault: {}", base_type->get_description(), base_type->get_default()).c_str());
    return changed;
}

template <class T>
inline void check_setting_changed(std::unordered_set<std::string>& settings, const std::string& id, rpcore::TemplatedType<T>* base_type, T value)
{
    if (value != base_type->get_value())
    {
        base_type->set_value(value);
        settings.insert(id);
    }
}

}
