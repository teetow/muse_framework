/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include <QDir>
#include <QString>

namespace muse {
//! MUSE_PROFILE_DIR keeps every per-user file of a run in one folder, for
//! example a test or capture run that must not share state with the user:
//! <dir>/roaming (settings), <dir>/local (app data), <dir>/documents (user data).
//! Returns an empty string when the variable isn't set.
inline QString profileSubdir(const char* sub)
{
    const QString dir = qEnvironmentVariable("MUSE_PROFILE_DIR");
    return dir.isEmpty() ? QString() : QDir::cleanPath(dir + "/" + sub);
}
}
