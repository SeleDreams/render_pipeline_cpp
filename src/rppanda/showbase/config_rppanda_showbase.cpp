/**
 * Render Pipeline C++
 *
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

#include "rppanda/showbase/config_rppanda_showbase.hpp"

#include <dconfig.h>

#include "render_pipeline/rppanda/showbase/direct_object.hpp"
#include "render_pipeline/rppanda/showbase/loader.hpp"
#include "render_pipeline/rppanda/showbase/showbase.hpp"
#include "render_pipeline/rppanda/showbase/sfx_player.hpp"
#include "render_pipeline/rppanda/showbase/messenger.hpp"

Configure(config_rppanda_showbase);
NotifyCategoryDef(rppanda_showbase, "");

ConfigureFn(config_rppanda_showbase)
{
    static bool initialized = false;
    if (initialized)
        return;
    initialized = true;

    rppanda::Messenger::init_type();
    rppanda::ShowBase::init_type();
    rppanda::SfxPlayer::init_type();
}
