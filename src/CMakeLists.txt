if(CMAKE_SYSTEM_NAME MATCHES "Windows")
  add_definitions(-DNOMINMAX)
  add_definitions(-DUNICODE)
  add_definitions(-D_UNICODE)
  add_definitions(-DWIN32_LEAN_AND_MEAN)
  add_definitions(-D_WIN32_WINNT=0x0602)
  add_definitions(-D_SCL_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE)
else()
  #thread sanitizer - issues to fix
  #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
  #set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=address")
  #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=memory -fsanitize-memory-track-origins -fsanitize-memory-use-after-dtor -fsanitize-blacklist=blacklist.txt")
  #set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=memory -fsanitize-memory-track-origins -fsanitize-memory-use-after-dtor -fsanitize-blacklist=blacklist.txt")
  #set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=undefined")
  #set(CMAKE_LINKER_FLAGS_DEBUG "${CMAKE_LINKER_FLAGS_DEBUG} -fno-omit-frame-pointer -fsanitize=undefined")
endif()

add_definitions(-DUSE_WISP=0)
add_definitions(-DUSE_BASS=0)
add_definitions(-DUSE_TIMERTHREAD=0)

if(CMAKE_C_COMPILER_ID MATCHES "MSVC")
  # enable the latest C++ standard feature set,
  # and also disable MSVC specific extensions
  # to be even more standards compliant.
  check_and_add_flag(CPPLATEST /std:c++latest)
else()
  # Enable C++17, but fall back to C++14 if it isn't available.
  # CMAKE_CXX_STANDARD cannot be used here because we require C++14 or newer, not any standard.
  check_and_add_flag(CXX17 -std=c++17)
  if(NOT FLAG_CXX_CXX17)
    set(CMAKE_CXX_STANDARD 14)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
  endif()
  check_and_add_flag(THREADS -pthread)
  set(THREAD "-pthread")
endif()

# These aren't actually needed for C11/C++11
# but some dependencies require them (LLVM, libav).
add_definitions(-D__STDC_LIMIT_MACROS)
add_definitions(-D__STDC_CONSTANT_MACROS)

