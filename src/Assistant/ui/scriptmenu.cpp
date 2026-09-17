// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

// See scriptmenu.h. The static rows cover every menu entry; the
// dynamic rows (the Spells submenu from the spell manager's spellbook list
// and the per-macro Client-macro rows) are rendered from the live managers
// in the same path scheme.

#include "scriptmenu.h"

#include <external/gfx/imgui/imgui.h>

#include <cstring>
#include <string>
#include <vector>

#include "../managers/spellmanager.h"

// The command context menu rows;
static const xuoassist::XA_MENU_ITEM g_MenuItems[] = {
    { "Objects", "Info", "Client.Info(['serial']);" },
    { "Objects", "GetSerial", "Client.GetSerial(['serial']);" },
    { "Objects", "GetGraphic", "Client.GetGraphic(['graphic']);" },
    { "Objects", "GetContainer", "Client.GetContainer(['serial']);" },
    { "Objects", "Click", "Client.Click(['serial']);" },
    { "Objects", "Use object", "Client.UseObject(['serial']);" },
    { "Objects", "Attack", "Client.Attack(['serial']);" },
    { "Objects", "Request status", "Client.GetStatus(['serial']);" },
    { "Objects", "Hide", "Client.Hide(['serial']);" },
    { "Objects", "Set dress bag", "Client.SetDressBag(['serial']);" },
    { "Objects", "Unset dress bag", "Client.UnsetDressBag();" },
    { "Objects", "Drop", "Client.Drop(['serial'], [count], [x, y], [z]);" },
    { "Objects", "DropHere", "Client.DropHere(['serial'], [count]);" },
    { "Objects", "MoveItem", "Client.MoveItem(['serial'], [count], ['container'], [x, y], [z]);" },
    { "Objects", "GetDistance", "Client.GetDistance('serial')" },
    { "Objects", "GetDistance", "Client.GetDistance(x, y)" },
    { "Objects", "Get friends status", "Client.GetFriendsStatus();" },
    { "Objects", "Get enemies status", "Client.GetEnemiesStatus();" },
    { "Objects", "Get last target (remove) position", "Client.GetLastTargetPosition();" },
    { "Objects", "Get last attack (remove) position", "Client.GetLastAttackPosition();" },
    { "Objects", "Open container", "Client.OpenContainer('serial', ['delay'=600], ['errorTextPattern']);" },
    { "Objects", "Exists", "Client.ObjectExists('serial');" },
    { "Objects", "Request name", "Client.RequestName('serial', ['delay'=200]);" },
    { "Objects", "Display/Move statusbar gump", "Client.ShowStatusbar('serial', x, y, [minimized]);" },
    { "Objects", "Close statusbar gump", "Client.CloseStatusbar('serial');" },
    { "Objects", "Open paperdoll", "Client.OpenPaperdoll('serial');" },
    { "Objects", "Close paperdoll", "Client.ClosePaperdoll('serial');" },
    { "Objects", "Move paperdoll", "Client.MovePaperdoll('serial', x, y);" },
    { "Find or Use", "UseType", "Client.UseType('graphic', ['color'], ['container'], [recurseSearch]);" },
    { "Find or Use", "UseFromGround", "Client.UseFromGround('graphic', ['color'], ['distance'], ['flags']);" },
    { "Find or Use", "UseTypeList", "Client.UseTypeList('findListName', ['container'], [recurseSearch]);" },
    { "Find or Use", "UseFromGroundList", "Client.UseFromGroundList('findListName', ['distance'], ['flags']);" },
    { "Find or Use", "Count", "Client.Count('graphic', ['color'], ['container'], ['distance'], [recurseSearch]);" },
    { "Find or Use", "FindType", "Client.FindType('graphic', ['color'], ['container'], ['flags'], ['distance'], ['notoriety'], [recurseSearch]);" },
    { "Find or Use", "FindList", "Client.FindList('findListName', ['container'], ['flags'], ['distance'], ['notoriety'], [recurseSearch]);" },
    { "Find or Use", "Use ignore list", "Client.UseIgnoreList('ignoreListName');" },
    { "Find or Use", "Reset ignore list", "Client.ResetIgnoreList();" },
    { "Find or Use", "Find object", "Client.FindObject('serial');" },
    { "Find or Use", "Ignore object", "Client.Ignore('serial', [true/false]);" },
    { "Find or Use", "Reset ignored objects", "Client.IgnoreReset();" },
    { "Find or Use", "Find object at layer", "Client.ObjAtLayer('layerName', ['serial']);" },
    { "Find or Use", "Find friend", "Client.FindFriend(['flags'], ['distance']);" },
    { "Find or Use", "Find enemy", "Client.FindEnemy(['flags'], ['distance']);" },
    { "Targets", "HaveTarget", "Client.HaveTarget();" },
    { "Targets", "WaitTargetType", "Client.WaitTargetType('graphic', ['color'], ['container'], ['flags'], [recurseSearch]);" },
    { "Targets", "WaitTargetGround", "Client.WaitTargetGround('graphic', ['color'], ['distance'], ['flags']);" },
    { "Targets", "WaitTargetTypeList", "Client.WaitTargetTypeList('findListName', ['container'], ['flags'], [recurseSearch]);" },
    { "Targets", "WaitTargetGroundList", "Client.WaitTargetGroundList('findListName', ['distance'], ['flags']);" },
    { "Targets", "WaitTarget on object", "Client.WaitTargetObject('serial');" },
    { "Targets", "WaitTarget on lasttarget", "Client.WaitTargetObject(lasttarget);" },
    { "Targets", "WaitTarget on self", "Client.WaitTargetObject(self);" },
    { "Targets", "WaitTarget on tile", "Client.WaitTargetTile('graphic', x, y, z);" },
    { "Targets", "WaitTarget on lasttile", "Client.WaitTargetTile(lasttile);" },
    { "Targets", "WaitTarget on tile (relative)", "Client.WaitTargetTileRelative('graphic', x, y, z);" },
    { "Targets", "Cancel wait target", "Client.CancelWaitTarget();" },
    { "Targets", "TargetType", "Client.TargetType('graphic', ['color'], ['container'], ['flags'], [recurseSearch]);" },
    { "Targets", "TargetGround", "Client.TargetGround('graphic', ['color'], ['distance'], ['flags']);" },
    { "Targets", "TargetTypeList", "Client.TargetTypeList('findListName', ['container'], ['flags'], [recurseSearch]);" },
    { "Targets", "TargetGroundList", "Client.TargetGroundList('findListName', ['distance'], ['flags']);" },
    { "Targets", "Target on object", "Client.TargetObject('serial');" },
    { "Targets", "Target on lasttarget", "Client.TargetObject(lasttarget);" },
    { "Targets", "Target on self", "Client.TargetObject(self);" },
    { "Targets", "Target on tile", "Client.TargetTile('graphic', x, y, z);" },
    { "Targets", "Target on lasttile", "Client.TargetTile(lasttile);" },
    { "Targets", "Target on tile (relative)", "Client.TargetTileRelative('graphic', x, y, z);" },
    { "Targets", "Cancel current client target", "Client.CancelTarget();" },
    { "Targets", "Validate target on tile", "Client.ValidateTargetTile('graphic', x, y);" },
    { "Targets", "Validate target on tile (relative)", "Client.ValidateTargetTileRelative('graphic', x, y);" },
    { "Targets", "Waiting for target", "Client.WaitForTarget([maxDelay=1000]);" },
    { "Targets", "Get target type", "Client.GetTargetType();" },
    { "Actions", "Resend", "Client.Resend();" },
    { "Actions", "WarMode", "Client.WarMode([true/false]);" },
    { "Actions", "RenameMount", "Client.RenameMount('serial', 'new name');" },
    { "Actions", "EmoteAction", "Client.EmoteAction('action name');" },
    { "Actions", "Morph", "Client.Morph(['graphic']);" },
    { "Actions", "Bandage Self", "Client.BandageSelf();" },
    { "New target system actions", "Bandage self", "Client.BandageTarget(self);" },
    { "New target system actions", "Bandage target", "Client.BandageTarget('serial');" },
    { "New target system actions", "Cast spell on target", "Client.CastTarget('nameOrIndex', 'serial');" },
    { "New target system actions", "Use skill on target", "Client.UseSkillTarget('nameOrIndex', 'serial');" },
    { "Speech and print", "Print", "Client.Print('some text');" },
    { "Speech and print", "Colored Print", "Client.Print('color', 'some text');" },
    { "Speech and print", "CharPrint", "Client.CharPrint('serial', 'color', 'some text');" },
    { "Speech and print", "Say", "Client.Say('some text');" },
    { "Speech and print", "Yell", "Client.SayYell('some text');" },
    { "Speech and print", "Whisper", "Client.SayWhisper('some text');" },
    { "Speech and print", "Emote", "Client.SayEmote('some text');" },
    { "Speech and print", "Broadcast", "Client.SayBroadcast('some text');" },
    { "Speech and print", "Party", "Client.SayParty('some text');" },
    { "Speech and print", "Guild", "Client.SayGuild('some text');" },
    { "Speech and print", "Alliance", "Client.SayAlliance('some text');" },
    { "MapUO", "Open Map", "Client.OpenMap([x, y]);" },
    { "MapUO", "Move Map", "Client.MoveMap(x, y);" },
    { "MapUO", "Close Map", "Client.CloseMap());" },
    { "MapUO", "Open Enhanced Map", "Client.OpenEnhancedMap(['filePath']);" },
    { "Special abilities", "Primary", "Client.UseAbility('Primary');" },
    { "Special abilities", "Secondary", "Client.UseAbility('Secondary');" },
    { "Special abilities", "Wrestling Disarm", "Client.UseWrestlingDisarm();" },
    { "Special abilities", "Wrestling Stun", "Client.UseWrestlingStun();" },
    { "Globals", "Self", "self" },
    { "Globals", "Last target", "lasttarget" },
    { "Globals", "Last attack", "lastattack" },
    { "Globals", "Last status", "laststatus" },
    { "Globals", "Last corpse", "lastcorpse" },
    { "Globals", "Last object", "lastobject" },
    { "Globals", "Last container", "lastcontainer" },
    { "Globals", "Backpack", "backpack" },
    { "Globals", "Ground", "ground" },
    { "Globals", "Ground", "ScriptName" },
    { "Client global variables/Get", "Last target", "Client.ClientLastTarget();" },
    { "Client global variables/Get", "Last attack", "Client.ClientLastAttack();" },
    { "Client global variables/Get", "New target system (serial)", "Client.TargetSystemSerial();" },
    { "Client global variables/Set", "Last target", "Client.ClientLastTarget('serial');" },
    { "Client global variables/Set", "Last attack", "Client.ClientLastAttack('serial');" },
    { "Client global variables/Set", "New target system (serial)", "Client.TargetSystemSerial('serial');" },
    { "Scripts control", "Execute script", "Client.Exec('scriptName', [oneScriptCanBeRunned], [arguments]);" },
    { "Scripts control", "Terminate script", "Client.Terminate('scriptName', ['saveScriptName']);" },
    { "Scripts control", "Load script", "Client.LoadScript('filePath');" },
    { "Scripts control", "Script running", "Client.ScriptRunning('scriptName');" },
    { "Scripts control", "Activate client window", "Client.ActivateClient();" },
    { "Scripts control", "Turn off window", "Client.ShutdownWindows(['mode']);" },
    { "Scripts control", "Get current hotkeys state (on/off)", "Client.OnOffHotkeys();" },
    { "Scripts control", "Set current hotkeys state (on/off)", "Client.OnOffHotkeys(state);" },
    { "Script", "Wait (ms)", "Client.Wait(100);" },
    { "Script", "Return", "return;" },
    { "Script", "Break", "break;" },
    { "Script", "Continue", "continue;" },
    { "Script", "If", "if (condition)" },
    { "Script", "Else", "else" },
    { "Script", "For cycle", "for (var i = 0; i < ...; i++)" },
    { "Script", "While cycle", "while (condition)" },
    { "Script", "Set global valiable", "Client.SetGlobal(name, value);" },
    { "Script", "Get global valiable", "Client.GetGlobal(name);" },
    { "Script", "Clear all global valiables", "Client.ClearGlobals();" },
    { "Walker", "BlockMoving", "Client.BlockMoving(true/false);" },
    { "Walker", "CanWalk", "Client.CanWalk(direction, x, y, z);" },
    { "Walker", "Step", "Client.Step(direction, [run]);" },
    { "Walker", "WalkTo", "Client.WalkTo( x, y, z, distance);" },
    { "Walker", "StopWalking", "Client.StopWalking();" },
    { "Walker", "IsWalking", "Client.IsWalking();" },
    { "Dressing", "SetArm", "Client.SetArm('setName');" },
    { "Dressing", "UnsetArm", "Client.UnsetArm('setName');" },
    { "Dressing", "Arm", "Client.Arm('setName');" },
    { "Dressing", "Disarm", "Client.Disarm();" },
    { "Dressing", "SetDress", "Client.SetDress('setName');" },
    { "Dressing", "UnsetDress", "Client.UnsetDress('setName');" },
    { "Dressing", "Dress", "Client.Dress('setName');" },
    { "Dressing", "Undress", "Client.Undress();" },
    { "Dressing", "Equip", "Client.Equip('serial');" },
    { "Dressing", "EquipT", "Client.EquipT('graphic', ['color']);" },
    { "Journal", "ShowJournal", "Client.ShowJournal([linesCount]);" },
    { "Journal", "ClearJournal", "Client.ClearJournal(['pattern'], ['flags'], ['serial'], ['color']);" },
    { "Journal", "JournalIgnoreCase", "Client.JournalIgnoreCase(true/false);" },
    { "Journal", "InJournal", "Client.InJournal('pattern', ['flags'], ['serial'], ['color'], [startTime], [endTime]);" },
    { "Journal", "WaitJournal", "Client.WaitJournal('pattern', startTime, endTime, ['flags'], ['serial'], ['color']);" },
    { "Journal", "Last journal message", "Client.LastJournalMessage();" },
    { "Journal", "Get journal lines count", "Client.JournalCount();" },
    { "Journal", "Get journal line", "Client.JournalLine(index);" },
    { "Menu", "WaitMenu", "Client.WaitMenu(prompt, choice);" },
    { "Menu", "CancelWaitMenu", "Client.CancelWaitMenu();" },
    { "Menu", "InfoMenu", "Client.InfoMenu();" },
    { "Menu", "MenuCount", "Client.MenuCount();" },
    { "Menu", "GetMenu", "Client.GetMenu(name);" },
    { "Menu", "SelectMenu", "Client.SelectMenu(name, itemName);" },
    { "Menu", "CloseMenu", "Client.CloseMenu(name);" },
    { "Menu", "Waiting for menu", "Client.WaitForMenu([maxDelay=1000]);" },
    { "Gump", "Help gump", "Client.HelpGump();" },
    { "Gump", "Info gump", "Client.InfoGump([index=-1]);" },
    { "Gump", "Create gump hook", "Client.CreateGumpHook(index);" },
    { "Gump", "Wait gump", "Client.WaitGump(hook);" },
    { "Gump", "Cancel wait gump", "Client.CancelWaitGump();" },
    { "Gump", "Get gumps count", "Client.GumpCount();" },
    { "Gump", "Get last gump", "Client.GetLastGump();" },
    { "Gump", "Get gump by index", "Client.GetGump(index);" },
    { "Gump", "Get gump by serial & id", "Client.GetGump(serial, id);" },
    { "Gump", "Waiting for gump", "Client.WaitForGump([maxDelay=1000]);" },
    { "Timers", "Exists", "Client.TimerExists(name);" },
    { "Timers", "Add/Set value", "Client.SetTimer(name, [value=0]);" },
    { "Timers", "Get current value", "Client.Timer(name);" },
    { "Timers", "Remove", "Client.RemoveTimer(name);" },
    { "Timers", "Clear all timers", "Client.ClearTimers();" },
    { "Shop", "Buy", "Client.Buy(shopListName, [vendorName=''], [shopDelay=0]);" },
    { "Shop", "Sell", "Client.Sell(shopListName, [vendorName=''], [shopDelay=0]);" },
    { "Shop", "Waiting for shop", "Client.WaitForShop([maxDelay=1000]);" },
    { "Trade", "Count", "Client.TradeCount();" },
    { "Trade", "Container", "Client.TradeContainer('index', 'container');" },
    { "Trade", "Opponent", "Client.TradeOpponent('index');" },
    { "Trade", "Name", "Client.TradeName('index');" },
    { "Trade", "CheckState", "Client.TradeCheckState('index', 'container');" },
    { "Trade", "Check", "Client.TradeCheck('index', state);" },
    { "Trade", "Close", "Client.TradeClose('index');" },
    { "Trade", "Waiting for trade", "Client.WaitForTrade([maxDelay=1000]);" },
    { "Context menu", "Request", "Client.RequestContextMenu('serial');" },
    { "Context menu", "Wait context menu", "Client.WaitContextMenu('serial', index);" },
    { "Context menu", "Cancel context menu waiting", "Client.CancelContextMenu();" },
    { "Context menu", "Waiting for contect menu", "Client.WaitForContextMenu([maxDelay=1000]);" },
    { "Filters", "SetLight", "Client.SetLight(true/false, [amount]);" },
    { "Filters", "SetWeather", "Client.SetWeather(true/false, [index], [effectsCount], [temperature]);" },
    { "Filters", "SetSeason", "Client.SetSeason(true/false, [index], [musicIndex]);" },
    { "Text window", "Open", "TextWindow.Open();" },
    { "Text window", "Close", "TextWindow.Close();" },
    { "Text window", "Clear", "TextWindow.Clear();" },
    { "Text window", "Print", "TextWindow.Print('some text');" },
    { "Text window", "Set window position", "TextWindow.SetPos(x, y);" },
    { "Text window", "Set window size", "TextWindow.SetSize(width, height);" },
    { "Text window", "Save content to file", "TextWindow.SaveToFile('filePath');" },
    { "Other", "Info tile", "Client.InfoTile([lasttile]);" },
    { "Other", "Save config", "Client.SaveConfig();" },
    { "Other", "CloseUO", "Client.CloseUO();" },
    { "Other", "Sound", "Client.Sound(index);" },
    { "Other", "SaveHotkeys", "Client.SaveHotkeys('fileName');" },
    { "Other", "LoadHotkeys", "Client.LoadHotkeys('fileName');" },
    { "Other", "Track", "Client.Track(true/false, [x, y]);" },
    { "Other", "Now time (in ms)", "Client.Now();" },
    { "Other", "BuffExists", "Client.BuffExists(nameOrGraphic);" },
    { "Other", "Launch", "Client.Launch(filePath, [arguments]);" },
    { "Other", "Contains text", "Client.Contains(text, pattern, [ignoreCase=false]);" },
    { "Other", "Split text", "Client.Contains(text, [separator], [skipEmptyWords=true]);" },
    { "Other", "Time", "Client.Time([format]);" },
    { "Other", "Date", "Client.Date([format]);" },
    { "Other", "Assistant version", "Client.AssistVersion();" },
    { "Other", "Connected", "Client.Connected();" },
    { "Other", "Random", "Client.Random([value]);" },
    { "Other", "Random min max", "Client.Random(minValue, maxValue);" },
    { "Other", "Play WAV file", "Client.PlayWav('fileName');" },
    { "Other", "Save screenshot", "Client.Screenshot();" },
    { "Other", "LogOut", "Client.LogOut();" },
    { "Other", "Set view range", "Client.ClientViewRange(range);" },
    { "Other", "Get view range", "Client.ClientViewRange();" },
    { "Other", "Load profile", "Client.LoadProfile('name');" },
    { "Other", "set PVPCaller", "Client.SetPVPCaller(['serial']);" },
    { "Prompts", "Exists", "Client.PromptExists();" },
    { "Prompts", "Serial", "Client.PromptSerial();" },
    { "Prompts", "ID", "Client.PromptID();" },
    { "Prompts", "Wait prompt", "Client.WaitPrompt(text, [serial='0'], [type='all']);" },
    { "Prompts", "Cancel wait prompt", "Client.CancelWaitPrompt();" },
    { "Prompts", "Send current prompt", "Client.SendPrompt('text');" },
    { "Prompts", "Waiting for prompt", "Client.WaitForPrompt([maxDelay=1000]);" },
    { "Font color", "Enable and set your font color", "Client.SetFontColor(state, ['color']);" },
    { "Font color", "Get your font color state", "Client.GetFontColor();" },
    { "Font color", "Get your font color value", "Client.GetFontColorValue();" },
    { "Font color", "Enable and set characters font color", "Client.SetCharactersFontColor(state, ['color']);" },
    { "Font color", "Get characters font color state", "Client.GetCharactersFontColor();" },
    { "Font color", "Get characters font color value", "Client.GetCharactersFontColorValue();" },
    { "Work with files", "Create file object", "Client.NewFile();" },
    { "Work with files", "Open", ".Open('filePath', [checkExists]);" },
    { "Work with files", "Append", ".Append('filePath', [checkExists]);" },
    { "Work with files", "Opened", ".Opened();" },
    { "Work with files", "Close", ".Close();" },
    { "Work with files", "Remove", ".Remove(['filePath']);" },
    { "Work with files", "ReadLine", ".ReadLine();" },
    { "Work with files", "Read", ".Read();" },
    { "Work with files", "WriteLine", ".WriteLine('some text');" },
    { "Work with files", "Write", ".Write('some text');" },
    { "Work with lists", "Add (or save) type", "Client.AddType('typeName', ['typeValue']);" },
    { "Work with lists", "Remove type", "Client.RemoveType('typeName');" },
    { "Work with lists", "Add (or save) object", "Client.AddObject('objectName', ['objectValue']);" },
    { "Work with lists", "Remove object", "Client.RemoveObject('objectName');" },
    { "Work with lists", "Add find list item", "Client.AddFindList(['listName'=targetRequest], ['graphic', 'color'], ['comment']);" },
    { "Work with lists", "Clear find list", "Client.ClearFindList('listName');" },
    { "Work with lists", "Add ignore list item", "Client.AddIgnoreList(['listName'=targetRequest], ['graphic', 'color'], ['comment']);" },
    { "Work with lists", "Add ignore list object", "Client.AddIgnoreListObject(['listName'=targetRequest], ['serial'], ['comment']);" },
    { "Work with lists", "Clear ignore list", "Client.ClearIgnoreList('listName');" },
    { "Work with lists", "Add (or save) friend", "Client.AddFriend('friendName', ['serial']);" },
    { "Work with lists", "Remove friend", "Client.RemoveFriend('friendName');" },
    { "Work with lists", "Get friend list", "Client.GetFriendList();" },
    { "Work with lists", "Clear friend list", "Client.ClearFriendList();" },
    { "Work with lists", "Add (or save) enemy", "Client.AddEnemy('enemyName', ['serial']);" },
    { "Work with lists", "Remove enemy", "Client.RemoveEnemy('enemyName');" },
    { "Work with lists", "Get enemy list", "Client.GetEnemyList();" },
    { "Work with lists", "Clear enemy list", "Client.ClearEnemyList();" },
    { "Client options/Get", "Sound", "Client.OptionSound()" },
    { "Client options/Get", "SoundVolume", "Client.OptionSoundVolume()" },
    { "Client options/Get", "Music", "Client.OptionMusic()" },
    { "Client options/Get", "MusicVolume", "Client.OptionMusicVolume()" },
    { "Client options/Get", "UseTooltips", "Client.OptionUseTooltips()" },
    { "Client options/Get", "AlwaysRun", "Client.OptionAlwaysRun()" },
    { "Client options/Get", "NewTargetSystem", "Client.OptionNewTargetSystem()" },
    { "Client options/Get", "ObjectHandles", "Client.OptionObjectHandles()" },
    { "Client options/Get", "ScaleSpeech", "Client.OptionScaleSpeech()" },
    { "Client options/Get", "ScaleSpeechDelay", "Client.OptionScaleSpeechDelay()" },
    { "Client options/Get", "IgnoreGuildMessages", "Client.OptionIgnoreGuildMessages()" },
    { "Client options/Get", "IgnoreAllianceMessages", "Client.OptionIgnoreAllianceMessages()" },
    { "Client options/Get", "DarkNights", "Client.OptionDarkNights()" },
    { "Client options/Get", "ColoredLighting", "Client.OptionColoredLighting()" },
    { "Client options/Get", "CriminalActionsQuery", "Client.OptionCriminalActionsQuery()" },
    { "Client options/Get", "CircleOfTransparency", "Client.OptionCircleOfTransparency()" },
    { "Client options/Get", "CircleOfTransparencyValue", "Client.OptionCircleOfTransparencyValue()" },
    { "Client options/Get", "LockResizingGameWindow", "Client.OptionLockResizingGameWindow()" },
    { "Client options/Get", "FPSValue", "Client.OptionFPSValue()" },
    { "Client options/Get", "UseScalingGameWindow", "Client.OptionUseScalingGameWindow()" },
    { "Client options/Get", "DrawStatusState", "Client.OptionDrawStatusState()" },
    { "Client options/Get", "DrawStumps", "Client.OptionDrawStumps()" },
    { "Client options/Get", "MarkingCaves", "Client.OptionMarkingCaves()" },
    { "Client options/Get", "NoVegetation", "Client.OptionNoVegetation()" },
    { "Client options/Get", "NoFieldsAnimation", "Client.OptionNoFieldsAnimation()" },
    { "Client options/Get", "StandardCharactesFrameRate", "Client.OptionStandardCharactesFrameRate()" },
    { "Client options/Get", "StandardItemsFrameRate", "Client.OptionStandardItemsFrameRate()" },
    { "Client options/Get", "LockGumpsMoving", "Client.OptionLockGumpsMoving()" },
    { "Client options/Get", "EnterChat", "Client.OptionEnterChat()" },
    { "Client options/Get", "HiddenCharacters", "Client.OptionHiddenCharacters()" },
    { "Client options/Get", "HiddenCharactersAlpha", "Client.OptionHiddenCharactersAlpha()" },
    { "Client options/Get", "HiddenCharactersModeOnlyForSelf", "Client.OptionHiddenCharactersModeOnlyForSelf()" },
    { "Client options/Get", "TransparentSpellIcons", "Client.OptionTransparentSpellIcons()" },
    { "Client options/Get", "SpellIconsAlpha", "Client.OptionSpellIconsAlpha()" },
    { "Client options/Get", "FastRotation", "Client.OptionFastRotation()" },
    { "Client options/Set", "Sound", "Client.OptionSound(state);" },
    { "Client options/Set", "SoundVolume", "Client.OptionSoundVolume(value);" },
    { "Client options/Set", "Music", "Client.OptionMusic(state);" },
    { "Client options/Set", "MusicVolume", "Client.OptionMusicVolume(value);" },
    { "Client options/Set", "UseTooltips", "Client.OptionUseTooltips(state);" },
    { "Client options/Set", "AlwaysRun", "Client.OptionAlwaysRun(state);" },
    { "Client options/Set", "NewTargetSystem", "Client.OptionNewTargetSystem(state);" },
    { "Client options/Set", "ObjectHandles", "Client.OptionObjectHandles(state);" },
    { "Client options/Set", "ScaleSpeech", "Client.OptionScaleSpeech(state);" },
    { "Client options/Set", "ScaleSpeechDelay", "Client.OptionScaleSpeechDelay(value);" },
    { "Client options/Set", "IgnoreGuildMessages", "Client.OptionIgnoreGuildMessages(state);" },
    { "Client options/Set", "IgnoreAllianceMessages", "Client.OptionIgnoreAllianceMessages(state);" },
    { "Client options/Set", "DarkNights", "Client.OptionDarkNights(state);" },
    { "Client options/Set", "ColoredLighting", "Client.OptionColoredLighting(state);" },
    { "Client options/Set", "CriminalActionsQuery", "Client.OptionCriminalActionsQuery(state);" },
    { "Client options/Set", "CircleOfTransparency", "Client.OptionCircleOfTransparency(state);" },
    { "Client options/Set", "CircleOfTransparencyValue", "Client.OptionCircleOfTransparencyValue(value);" },
    { "Client options/Set", "LockResizingGameWindow", "Client.OptionLockResizingGameWindow(state);" },
    { "Client options/Set", "FPSValue", "Client.OptionFPSValue(value);" },
    { "Client options/Set", "UseScalingGameWindow", "Client.OptionUseScalingGameWindow(state);" },
    { "Client options/Set", "DrawStatusState", "Client.OptionDrawStatusState(state);" },
    { "Client options/Set", "DrawStumps", "Client.OptionDrawStumps(state);" },
    { "Client options/Set", "MarkingCaves", "Client.OptionMarkingCaves(state);" },
    { "Client options/Set", "NoVegetation", "Client.OptionNoVegetation(state);" },
    { "Client options/Set", "NoFieldsAnimation", "Client.OptionNoFieldsAnimation(state);" },
    { "Client options/Set", "StandardCharactesFrameRate", "Client.OptionStandardCharactesFrameRate(state);" },
    { "Client options/Set", "StandardItemsFrameRate", "Client.OptionStandardItemsFrameRate(state);" },
    { "Client options/Set", "LockGumpsMoving", "Client.OptionLockGumpsMoving(state);" },
    { "Client options/Set", "EnterChat", "Client.OptionEnterChat(state);" },
    { "Client options/Set", "HiddenCharacters", "Client.OptionHiddenCharacters(state);" },
    { "Client options/Set", "HiddenCharactersAlpha", "Client.OptionHiddenCharactersAlpha(value);" },
    { "Client options/Set", "HiddenCharactersModeOnlyForSelf", "Client.OptionHiddenCharactersModeOnlyForSelf(state);" },
    { "Client options/Set", "TransparentSpellIcons", "Client.OptionTransparentSpellIcons(state);" },
    { "Client options/Set", "SpellIconsAlpha", "Client.OptionSpellIconsAlpha(value);" },
    { "Client options/Set", "FastRotation", "Client.OptionFastRotation(state);" },
    { "Object properties/Journal object", "Serial", ".Serial()" },
    { "Object properties/Journal object", "Timer", ".Timer()" },
    { "Object properties/Journal object", "Color", ".Color()" },
    { "Object properties/Journal object", "Text", ".Text()" },
    { "Object properties/Journal object", "FindTextID", ".FindTextID()" },
    { "Object properties/Journal object", "SetText", ".SetText('newText')" },
    { "Object properties/Game object", "Serial", ".Serial()" },
    { "Object properties/Game object", "Graphic", ".Graphic()" },
    { "Object properties/Game object", "Color", ".Color()" },
    { "Object properties/Game object", "X", ".X()" },
    { "Object properties/Game object", "Y", ".Y()" },
    { "Object properties/Game object", "Z", ".Z()" },
    { "Object properties/Game object", "Container", ".Container()" },
    { "Object properties/Game object", "Map", ".Map()" },
    { "Object properties/Game object", "Count", ".Count()" },
    { "Object properties/Game object", "Flags", ".Flags()" },
    { "Object properties/Game object", "Name", ".Name()" },
    { "Object properties/Game object", "Mobile", ".Mobile()" },
    { "Object properties/Game object", "Ignored", ".Ignored()" },
    { "Object properties/Game object", "Frozen", ".Frozen()" },
    { "Object properties/Game object", "Poisoned", ".Poisoned()" },
    { "Object properties/Game object", "Flying", ".Flying()" },
    { "Object properties/Game object", "YellowHits", ".YellowHits()" },
    { "Object properties/Game object", "IgnoreCharacters", ".IgnoreCharacters()" },
    { "Object properties/Game object", "Locked", ".Locked()" },
    { "Object properties/Game object", "WarMode", ".WarMode()" },
    { "Object properties/Game object", "Hidden", ".Hidden()" },
    { "Object properties/Game object", "IsHuman", ".IsHuman()" },
    { "Object properties/Game object", "IsPlayer", ".IsPlayer()" },
    { "Object properties/Game object", "IsCorpse", ".IsCorpse()" },
    { "Object properties/Game object", "Layer", ".Layer()" },
    { "Object properties/Game object", "IsMulti", ".IsMulti()" },
    { "Object properties/Game object", "EquipLayer", ".EquipLayer()" },
    { "Object properties/Game object", "Hits", ".Hits()" },
    { "Object properties/Game object", "MaxHits", ".MaxHits()" },
    { "Object properties/Game object", "Mana", ".Mana()" },
    { "Object properties/Game object", "MaxMana", ".MaxMana()" },
    { "Object properties/Game object", "Stam", ".Stam()" },
    { "Object properties/Game object", "MaxStam", ".MaxStam()" },
    { "Object properties/Game object", "Female", ".Female()" },
    { "Object properties/Game object", "Race", ".Race()" },
    { "Object properties/Game object", "Direction", ".Direction()" },
    { "Object properties/Game object", "Notoriety", ".Notoriety()" },
    { "Object properties/Game object", "CanChangeName", ".CanChangeName()" },
    { "Object properties/Game object", "Dead", ".Dead()" },
    { "Object properties/Game object", "Exists", ".Exists()" },
    { "Object properties/Game object", "Properties", ".Properties()" },
    { "Object properties/Game object", "ProfileReceived", ".ProfileReceived()" },
    { "Object properties/Game object", "Profile", ".Profile()" },
    { "Object properties/Game object", "Title", ".Title()" },
    { "Object properties/Player", "Serial", "Player.Serial()" },
    { "Object properties/Player", "Graphic", "Player.Graphic()" },
    { "Object properties/Player", "Color", "Player.Color()" },
    { "Object properties/Player", "X", "Player.X()" },
    { "Object properties/Player", "Y", "Player.Y()" },
    { "Object properties/Player", "Z", "Player.Z()" },
    { "Object properties/Player", "Container", "Player.Container()" },
    { "Object properties/Player", "Map", "Player.Map()" },
    { "Object properties/Player", "Count", "Player.Count()" },
    { "Object properties/Player", "Flags", "Player.Flags()" },
    { "Object properties/Player", "Name", "Player.Name()" },
    { "Object properties/Player", "Mobile", "Player.Mobile()" },
    { "Object properties/Player", "Ignored", "Player.Ignored()" },
    { "Object properties/Player", "Frozen", "Player.Frozen()" },
    { "Object properties/Player", "Poisoned", "Player.Poisoned()" },
    { "Object properties/Player", "Paralyzed", "Player.Paralyzed()" },
    { "Object properties/Player", "Flying", "Player.Flying()" },
    { "Object properties/Player", "YellowHits", "Player.YellowHits()" },
    { "Object properties/Player", "IgnoreCharacters", "Player.IgnoreCharacters()" },
    { "Object properties/Player", "Locked", "Player.Locked()" },
    { "Object properties/Player", "WarMode", "Player.WarMode()" },
    { "Object properties/Player", "Hidden", "Player.Hidden()" },
    { "Object properties/Player", "IsHuman", "Player.IsHuman()" },
    { "Object properties/Player", "IsPlayer", "Player.IsPlayer()" },
    { "Object properties/Player", "IsCorpse", "Player.IsCorpse()" },
    { "Object properties/Player", "Layer", "Player.Layer()" },
    { "Object properties/Player", "IsMulti", "Player.IsMulti()" },
    { "Object properties/Player", "EquipLayer", "Player.EquipLayer()" },
    { "Object properties/Player", "Hits", "Player.Hits()" },
    { "Object properties/Player", "MaxHits", "Player.MaxHits()" },
    { "Object properties/Player", "Mana", "Player.Mana()" },
    { "Object properties/Player", "MaxMana", "Player.MaxMana()" },
    { "Object properties/Player", "Stam", "Player.Stam()" },
    { "Object properties/Player", "MaxStam", "Player.MaxStam()" },
    { "Object properties/Player", "Female", "Player.Female()" },
    { "Object properties/Player", "Race", "Player.Race()" },
    { "Object properties/Player", "Direction", "Player.Direction()" },
    { "Object properties/Player", "Notoriety", "Player.Notoriety()" },
    { "Object properties/Player", "CanChangeName", "Player.CanChangeName()" },
    { "Object properties/Player", "Dead", "Player.Dead()" },
    { "Object properties/Player", "Str", "Player.Str()" },
    { "Object properties/Player", "Int", "Player.Int()" },
    { "Object properties/Player", "Dex", "Player.Dex()" },
    { "Object properties/Player", "LockStrState", "Player.LockStrState()" },
    { "Object properties/Player", "LockIntState", "Player.LockIntState()" },
    { "Object properties/Player", "LockDexState", "Player.LockDexState()" },
    { "Object properties/Player", "Weight", "Player.Weight()" },
    { "Object properties/Player", "MaxWeight", "Player.MaxWeight()" },
    { "Object properties/Player", "Armor", "Player.Armor()" },
    { "Object properties/Player", "Gold", "Player.Gold()" },
    { "Object properties/Player", "StatsCap", "Player.StatsCap()" },
    { "Object properties/Player", "Followers", "Player.Followers()" },
    { "Object properties/Player", "MaxFollowers", "Player.MaxFollowers()" },
    { "Object properties/Player", "FireResistance", "Player.FireResistance()" },
    { "Object properties/Player", "ColdResistance", "Player.ColdResistance()" },
    { "Object properties/Player", "PoisonResistance", "Player.PoisonResistance()" },
    { "Object properties/Player", "EnergyResistance", "Player.EnergyResistance()" },
    { "Object properties/Player", "Luck", "Player.Luck()" },
    { "Object properties/Player", "MinDamage", "Player.MinDamage()" },
    { "Object properties/Player", "MaxDamage", "Player.MaxDamage()" },
    { "Object properties/Player", "TithingPoints", "Player.TithingPoints()" },
    { "Object properties/Player", "StealthSteps", "Player.StealthSteps()" },
    { "Object properties/Player", "Properties", "Player.Properties()" },
    { "Object properties/Player", "ProfileReceived", "Player.ProfileReceived()" },
    { "Object properties/Player", "Profile", "Player.Profile()" },
    { "Object properties/Player", "Title", "Player.Title()" },
    { "Object properties/Menu", "Serial", ".Serial()" },
    { "Object properties/Menu", "ID", ".ID()" },
    { "Object properties/Menu", "Name", ".Name()" },
    { "Object properties/Menu", "IsGrayMenu", ".IsGrayMenu()" },
    { "Object properties/Menu", "Select by index", ".Select(index)" },
    { "Object properties/Menu", "Select by name", ".Select(name)" },
    { "Object properties/Menu", "Close", ".Close()" },
    { "Object properties/Menu", "ItemsCount", ".ItemsCount()" },
    { "Object properties/Menu", "Item ID by index", ".ItemID(index)" },
    { "Object properties/Menu", "Item Graphic by index", ".ItemGraphic(index)" },
    { "Object properties/Menu", "Item Color by index", ".ItemColor(index)" },
    { "Object properties/Menu", "Item Name by index", ".ItemName(index)" },
    { "Object properties/Coordinates", "X", ".X()" },
    { "Object properties/Coordinates", "Y", ".Y()" },
    { "Object properties/Gump", "Serial", ".Serial()" },
    { "Object properties/Gump", "ID", ".ID()" },
    { "Object properties/Gump", "X", ".X()" },
    { "Object properties/Gump", "Y", ".Y()" },
    { "Object properties/Gump", "Replayed", ".Replayed()" },
    { "Object properties/Gump", "ReplyID", ".ReplyID()" },
    { "Object properties/Gump", "ButtonList", ".ButtonList()" },
    { "Object properties/Gump", "CheckboxList", ".CheckboxList()" },
    { "Object properties/Gump", "RadioList", ".RadioList()" },
    { "Object properties/Gump", "TilepicList", ".TilepicList()" },
    { "Object properties/Gump", "GumppicList", ".GumppicList()" },
    { "Object properties/Gump", "EntriesList", ".EntriesList()" },
    { "Object properties/Gump", "CommandList", ".CommandList()" },
    { "Object properties/Gump", "TextList", ".TextList()" },
    { "Object properties/Gump", "Command", ".Command(index)" },
    { "Object properties/Gump", "Text", ".Text(index)" },
    { "Object properties/Gump", "Select", ".Select(hook)" },
    { "Object properties/Gump", "Close", ".Close()" },
    { "Object properties/Gump hook", "Index", ".Index()" },
    { "Object properties/Gump hook", "AddEntry", ".AddEntry(index, 'text')" },
    { "Object properties/Gump hook", "AddCheck", ".AddCheck(index, state)" },
    { "Object properties/Selected slient tile", "Serial", "SelectedTile.Serial()" },
    { "Object properties/Selected slient tile", "Graphic", "SelectedTile.Graphic()" },
    { "Object properties/Selected slient tile", "Color", "SelectedTile.Color()" },
    { "Object properties/Selected slient tile", "X", "SelectedTile.X()" },
    { "Object properties/Selected slient tile", "Y", "SelectedTile.Y()" },
    { "Object properties/Selected slient tile", "Z", "SelectedTile.Z()" },
    { "Object properties/Selected slient tile", "Land tile graphic", "SelectedTile.LandGraphic()" },
    { "Object properties/Selected slient tile", "Land tile x", "SelectedTile.LandX()" },
    { "Object properties/Selected slient tile", "Land tile y", "SelectedTile.LandY()" },
    { "Object properties/Selected slient tile", "Land tile z", "SelectedTile.LandZ()" },
    { "Object properties/Selected slient tile", "IsLandTile", "SelectedTile.IsLandTile()" },
    { "Object properties/Selected slient tile", "IsStaticTile", "SelectedTile.IsStaticTile()" },
    { "Object properties/Selected slient tile", "IsGameObject", "SelectedTile.IsGameObject()" },
    { "Object properties", "Create client macro", "Client.CreateClientMacro(['action'], ['subAction'])" },
    { "Object properties/Client macro", "Add macro action to queue", "macro.AddAction('action', ['subAction'])" },
    { "Object properties/Client macro", "Play macro queue", "macro.Play([waitForEndOfPlaying=false], [maxDelay=10050000])" },
};


