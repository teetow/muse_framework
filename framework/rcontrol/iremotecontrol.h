/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore/Audacity CLA applies
 *
 * Copyright (C) 2026 MuseScore/Audacity and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once

#include "modularity/imoduleinterface.h"

namespace muse::rcontrol {
//! Starts and stops the remote-control server. An app that creates the module
//! without auto-start uses this to open the channel once it is ready.
class IRemoteControl : MODULE_CONTEXT_INTERFACE
{
    INTERFACE_ID(IRemoteControl)

public:
    virtual ~IRemoteControl() = default;

    virtual bool start() = 0;
    virtual void stop() = 0;
};
}
