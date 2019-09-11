// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "GameObjects/GameWorld.h"
#include <xuocore/plugininterface.h>

struct CIndexObjectStatic;
struct CIndexObject;
struct CIndexSound;
struct CUopMappedFile;

class CGame
{
public:
    int TexturesDataCount = 0;
    string m_OverrideServerAddress;
    int m_OverrideServerPort = 0;

    uint16_t m_WinterTile[MAX_LAND_DATA_INDEX_COUNT];
    std::vector<std::pair<uint16_t, uint16_t>> m_IgnoreInFilterTiles;
    std::deque<CIndexObjectStatic *> m_StaticAnimList;
    std::deque<CIndexObject *> m_UsedLandList;
    std::deque<CIndexObject *> m_UsedStaticList;
    std::deque<CIndexObject *> m_UsedGumpList;
    std::deque<CIndexObject *> m_UsedTextureList;
    std::deque<CIndexObject *> m_UsedLightList;
    std::deque<CIndexSound *> m_UsedSoundList;

private:
    uint8_t m_StaticTilesFilterFlags[MAX_STATIC_DATA_INDEX_COUNT];
    string m_GameServerIP = "";

    void LoadAutoLoginNames();
    void ProcessStaticAnimList();
    void PatchFiles();
    void IndexReplaces();
    void InitStaticAnimList();
    void LoadClientStartupConfig();
    void LoadShaders();
    void CreateAuraTexture();
    void CreateObjectHandlesBackground();
    void ClearUnusedTextures();
    void UnloadIndexFiles();

    uint16_t TextToGraphic(const char *text);
    void CheckStaticTileFilterFiles();
    string DecodeArgumentString(const char *text, int length);
    void LoadPlugin(const os_path &libpath, const string &function, int flags);
    bool InstallPlugin(PluginEntry *initFunc, int flags);
    void LoadContainerOffsets();

public:
    CGame();
    ~CGame();

    bool Install();
    void Uninstall();
    void LoadPlugins();
    void ProcessCommandLine();

    void InitScreen(GAME_STATE state);
    void ProcessDelayedClicks();

    void Process(bool rendering = false);
    void LoadStartupConfig(int serial);
    void LoadLocalConfig(int serial, string characterName);
    void SaveLocalConfig(int serial);

    bool InTileFilter(uint16_t graphic);

    static string FixServerName(string name);

    void Connect();
    void Disconnect();
    int Send(uint8_t *buf, int size);
    int Send(const vector<uint8_t> &buf) { return Send((uint8_t *)&buf[0], int(buf.size())); }
    void ServerSelection(int pos);
    void RelayServer(const char *ip, int port, uint8_t *gameSeed);
    void CharacterSelection(int pos);
    void LoginComplete(bool reload);
    void ChangeSeason(const SEASON_TYPE &season, int music);

    uint16_t GetLandSeasonGraphic(uint16_t graphic);
    uint16_t GetSeasonGraphic(uint16_t graphic);
    uint16_t GetSpringGraphic(uint16_t graphic);
    uint16_t GetSummerGraphic(uint16_t graphic);
    uint16_t GetFallGraphic(uint16_t graphic);
    uint16_t GetWinterGraphic(uint16_t graphic);
    uint16_t GetDesolationGraphic(uint16_t graphic);

    int ValueInt(const VALUE_KEY_INT &key, int value = -1);
    string ValueString(const VALUE_KEY_STRING &key, string value = "");

