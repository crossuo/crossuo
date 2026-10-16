# SPDX-License-Identifier: AGPL-3.0-or-later
# SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

set(wisp_src
  Wisp/WispConnection.cpp
  Wisp/WispMouse.cpp
  Wisp/WispPacketReader.cpp
  Wisp/WispThread.cpp
  Wisp/WispWindow.cpp
  Wisp/WispBinaryFileWriter.cpp
  Wisp/WispPacketMessage.cpp
)

set(gameobjects_src
  GameObjects/CustomHouseMultiObject.cpp
  GameObjects/GameEffect.cpp
  GameObjects/GameItem.cpp
  GameObjects/GameWorld.cpp
  GameObjects/MapObject.cpp
  GameObjects/RenderStaticObject.cpp
  GameObjects/CustomHouseObjects.cpp
  GameObjects/GameEffectDrag.cpp
  GameObjects/GameObject.cpp
  GameObjects/LandObject.cpp
  GameObjects/MultiObject.cpp
  GameObjects/StaticObject.cpp
  GameObjects/GameCharacter.cpp
  GameObjects/GameEffectMoving.cpp
  GameObjects/GamePlayer.cpp
  GameObjects/MapBlock.cpp
  GameObjects/ObjectOnCursor.cpp
)

set(renderer_src
  Renderer/RenderAdd.cpp
  Renderer/RenderBatch.cpp
  Renderer/RenderCircleOfTransparency.cpp
  Renderer/RenderDraw.cpp
  Renderer/RenderDraw_GL1.cpp
  Renderer/RenderDebug.cpp
  Renderer/RenderFrameBuffer.cpp
  Renderer/RenderShader.cpp
  Renderer/RenderTexture.cpp
  Renderer/Debug/GLTextureTracker.cpp
  Renderer/RenderGlobals.cpp
  Renderer/RenderState.cpp
  Renderer/RenderInternal.cpp
  Renderer/RenderTypes.cpp
)

set(gui_src
  GUI/BaseGUI.cpp
  GUI/GUIAlphaBlending.cpp
  GUI/GUIBuff.cpp
  GUI/GUIBulletinBoardObject.cpp
  GUI/GUIButton.cpp
  GUI/GUIButtonTileart.cpp
  GUI/GUICheckbox.cpp
  GUI/GUIChecktrans.cpp
  GUI/GUIColoredPolygone.cpp
  GUI/GUIComboBox.cpp
  GUI/GUIComboboxText.cpp
  GUI/GUIDataBox.cpp
  GUI/GUIDrawObject.cpp
  GUI/GUIExternalTexture.cpp
  GUI/GUIGenericText.cpp
  GUI/GUIGenericTextEntry.cpp
  GUI/GUIGlobalColor.cpp
  GUI/GUIGroup.cpp
  GUI/GUIGumppic.cpp
  GUI/GUIGumppicHightlighted.cpp
  GUI/GUIGumppicTiled.cpp
  GUI/GUIHitBox.cpp
  GUI/GUIHTMLButton.cpp
  GUI/GUIHTMLGump.cpp
  GUI/GUIHTMLHitBox.cpp
  GUI/GUIHTMLResizepic.cpp
  GUI/GUIHTMLSlider.cpp
  GUI/GUIHTMLText.cpp
  GUI/GUILine.cpp
  GUI/GUIMenuObject.cpp
  GUI/GUIMinMaxButtons.cpp
  GUI/GUIPolygonal.cpp
  GUI/GUIRadio.cpp
  GUI/GUIResizeButton.cpp
  GUI/GUIResizepic.cpp
  GUI/GUIScissor.cpp
  GUI/GUIScrollBackground.cpp
  GUI/GUIShader.cpp
  GUI/GUIShopItem.cpp
  GUI/GUIShopResult.cpp
  GUI/GUISkillGroup.cpp
  GUI/GUISkillItem.cpp
  GUI/GUISlider.cpp
  GUI/GUIText.cpp
  GUI/GUITextEntry.cpp
  GUI/GUITilepic.cpp
  GUI/GUITilepicHightlighted.cpp
  GUI/GUITilepicScaled.cpp
  GUI/GUIVirtueGump.cpp
  GUI/GUIWorldMapTexture.cpp
)

