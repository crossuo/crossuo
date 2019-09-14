set(wisp_src
  Wisp/WispApplication.cpp
  Wisp/WispConnection.cpp
  Wisp/WispMouse.cpp
  Wisp/WispPacketReader.cpp
  Wisp/WispThread.cpp
  Wisp/WispWindow.cpp
  Wisp/WispBinaryFileWriter.cpp
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
  GLEngine/GLShader.cpp
  GLEngine/GLTextureCircleOfTransparency.cpp
  GLEngine/GLTexture.cpp
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
  Vector.cpp
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
  Sprite.cpp
  CrossPCH.cpp
  Target.cpp
  TargetGump.cpp
  ToolTip.cpp
  UseItemsList.cpp
  Weather.cpp
  WeatherEffect.cpp
  Utility/PingThread.cpp

  Sockets.cpp
  Main.cpp
  Platform.cpp
  Config.cpp
)

if(XUO_WINDOWS)
  set(crossuo_src ${crossuo_src}
    VMQuery.cpp                 # FIXME: windows only, wrap, coredump
    StackWalker.cpp             # FIXME: windows only, wrap, use backtrace
    ExceptionFilter.cpp         # FIXME: exception handler, wrap, use signals
    CrossUO.rc
  )
endif()

set(XUO_DIR ${PROJECT_SOURCE_DIR}/src/)
include_directories(${XUO_DIR})

set(XUO_SRCS ${crossuo_src} ${gameobjects_src} ${glengine_src} ${gui_src} ${gumps_src} ${managers_src} ${network_src} ${screenstages_src} ${textengine_src} ${utility_src} ${walker_src} ${crypt_src} ${wisp_src})
file(GLOB XUO_HDRS ${XUO_DIR}/*.h)

set(XUO_FILES ${XUO_SRCS} ${XUO_HDRS})

if(${CMAKE_VERSION} VERSION_LESS "3.8.0")
  source_group("${XUO_DIR}" FILES ${XUO_FILES})
else()
  source_group(TREE "${XUO_DIR}" FILES ${XUO_FILES})
endif()

if(XUO_WINDOWS OR XUO_EMSC)
  add_executable(crossuo ${XUO_FILES})
else()
  # crossuo loader
  add_executable(client CrossApp.cpp)
  add_dependencies(client ${EXTERNAL_PROJECT} ${XUOCORE_PROJECT})
  set_target_properties(client PROPERTIES OUTPUT_NAME crossuo)
  target_link_libraries(client PUBLIC ${LOADER} ${THREAD} ${EXTERNAL_PROJECT} ${XUOCORE_PROJECT})
  add_library(crossuo SHARED ${XUO_FILES})
endif()

add_dependencies(crossuo ${EXTERNAL_PROJECT} ${XUOCORE_PROJECT})
target_link_libraries(crossuo PUBLIC ${EXTERNAL_PROJECT} ${XUOCORE_PROJECT} ${SDL2_LIBRARIES} ${OPENGL_LIBRARIES} ${GLEW_LIBRARIES} ${THREAD})
target_include_directories(crossuo PUBLIC ${COMMON_INCLUDE})

if(CLANG_FORMAT AND NOT $ENV{XUO_TRAVIS})
  add_dependencies(crossuo clang-format)
endif()

if(XUO_WINDOWS)
  set_target_properties(crossuo PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
  add_custom_command(TARGET crossuo POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/Dependencies/${XUO_ARCH}/bin" $<TARGET_FILE_DIR:crossuo>)
elseif(XUO_EMSC)
else()
  add_custom_command(TARGET crossuo POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/libcrossuo${CMAKE_SHARED_LIBRARY_SUFFIX}" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/crossuo.so")
  add_dependencies(crossuo client)
endif()

if (ENABLE_COTIRE)
  set_property(SOURCE Misc.cpp PROPERTY COTIRE_EXCLUDED 1)
  set_target_properties(crossuo PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "CrossPCH.h")
  cotire(crossuo)

  if(XUO_WINDOWS)
    set_target_properties(crossuo_unity PROPERTIES LINK_FLAGS "/SUBSYSTEM:WINDOWS")
    add_custom_command(TARGET crossuo_unity POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/Dependencies/${XUO_ARCH}/bin" $<TARGET_FILE_DIR:crossuo>)
  elseif(XUO_EMSC)
  else()
    add_custom_command(TARGET crossuo_unity POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/unity/libcrossuo${CMAKE_SHARED_LIBRARY_SUFFIX}" "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_CFG_INTDIR}/crossuo.so")
  endif()
endif()