set(wisp_src
  Wisp/WispApplication.cpp
  Wisp/WispConnection.cpp
  Wisp/WispMouse.cpp
  Wisp/WispPacketReader.cpp
  Wisp/WispThread.cpp
  Wisp/WispWindow.cpp
  Wisp/WispBinaryFileWriter.cpp
  Wisp/WispDataStream.cpp
  Wisp/WispMappedFile.cpp
  Wisp/WispPacketMessage.cpp
  Wisp/WispTextFileParser.cpp
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

# WGLEW_ARB_render_texture
set(glengine_src
  GLEngine/GLEngine.cpp
  GLEngine/GLFrameBuffer.cpp
  GLEngine/GLHTMLTextTexture.cpp
  GLEngine/GLShader.cpp
  GLEngine/GLTextTexture.cpp
  GLEngine/GLTextureCircleOfTransparency.cpp
  GLEngine/GLTexture.cpp
  GLEngine/GLVector.cpp
)

set(gui_src
  GUI/BaseGUI.cpp
  GUI/GUIAlphaBlending.cpp
  GUI/GUIBlending.cpp
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
  GUI/GUIPage.cpp
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
  Gumps/GumpPopupMenu.cpp
  Gumps/GumpProfile.cpp
  Gumps/GumpProperty.cpp
  Gumps/GumpPropertyIcon.cpp
  Gumps/GumpQuestion.cpp
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
)

set(managers_src
  Managers/AnimationManager.cpp
  Managers/CityManager.cpp
  Managers/ClilocManager.cpp
  Managers/ColorManager.cpp
  Managers/ConfigManager.cpp
  Managers/ConnectionManager.cpp
  Managers/CorpseManager.cpp
  Managers/CreateCharacterManager.cpp
  Managers/CustomHousesManager.cpp
  Managers/EffectManager.cpp
  Managers/FileManager.cpp
  Managers/FontsManager.cpp
  Managers/GumpManager.cpp
  Managers/IntlocManager.cpp
  Managers/MacroManager.cpp
  Managers/MapManager.cpp
  Managers/MouseManager.cpp
  Managers/ObjectPropertiesManager.cpp
  Managers/OptionsMacroManager.cpp
  Managers/PacketManager.cpp
  Managers/PluginManager.cpp
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
  Network/PluginPackets.cpp
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

set(utility_src
  Utility/AutoResetEvent.cpp
  Utility/PingThread.cpp
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
  Logging.cpp
  Point.cpp
  Misc.cpp

  BaseQueue.cpp
  CharacterList.cpp
  CityList.cpp
  ClickObject.cpp
  CommonInterfaces.cpp
  Container.cpp
  ContainerStack.cpp
  DateTimeStamp.cpp
  Globals.cpp
  ImageBounds.cpp
  IndexObject.cpp
  Macro.cpp
  Multi.cpp
  MultiMap.cpp
  Application.cpp
  CrossUO.cpp
  GameWindow.cpp
  Party.cpp
  PartyObject.cpp
  PressedObject.cpp
  Profession.cpp
  QuestArrow.cpp
  RenderObject.cpp
  RenderWorldObject.cpp
  ScreenshotBuilder.cpp
  SelectedObject.cpp
  ServerList.cpp
  SkillGroup.cpp
  CrossPCH.cpp
  Target.cpp
  TargetGump.cpp
  TextureObject.cpp
  ToolTip.cpp
  UseItemsList.cpp
  Weather.cpp
  WeatherEffect.cpp

  FileSystem.cpp
  Sockets.cpp
  Main.cpp
  Platform.cpp
  Config.cpp
)

if(XUO_WINDOWS)
  set(crossuo_src ${crossuo_src}
    VMQuery.cpp                 # FIXME: windows only, rap, coredump
    StackWalker.cpp             # FIXME: windows only, wrap, use backtrace
    ExceptionFilter.cpp         # FIXME: exception handler, wrap, use signals
    CrossUO.rc
  )
endif()

include_directories("${PROJECT_SOURCE_DIR}/src/GameObjects")
include_directories("${PROJECT_SOURCE_DIR}/src/GLEngine")
include_directories("${PROJECT_SOURCE_DIR}/src/GUI")
include_directories("${PROJECT_SOURCE_DIR}/src/Gumps")
include_directories("${PROJECT_SOURCE_DIR}/src/Managers")
include_directories("${PROJECT_SOURCE_DIR}/src/Network")
include_directories("${PROJECT_SOURCE_DIR}/src/ScreenStages")
include_directories("${PROJECT_SOURCE_DIR}/src/TextEngine")
include_directories("${PROJECT_SOURCE_DIR}/src/Utility")
include_directories("${PROJECT_SOURCE_DIR}/src/Walker")
include_directories("${PROJECT_SOURCE_DIR}/src/Wisp")
include_directories("${PROJECT_SOURCE_DIR}/src/")

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  #target_link_libraries(wisp PUBLIC dl rt)
  add_definitions(-DNULL=nullptr)
endif()

macro(xuo_library TARGET SRCS)
  string(REPLACE ".cpp" ".h" headers "${SRCS}")
  string(REPLACE "Crypt/twofish.h" "" headers "${headers}")
  string(REPLACE "CommonInterfaces.h" "api/commoninterfaces.h" headers "${headers}")
  add_library(${TARGET} ${SRCS} ${headers})
  set_target_properties(${TARGET} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "CrossPCH.h")
  cotire(${TARGET})
endmacro()

if(XUO_BUILD_MODULES)
  xuo_library(wisp "${wisp_src}")
  xuo_library(gameobjects "${gameobjects_src}")
  xuo_library(glengine "${glengine_src}")
  xuo_library(gui "${gui_src}")
  xuo_library(gumps "${gumps_src}")
  xuo_library(managers "${managers_src}")
  xuo_library(network "${network_src}")
  xuo_library(screenstages "${screenstages_src}")
  xuo_library(textengine "${textengine_src}")
  xuo_library(utility "${utility_src}")
  xuo_library(walker "${walker_src}")
  xuo_library(crypt "${crypt_src}")

  string(REPLACE ".cpp" ".h" crossuo_hdr "${crossuo_src}")
  string(REPLACE "Crypt/twofish.h" "" headers "${headers}")
  string(REPLACE "CommonInterfaces.h" "api/commoninterfaces.h" headers "${headers}")

  add_executable(CrossUO ${crossuo_src} ${crossuo_hdr})
  target_link_libraries(screenstages PRIVATE gumps)
  target_link_libraries(gumps PRIVATE managers)
  target_link_libraries(CrossUO PUBLIC managers network gumps screenstages gameobjects glengine gui textengine utility walker crypt wisp)
else()
  set(SRCS ${crossuo_src} ${gameobjects_src} ${glengine_src} ${gui_src} ${gumps_src} ${managers_src} ${network_src} ${screenstages_src} ${textengine_src} ${utility_src} ${walker_src} ${crypt_src} ${wisp_src})
  string(REPLACE ".cpp" ".h" headers "${SRCS}")
  string(REPLACE "Crypt/twofish.h" "" headers "${headers}")
  string(REPLACE "CommonInterfaces.h" "api/commoninterfaces.h" headers "${headers}")

  if(${CMAKE_VERSION} VERSION_LESS "3.8.0")
    source_group("${PROJECT_SOURCE_DIR}/src/" FILES ${SRCS} ${headers})
  else()
    source_group(TREE "${PROJECT_SOURCE_DIR}/src/" FILES ${SRCS} ${headers})
  endif()

  if(NOT XUO_WINDOWS)
    add_library(crossuo SHARED ${SRCS} ${headers})
    target_link_libraries(crossuo PUBLIC ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} ${THREAD})
    set_target_properties(crossuo PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "CrossPCH.h")
    cotire(crossuo)
    if(XUO_OSX)
      add_custom_command(TARGET crossuo POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/libcrossuo.dylib" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/crossuo.so")
      add_custom_command(TARGET crossuo_unity POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/unity/libcrossuo.dylib" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/crossuo.so")
    else()
      add_custom_command(TARGET crossuo POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/libcrossuo.so" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/crossuo.so")
      add_custom_command(TARGET crossuo_unity POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/unity/libcrossuo.so" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/crossuo.so")
    endif()
    add_executable(client CrossApp.cpp)
    set_target_properties(client PROPERTIES OUTPUT_NAME crossuo)
    target_link_libraries(client PUBLIC -ldl ${THREAD})
    add_dependencies(crossuo client)
    add_dependencies(crossuo_unity client)
  else()
    add_executable(crossuo ${SRCS} ${headers})
  endif()
endif()

target_link_libraries(crossuo PUBLIC ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} ${THREAD})

if(XUO_WINDOWS)
  set_target_properties(crossuo PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "CrossPCH.h")
  cotire(crossuo)
  set_target_properties(crossuo PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
  set_target_properties(crossuo_unity PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
  add_custom_command(TARGET crossuo POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/Dependencies/${XUO_ARCH}/bin" $<TARGET_FILE_DIR:crossuo>)
  add_custom_command(TARGET crossuo_unity POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/Dependencies/${XUO_ARCH}/bin" $<TARGET_FILE_DIR:crossuo_unity>)
endif()