set(gumps_src
  Gumps/GumpAbility.cpp
  Gumps/GumpBaseScroll.cpp
  Gumps/GumpBook.cpp
  Gumps/GumpBuff.cpp
  Gumps/GumpBulletinBoard.cpp
  Gumps/GumpBulletinBoardItem.cpp
  Gumps/GumpChat.cpp
  Gumps/GumpChatChooseName.cpp
  Gumps/GumpCombatBook.cpp
  Gumps/GumpConsoleType.cpp
  Gumps/GumpContainer.cpp
  Gumps/Gump.cpp
  Gumps/GumpCustomHouse.cpp
  Gumps/GumpDrag.cpp
  Gumps/GumpDye.cpp
  Gumps/GumpGeneric.cpp
  Gumps/GumpGrayMenu.cpp
  Gumps/GumpJournal.cpp
  Gumps/GumpMap.cpp
  Gumps/GumpMenubar.cpp
  Gumps/GumpMenu.cpp
  Gumps/GumpMinimap.cpp
  Gumps/GumpNotify.cpp
  Gumps/GumpOptions.cpp
  Gumps/GumpPaperdoll.cpp
  Gumps/GumpPartyManifest.cpp
  Gumps/GumpPartyInvite.cpp
  Gumps/GumpPopupMenu.cpp
  Gumps/GumpProfile.cpp
  Gumps/GumpProperty.cpp
  Gumps/GumpPropertyIcon.cpp
  Gumps/GumpQuestion.cpp
  Gumps/GumpRaceChange.cpp
  Gumps/GumpRacialAbilitiesBook.cpp
  Gumps/GumpRacialAbility.cpp
  Gumps/GumpScreenCharacterList.cpp
  Gumps/GumpScreenConnection.cpp
  Gumps/GumpScreenCreateCharacter.cpp
  Gumps/GumpScreenGame.cpp
  Gumps/GumpScreenMain.cpp
  Gumps/GumpScreenSelectProfession.cpp
  Gumps/GumpScreenSelectTown.cpp
  Gumps/GumpScreenServer.cpp
  Gumps/GumpSecureTrading.cpp
  Gumps/GumpSelectColor.cpp
  Gumps/GumpSelectFont.cpp
  Gumps/GumpShop.cpp
  Gumps/GumpSkill.cpp
  Gumps/GumpSkills.cpp
  Gumps/GumpSpellbook.cpp
  Gumps/GumpSpell.cpp
  Gumps/GumpStatusbar.cpp
  Gumps/GumpTargetSystem.cpp
  Gumps/GumpTextEntryDialog.cpp
  Gumps/GumpTip.cpp
  Gumps/GumpWorldMap.cpp
  Gumps/GumpResourceTracker.cpp
)

set(managers_src
  Managers/AnimationManager.cpp
  Managers/CityManager.cpp
  Managers/ClilocManager.cpp
  Managers/ColorManager.cpp
  Managers/ConfigManager.cpp
  Managers/ConnectionManager.cpp
  Managers/ChatManager.cpp
  Managers/CorpseManager.cpp
  Managers/CreateCharacterManager.cpp
  Managers/CustomHousesManager.cpp
  Managers/EffectManager.cpp
  Managers/FontsManager.cpp
  Managers/GumpManager.cpp
  Managers/IntlocManager.cpp
  Managers/MacroManager.cpp
  Managers/MapManager.cpp
  Managers/MouseManager.cpp
  Managers/ObjectPropertiesManager.cpp
  Managers/OptionsMacroManager.cpp
  Managers/PacketManager.cpp
  Managers/Waypoints.cpp
  Managers/ProfessionManager.cpp
  Managers/ScreenEffectManager.cpp
  Managers/SkillGroupManager.cpp
  Managers/SkillsManager.cpp
  Managers/SoundManager.cpp
  Managers/SpeechManager.cpp
  Managers/UOFileReader.cpp
)

set(network_src
  Network/Connection.cpp
  Network/Packets.cpp
  Network/UOHuffman.cpp
)

set(screenstages_src
  ScreenStages/BaseScreen.cpp
  ScreenStages/CharacterListScreen.cpp
  ScreenStages/ConnectionScreen.cpp
  ScreenStages/CreateCharacterScreen.cpp
  ScreenStages/GameBlockedScreen.cpp
  ScreenStages/GameScreen.cpp
  ScreenStages/MainScreen.cpp
  ScreenStages/SelectProfessionScreen.cpp
  ScreenStages/SelectTownScreen.cpp
  ScreenStages/ServerScreen.cpp
  ScreenStages/VfsStatusHud.cpp
)