namespace xuoassist {

namespace {

// forward-path split: "A/B" -> ["A", "B"]
vector<astr_t> split_path(const char *path)
{
    vector<astr_t> parts;
    const char *start = path;
    for (const char *p = path;; p++)
    {
        if (*p == '/' || *p == 0)
        {
            parts.push_back(astr_t(start, p - start));
            if (*p == 0)
            {
                break;
            }
            start = p + 1;
        }
    }
    return parts;
}

struct MENU_NODE
{
    astr_t label;
    vector<int> items;   // indices into g_MenuItems at this node (leaf)
    vector<int> children; // indices into nodes (sub-menu)
};

// Builds the tree from the flat table (called once; the table is static).
vector<MENU_NODE> build_tree()
{
    vector<MENU_NODE> nodes;
    nodes.push_back({ "", {}, {} }); // root

    for (int i = 0; i < (int)(sizeof(g_MenuItems) / sizeof(g_MenuItems[0])); i++)
    {
        vector<astr_t> parts = split_path(g_MenuItems[i].Path);
        int current = 0;
        for (size_t depth = 0; depth + 1 < parts.size(); depth++)
        {
            int next = -1;
            for (int child : nodes[current].children)
            {
                if (nodes[child].label == parts[depth])
                {
                    next = child;
                    break;
                }
            }
            if (next < 0)
            {
                nodes.push_back({ parts[depth], {}, {} });
                next = (int)nodes.size() - 1;
                nodes[current].children.push_back(next);
            }
            current = next;
        }
        // leaf label + item
        const astr_t &leaf = parts.back();
        int leafIndex = -1;
        for (int child : nodes[current].children)
        {
            if (nodes[child].label == leaf && nodes[child].items.empty() &&
                nodes[child].children.empty())
            {
                leafIndex = child;
                break;
            }
        }
        if (leafIndex < 0)
        {
            nodes.push_back({ leaf, {}, {} });
            leafIndex = (int)nodes.size() - 1;
            nodes[current].children.push_back(leafIndex);
        }
        nodes[leafIndex].items.push_back(i);
    }
    return nodes;
}

const vector<MENU_NODE> &menu_tree()
{
    static const vector<MENU_NODE> tree = build_tree();
    return tree;
}

bool draw_leaf(const MENU_NODE &node, astr_t &inserted)
{
    for (int item : node.items)
    {
        if (ImGui::MenuItem(g_MenuItems[item].Label))
        {
            inserted = g_MenuItems[item].Insertion;
            return true;
        }
    }
    return false;
}

bool draw_node(const MENU_NODE &node, astr_t &inserted)
{
    if (draw_leaf(node, inserted))
    {
        return true;
    }
    for (int child : node.children)
    {
        const MENU_NODE &sub = menu_tree()[child];
        if (sub.items.empty() && sub.children.empty())
        {
            continue;
        }
                if (ImGui::BeginMenu(sub.label.c_str()))
        {
            if (draw_node(sub, inserted))
            {
                ImGui::EndMenu();
                return true;
            }
            ImGui::EndMenu();
        }
    }
    return false;
}

// The dynamic Spells menu.
bool draw_spells(astr_t &inserted)
{
    bool picked = false;
    if (ImGui::BeginMenu("Spells"))
    {
        static const char *const BOOKS[] = { "Mage", "Necromancer", "Paladin", "Bushido",
            "Ninjitsu", "Spellweaving", "Mysticism" };
        const int count = (int)g_SpellManager.m_SpellbookSpellNames.size() < 7
            ? (int)g_SpellManager.m_SpellbookSpellNames.size()
            : 7;
        for (int i = 0; i < count; i++)
        {
            if (ImGui::BeginMenu(BOOKS[i]))
            {
                for (const astr_t &spell : g_SpellManager.m_SpellbookSpellNames[i])
                {
                    if (spell.empty())
                    {
                        continue;
                    }
                    if (ImGui::MenuItem(spell.c_str()))
                    {
                        inserted = "Client.Cast('" + spell + "', ['targetSerial']);";
                        picked = true;
                    }
                }
                ImGui::EndMenu();
            }
        }
        ImGui::EndMenu();
    }
    return picked;
}

} // namespace

bool ScriptMenu_Draw(const char *popupId, astr_t &inserted)
{
    bool picked = false;
    if (!ImGui::BeginPopup(popupId))
    {
        return false;
    }

    if (draw_node(menu_tree().front(), inserted))
    {
        picked = true;
    }
    if (draw_spells(inserted))
    {
        picked = true;
    }

    ImGui::EndPopup();
    return picked;
}

} // namespace xuoassist
