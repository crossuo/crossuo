// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: 2016 Hotride

#pragma once

#include <common/fs.h>
#include <xuocore/enumlist.h>
#include "../Platform.h"
#include "../BaseQueue.h"
#include "../Wisp.h"

class CGump;

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
    CGump *UpdateContent(uint32_t serial, int id, const GUMP_TYPE &type);
    CGump *UpdateGump(uint32_t serial, int id, const GUMP_TYPE &type);
    CGump *GetGump(uint32_t serial, int id, const GUMP_TYPE &type);
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
    void Load(const fs_path &path);
    void Save(const fs_path &path);
    virtual bool OnTextInput(const TextEvent &ev, bool blocked);
    virtual bool OnKeyDown(const KeyEvent &ev, bool blocked);
};

extern CGumpManager g_GumpManager;