set(textengine_src
  TextEngine/EntryText.cpp
  TextEngine/GameConsole.cpp
  TextEngine/Journal.cpp
  TextEngine/RenderTextObject.cpp
  TextEngine/TextContainer.cpp
  TextEngine/TextData.cpp
  TextEngine/TextRenderer.cpp
)

set(walker_src
  Walker/FastWalk.cpp
  Walker/PathFinder.cpp
  Walker/PathNode.cpp
  Walker/WalkData.cpp
  Walker/Walker.cpp
)

set(crypt_src
  Crypt/GameCrypt.cpp
  Crypt/LoginCrypt.cpp
  Crypt/md5.cpp
  Crypt/CryptEntry.cpp
)

set(crossuo_src
  Point.cpp
  Vector.cpp
  Misc.cpp
  LauncherWindow.cpp
  BaseQueue.cpp
  CharacterList.cpp
  CityList.cpp
  ClickObject.cpp
  Container.cpp
  ContainerStack.cpp
  DateTimeStamp.cpp
  Globals.cpp
  ImageBounds.cpp
  Macro.cpp
  Multi.cpp
  MultiMap.cpp
  CrossUO.cpp
  GameWindow.cpp
  Party.cpp
  PressedObject.cpp
  Profession.cpp
  QuestArrow.cpp
  RenderObject.cpp
  RenderWorldObject.cpp
  ScreenshotBuilder.cpp
  SelectedObject.cpp
  ServerList.cpp
  SkillGroup.cpp
  Sprite.cpp
  Spells.cpp
  Target.cpp
  TargetGump.cpp
  ToolTip.cpp
  UseItemsList.cpp
  Weather.cpp
  WeatherEffect.cpp
  Splash.cpp
  Utility/PingThread.cpp
  Utility/PerfMarker.cpp
  Sockets.cpp
  Main.cpp
  Platform.cpp
  Config.cpp
)

set(XUO_DIR ${PROJECT_SOURCE_DIR}/src/)
include_directories(${XUO_DIR})

# in-client assistant (X:UO Assist, ImGui; GL3/GLES2 only; wired into the
# crossuo target further below)
if (XUO_ASSISTANT_ENABLED)
  add_subdirectory(Assistant)
  add_subdirectory(AssetViewer)
endif()

set(XUO_SRCS ${crossuo_src} ${gameobjects_src} ${renderer_src} ${gui_src} ${gumps_src} ${managers_src} ${network_src} ${screenstages_src} ${textengine_src} ${utility_src} ${walker_src} ${crypt_src} ${wisp_src})
file(GLOB XUO_HDRS ${XUO_DIR}/**/*.h)

set(XUO_FILES ${XUO_SRCS} ${XUO_HDRS})

if(${CMAKE_VERSION} VERSION_LESS "3.8.0")
  source_group("${XUO_DIR}" FILES ${XUO_FILES})
else()
  source_group(TREE "${XUO_DIR}" FILES ${XUO_FILES})
endif()

if(XUO_LAUNCHER_EMBEDDED)
  # X:UO Launcher app embedded into the client (GL3 builds only)
  set(XUOL_EMBEDDED_SRCS
    ${XUOI_DIR}/xuol_app.cpp
    ${XUOI_DIR}/xuol_app.h
    ${XUOI_DIR}/accounts.cpp
    ${XUOI_DIR}/accounts.h
    ${XUOI_DIR}/ui_model.cpp
    ${XUOI_DIR}/ui_model.h
    ${XUOI_DIR}/ui_shards.cpp
    ${XUOI_DIR}/ui_shards.h
    ${XUOI_DIR}/widgets.cpp
    ${XUOI_DIR}/widgets.h
  )
  source_group("xuoi" FILES ${XUOL_EMBEDDED_SRCS})
  set(XUO_FILES ${XUO_FILES} ${XUOL_EMBEDDED_SRCS})
endif()

if(XUO_WINDOWS)
  set(XUO_FILES ${XUO_FILES} ${CMAKE_SOURCE_DIR}/resources/client.rc)
endif()

# The client builds ONE self-contained executable with a real main().
add_executable(crossuo ${XUO_FILES})

