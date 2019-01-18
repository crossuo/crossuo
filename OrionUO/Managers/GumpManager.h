// MIT License
// Copyright (C) August 2016 Hotride

#pragma once

#include "Platform.h"
#include "../BaseQueue.h"

class CGumpManager : public CBaseQueue
{
private:
    int GetNonpartyStatusbarsCount();
    void SaveDefaultGumpProperties(Wisp::CBinaryFileWriter &writer, CGump *gump, int size);

public:
    CGumpManager()
        : CBaseQueue()
    {
    }
    virtual ~CGumpManager() {}

    void AddGump(CGump *obj);
    CGump *UpdateContent(int serial, int id, const GUMP_TYPE &type);
    CGump *UpdateGump(int serial, int id, const GUMP_TYPE &type);
    CGump *GetGump(int serial, int id, const GUMP_TYPE &type);
    CGump *GetTextEntryOwner();
    CGump *GumpExists(uintptr_t gumpID);
    void CloseGump(uint32_t serial, uint32_t ID, GUMP_TYPE Type);
    void RemoveGump(CGump *obj);
    void RedrawAll();
    void OnDelete();
    void RemoveRangedGumps();
    void PrepareContent();
    void RemoveMarked();
    void PrepareTextures();
    void Draw(bool blocked);
    void Select(bool blocked);
    void InitToolTip();
    void OnLeftMouseButtonDown(bool blocked);
    bool OnLeftMouseButtonUp(bool blocked);
    bool OnLeftMouseButtonDoubleClick(bool blocked);
    void OnRightMouseButtonDown(bool blocked);
    void OnRightMouseButtonUp(bool blocked);
    bool OnRightMouseButtonDoubleClick(bool blocked) { return false; }
    void OnMidMouseButtonScroll(bool up, bool blocked);
    void OnDragging(bool blocked);
    void Load(const os_path &path);
    void Save(const os_path &path);
    virtual bool OnTextInput(const TextEvent &ev, bool blocked);
    virtual bool OnKeyDown(const KeyEvent &ev, bool blocked);
};

extern CGumpManager g_GumpManager;
