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

#include "modularity/imodulesetup.h"
#include <memory>

namespace muse::rcontrol::mcp {
class McpController;
}

namespace muse::rcontrol {
class RControlModule : public modularity::IModuleSetup
{
public:
    //! autoStart: start the server on init; otherwise the app starts it through IRemoteControl when it is ready
    explicit RControlModule(bool autoStart = true)
        : m_autoStart(autoStart) {}

    std::string moduleName() const override;

    modularity::IContextSetup* newContext(const muse::modularity::ContextPtr& ctx) const override;

private:
    bool m_autoStart = true;
};

class RControlContext : public modularity::IContextSetup
{
public:
    RControlContext(const muse::modularity::ContextPtr& ctx, bool autoStart)
        : modularity::IContextSetup(ctx), m_autoStart(autoStart) {}

    void registerExports() override;
    void onInit(const IApplication::RunMode& mode) override;
    void onDeinit() override;

private:
    bool m_autoStart = true;
    std::shared_ptr<mcp::McpController> m_mcpController;
};
}
