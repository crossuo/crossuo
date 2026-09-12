# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

# xuo installer
add_subdirectory(xuoi)

# xuo data inspector
add_subdirectory(xuodump)

# in-client assistant (X:UO Assist, ImGui; GL3/GLES2 only)
if (XUO_ASSISTANT_ENABLED)
  add_subdirectory(xuoassist)
endif()

# external gfx test
add_subdirectory(gfxtest)