    void ClearRemovedStaticsTextures();
    void ClearTreesTextures();
    bool IsTreeTile(uint16_t graphic, int &index);
    void ClearCaveTextures();
    bool IsCaveTile(uint16_t graphic);
    bool IsVegetation(uint16_t graphic);
    uint64_t GetLandFlags(uint16_t id);
    uint64_t GetStaticFlags(uint16_t id);
    uint16_t GetLightColor(uint16_t id);
    CSize GetStaticArtDimension(uint16_t id);
    CSize GetGumpDimension(uint16_t id);
    CSprite *ExecuteGump(uint16_t id);
    CSprite *ExecuteLandArt(uint16_t id);
    CSprite *ExecuteStaticArt(uint16_t id);
    CSprite *ExecuteStaticArtAnimated(uint16_t id);
    CSprite *ExecuteTexture(uint16_t id);
    CSprite *ExecuteLight(uint8_t &id);
    bool ExecuteGumpPart(uint16_t id, int count);
    bool ExecuteResizepic(uint16_t id) { return ExecuteGumpPart(id, 9); }
    bool ExecuteButton(uint16_t id) { return ExecuteGumpPart(id, 3); }
    void DrawGump(uint16_t id, uint16_t color, int x, int y, bool partialHue = false);
    void DrawGump(
        uint16_t id, uint16_t color, int x, int y, int width, int height, bool partialHue = false);
    void DrawResizepicGump(uint16_t id, int x, int y, int width, int height);
    void DrawLandTexture(class CLandObject *land, uint16_t color, int x, int y);
    void DrawLandArt(uint16_t id, uint16_t color, int x, int y);
    void DrawStaticArt(uint16_t id, uint16_t color, int x, int y, bool selection = false);
    void DrawStaticArtAnimated(uint16_t id, uint16_t color, int x, int y, bool selection = false);
    void DrawStaticArtRotated(uint16_t id, uint16_t color, int x, int y, float angle);
    void DrawStaticArtAnimatedRotated(uint16_t id, uint16_t color, int x, int y, float angle);
    void
    DrawStaticArtTransparent(uint16_t id, uint16_t color, int x, int y, bool selection = false);
    void DrawStaticArtAnimatedTransparent(
        uint16_t id, uint16_t color, int x, int y, bool selection = false);
    void DrawStaticArtInContainer(
        uint16_t id, uint16_t color, int x, int y, bool selection = false, bool onMouse = false);
    void DrawLight(struct LIGHT_DATA &light);
    bool PolygonePixelsInXY(int x, int y, int width, int height);
    bool GumpPixelsInXY(uint16_t id, int x, int y);
    bool GumpPixelsInXY(uint16_t id, int x, int y, int width, int height);
    bool ResizepicPixelsInXY(uint16_t id, int x, int y, int width, int height);
    bool StaticPixelsInXY(uint16_t id, int x, int y);
    bool StaticPixelsInXYAnimated(uint16_t id, int x, int y);
    bool StaticPixelsInXYInContainer(uint16_t id, int x, int y);
    bool LandPixelsInXY(uint16_t id, int x, int y);
    bool LandTexturePixelsInXY(int x, int y, const SDL_Rect &r);
    void CreateTextMessageF(uint8_t font, uint16_t color, const char *format, ...);
    void CreateUnicodeTextMessageF(uint8_t font, uint16_t color, const char *format, ...);
    void CreateTextMessage(
        const TEXT_TYPE &type,
        int serial,
        uint8_t font,
        uint16_t color,
        const string &text,
        class CRenderWorldObject *clientObj = nullptr);
    void CreateUnicodeTextMessage(
        const TEXT_TYPE &type,
        int serial,
        uint8_t font,
        uint16_t color,
        const wstring &text,
        class CRenderWorldObject *clientObj = nullptr);
    void AddSystemMessage(CTextData *msg);
    void AddJournalMessage(CTextData *msg, const string &name);
    void ChangeMap(uint8_t newmap);
    void PickupItem(class CGameItem *obj, int count = 0, bool isGameFigure = false);
    void DropItem(int container, uint16_t x, uint16_t y, char z);
    void EquipItem(uint32_t container = 0);
    void ChangeWarmode(uint8_t status = 0xFF);
    void Click(uint32_t serial);
    void DoubleClick(uint32_t serial);
    void PaperdollReq(uint32_t serial);
    void Attack(uint32_t serial);
    void AttackReq(uint32_t serial);
    void SendASCIIText(const char *str, SPEECH_TYPE type);
    void CastSpell(int index);
    void CastSpellFromBook(int index, uint32_t serial);
    void UseSkill(int index);
    void OpenDoor();
    void EmoteAction(const char *action);
    void AllNames();
    void LoadLogin(string &login, int &port);
    void GoToWebLink(const string &url);
    void RemoveRangedObjects();
    void ClearWorld();
    void LogOut();
    void ConsolePromptSend();
    void ConsolePromptCancel();

    void PlayMusic(int index, bool warmode = false);
    void PlaySoundEffect(uint16_t id, float volume = -1);
    void PlaySoundEffectAtPosition(uint16_t id, int x, int y);
    void AdjustSoundEffects(int ticks, float volume = -1);
    void PauseSound() const;
    void ResumeSound() const;

    void OpenStatus(uint32_t serial);
    void DisplayStatusbarGump(int serial, int x, int y);
    void OpenMinimap();
    void OpenWorldMap();
    void OpenJournal();
    void OpenSkills();
    void OpenBackpack();
    void OpenLogOut();
    void OpenChat();
    void OpenConfiguration();
    void OpenMail();
    void OpenPartyManifest();
    void OpenProfile(uint32_t serial = 0);
    void DisconnectGump();
    void OpenCombatBookGump();
    void OpenRacialAbilitiesBookGump();
    void StartReconnect();
};

extern CGame g_Game;
