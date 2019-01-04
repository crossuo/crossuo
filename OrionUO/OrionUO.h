// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "plugin/plugininterface.h"

bool __cdecl PluginRecvFunction(uint8_t *buf, size_t size);
bool __cdecl PluginSendFunction(uint8_t *buf, size_t size);

class COrion
{
public:
    int TexturesDataCount = 0;
    string m_OverrideServerAddress;
    int m_OverrideServerPort = 0;

private:
    uint32_t m_CRC_Table[256];

    uint8_t m_StaticTilesFilterFlags[0x10000];

    vector<uint16_t> m_StumpTiles;
    vector<uint16_t> m_CaveTiles;

    deque<CIndexObjectStatic *> m_StaticAnimList;

    deque<CIndexObject *> m_UsedLandList;
    deque<CIndexObject *> m_UsedStaticList;
    deque<CIndexObject *> m_UsedGumpList;
    deque<CIndexObject *> m_UsedTextureList;
    deque<CIndexSound *> m_UsedSoundList;
    deque<CIndexObject *> m_UsedLightList;

    vector<uint8_t> m_AnimData;

    string m_GameServerIP = "";

    void LoadAutoLoginNames();
    void LoadTiledata(int landSize, int staticsSize);
    void LoadIndexFiles();
    void UnloadIndexFiles();
    void InitStaticAnimList();
    uint16_t CalculateLightColor(uint16_t id);
    void ProcessStaticAnimList();
    void PatchFiles();
    void IndexReplaces();
    void LoadClientStartupConfig();
    void LoadShaders();
    void CreateAuraTexture();
    void CreateObjectHandlesBackground();
    void ClearUnusedTextures();
    void ReadMulIndexFile(
        size_t indexMaxCount,
        const std::function<CIndexObject *(int index)> &getIdxObj,
        size_t address,
        BASE_IDX_BLOCK *ptr,
        const std::function<BASE_IDX_BLOCK *()> &getNewPtrValue);
    void ReadUOPIndexFile(
        size_t indexMaxCount,
        const std::function<CIndexObject *(int index)> &getIdxObj,
        const char *uopFileName,
        int padding,
        const char *extesion,
        CUopMappedFile &uopFile,
        int startIndex = 0);
    uint16_t TextToGraphic(const char *text);
    void CheckStaticTileFilterFiles();
    string DecodeArgumentString(const char *text, int length);
    void ParseCommandLine();
    void LoadPlugin(const os_path &libpath, const string &function, int flags);
    bool InstallPlugin(PluginEntry *initFunc, int flags);
    void LoadContainerOffsets();

public:
    COrion();
    ~COrion();

    static uint64_t CreateHash(const char *s);

    vector<LAND_TILES> m_LandData;
    vector<STATIC_TILES> m_StaticData;

    bool Install();
    void Uninstall();

    void InitScreen(GAME_STATE state);
    void ProcessDelayedClicks();

    void Process(bool rendering = false);
    void LoadStartupConfig(int serial);
    void LoadPluginConfig();
    void LoadLocalConfig(int serial);
    void SaveLocalConfig(int serial);

    CIndexObjectLand m_LandDataIndex[MAX_LAND_DATA_INDEX_COUNT];
    CIndexObjectStatic m_StaticDataIndex[MAX_STATIC_DATA_INDEX_COUNT];
    CIndexGump m_GumpDataIndex[MAX_GUMP_DATA_INDEX_COUNT];
    CIndexObject m_TextureDataIndex[MAX_LAND_TEXTURES_DATA_INDEX_COUNT];
    CIndexSound m_SoundDataIndex[MAX_SOUND_DATA_INDEX_COUNT];
    CIndexMusic m_MP3Data[MAX_MUSIC_DATA_INDEX_COUNT];
    CIndexMulti m_MultiDataIndex[MAX_MULTI_DATA_INDEX_COUNT];
    CIndexLight m_LightDataIndex[MAX_LIGHTS_DATA_INDEX_COUNT];

    uint16_t m_WinterTile[MAX_LAND_DATA_INDEX_COUNT];

    vector<std::pair<uint16_t, uint16_t>> m_IgnoreInFilterTiles;

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
    uint16_t GetLightColor(uint16_t id) { return m_StaticDataIndex[id].LightColor; }
    Wisp::CSize GetStaticArtDimension(uint16_t id);
    Wisp::CSize GetGumpDimension(uint16_t id);
    CGLTexture *ExecuteGump(uint16_t id);
    CGLTexture *ExecuteLandArt(uint16_t id);
    CGLTexture *ExecuteStaticArt(uint16_t id);
    CGLTexture *ExecuteStaticArtAnimated(uint16_t id);
    CGLTexture *ExecuteTexture(uint16_t id);
    CGLTexture *ExecuteLight(uint8_t &id);
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
    void AddSystemMessage(class CTextData *msg);
    void AddJournalMessage(class CTextData *msg, const string &name);
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
    uint32_t GetFileHashCode(uint8_t *ptr, size_t size);
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

extern COrion g_Orion;