# musl-style fully static client binary; applied to the executable target only
# so dependency configure-time checks keep linking dynamically. ld's -static
# mode rejects even -Bdynamic-wrapped shared objects, so static resolution is
# emulated: -Wl,-Bstatic up front, a -Wl,-Bdynamic zone around the GL stack
# (opened in the root CMakeLists.txt, Mesa has no static archive) and back to
# static after the last link item so the implicit -lc/-lgcc stay static.
if (XUO_STATIC_BINARY AND NOT MSVC)
  target_link_options(crossuo PRIVATE -static-libgcc -no-pie "-Wl,-Bstatic")
endif()

# portable glibc builds (e.g. manylinux2014): bake in the C++ runtime so the
# binary never asks for a newer libstdc++/libgcc than the target has
if (XUO_STATIC_CXX_RUNTIME AND NOT MSVC)
  target_link_options(crossuo PRIVATE -static-libgcc -static-libstdc++)
endif()

add_dependencies(crossuo ${EXTERNAL_PROJECT} ${EXTERNALGFX_PROJECT} ${XUOCORE_PROJECT} ${COMMON_PROJECT} ${3RDPARTY_STATIC})
target_link_libraries(crossuo PUBLIC ${EXTERNAL_PROJECT} ${EXTERNALGFX_PROJECT} ${XUOCORE_PROJECT} ${COMMON_PROJECT} ${SDL2_LIBRARIES} ${GL_LIBRARIES} ${GLM_LIBRARIES} ${THREAD} ${CURL_LIBRARIES})
target_include_directories(crossuo PUBLIC ${COMMON_INCLUDE} ${EXTERNALGFX_INCLUDE} ${GLM_INCLUDE_DIR} ${XUOI_INCLUDE})

# the launcher app is compiled into the client only; keeping the define on the
# target scope avoids flagged rebuilds of the whole dependency tree
if(XUO_LAUNCHER_EMBEDDED)
  target_compile_definitions(crossuo PRIVATE XUO_LAUNCHER_EMBEDDED=1)
endif()

# the in-client assistant (X:UO Assist) is compiled into the client only; the
# target-scoped define keeps the dependency tree clean (mirrors the launcher)
if(XUO_ASSISTANT_ENABLED)
  target_compile_definitions(crossuo PRIVATE XUO_ASSISTANT=1)
  add_dependencies(crossuo ${XUOASSIST_PROJECT})
  target_link_libraries(crossuo PUBLIC ${XUOASSIST_PROJECT})
  # the icon-atlas assets + the About logo are embedded in the binary via
  # create_resource (see src/Assistant/CMakeLists.txt); nothing is copied
  # next to the executable - the client ships as a single file
  target_include_directories(crossuo PUBLIC ${XUOASSIST_INCLUDE})
  # the in-client asset viewer shares the assistant's ImGui frame pipeline and
  # is linked/built under the same conditions (its window is submitted from
  # AssistantWindow_DrawOverlay)
  add_dependencies(crossuo ${XUOASSETVIEWER_PROJECT})
  target_link_libraries(crossuo PUBLIC ${XUOASSETVIEWER_PROJECT})
  target_include_directories(crossuo PUBLIC ${XUOASSETVIEWER_INCLUDE})
  # Link guard: the assistant is part of this shared lib,
  # and a scriptgump declaration that never got a definition used to slip
  # through as an undefined symbol that aborted the client on the first
  # assistant frame. --no-undefined keeps that class of bug out of the .so
  # (verified: the whole lib still links - its imports are all resolved
  # through the link line, the in-process "plugin" hooks do not rely on
  # undefined-symbol leniency).
  # (apple ld64 spells it -undefined,error; wasm-ld has no equivalent)
  if (APPLE)
    target_link_options(crossuo PRIVATE "LINKER:-undefined,error")
  elseif (NOT XUO_EMSC)
    target_link_options(crossuo PRIVATE "LINKER:--no-undefined")
  endif()
endif()

if(CLANG_FORMAT AND NOT $ENV{XUO_CI})
  add_dependencies(crossuo clang-format-7)
endif()

xuo_disable_console(crossuo)

if (XUO_STATIC_BINARY AND NOT MSVC)
  # flip the link line back to static resolution so the implicit -lc/-lgcc
  # still link statically (GL was switched to dynamic mid-line, see the root
  # CMakeLists.txt)
  target_link_libraries(crossuo PRIVATE "-Wl,-Bstatic")
endif()
