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

#include <QtGlobal>

namespace muse::rcontrol::mcp {
static constexpr quint16 DEFAULT_LISTEN_PORT = 2212;

//! NOTE MUSE_RCONTROL_PORT overrides the port, e.g. to run several instances
inline quint16 listenPort()
{
    bool ok = false;
    const int envPort = qEnvironmentVariableIntValue("MUSE_RCONTROL_PORT", &ok);
    return ok && envPort > 0 && envPort <= 65535 ? static_cast<quint16>(envPort) : DEFAULT_LISTEN_PORT;
}
}
