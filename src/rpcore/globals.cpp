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

#include "render_pipeline/rpcore/globals.hpp"

#include <clockObject.h>

#include "render_pipeline/rppanda/showbase/showbase.hpp"

namespace rpcore {

rppanda::ShowBase* Globals::base = nullptr;
NodePath Globals::render;
ClockObject* Globals::clock = nullptr;
LVecBase2i Globals::resolution;
LVecBase2i Globals::native_resolution;
TextFont* Globals::font = nullptr;

void Globals::load(rppanda::ShowBase* showbase)
{
    Globals::base = showbase;
    Globals::render = showbase->get_render();
    Globals::clock = ClockObject::get_global_clock();
    Globals::resolution = LVecBase2i(0, 0);
}

void Globals::unload()
{
    Globals::base = nullptr;
    Globals::render.clear();
    Globals::clock = nullptr;
    Globals::font = nullptr;
}

}
