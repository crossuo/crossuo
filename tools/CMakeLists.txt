# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

# xuo installer
add_subdirectory(xuoi)

# xuo data inspector
add_subdirectory(xuodump)

# external gfx test
add_subdirectory(gfxtest)

# standalone asset viewer (desktop GL builds with the assistant: it links the
# in-client viewer core, see src/AssetViewer)
if (XUO_ASSISTANT_ENABLED AND NOT XUO_EMSC AND (USE_GL3 OR USE_GLES2))
  add_subdirectory(assetviewer)
endif()
