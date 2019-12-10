// MIT License
// Copyright (C) August 2016 Hotride

#include "GameObject.h"
#include "GameEffect.h"
#include "GameCharacter.h"
#include <SDL_timer.h>
#include <xuocore/uodata.h>
#include <common/str.h>
#include "../Config.h"
#include "../Point.h"
#include "../CrossUO.h"
#include "../ServerList.h"
#include "../SelectedObject.h"
#include "../GameWindow.h"
#include "../Managers/AnimationManager.h"
#include "../Managers/GumpManager.h"
#include "../Managers/ClilocManager.h"
#include "../Managers/MouseManager.h"
#include "../Managers/FontsManager.h"
#include "../Managers/ConfigManager.h"
#include "../Managers/ColorManager.h"
#include "../TextEngine/TextData.h"
#include "../Renderer/RenderAPI.h"

static int s_objectHandleOffsetY = 25;
static int s_bodyHandleOffsetY = 15;

CGameObject::CGameObject(int serial)
    : CRenderStaticObject(ROT_GAME_OBJECT, serial, 0, 0, 0, 0, 0)
    , LastAnimationChangeTime(SDL_GetTicks())
{
    memset(&m_FrameInfo, 0, sizeof(DRAW_FRAME_INFORMATION));

    g_GameObjectsCount++;
}

CGameObject::~CGameObject()
{
    if (m_Effects != nullptr)
    {
        delete m_Effects;
        m_Effects = nullptr;
    }

    m_Next = nullptr;
    m_Prev = nullptr;
#ifndef NEW_RENDERER_ENABLED
    if (m_TextureObjectHandles.Texture != 0)
    {
        glDeleteTextures(1, &m_TextureObjectHandles.Texture);
        m_TextureObjectHandles.Texture = 0;
    }
#else
    if (m_TextureObjectHandles.Texture != RENDER_TEXTUREHANDLE_INVALID)
    {
        Render_DestroyTexture(m_TextureObjectHandles.Texture);
        m_TextureObjectHandles.Texture = RENDER_TEXTUREHANDLE_INVALID;
    }
#endif

    Clear();

    g_GameObjectsCount--;
}

void CGameObject::SetFlags(uint8_t val)
{
    bool poisoned = Poisoned();
    bool yellowHits = YellowHits();
    m_Flags = val;
    if (poisoned != Poisoned() || yellowHits != YellowHits())
    {
        g_GumpManager.UpdateContent(Serial, 0, GT_STATUSBAR);
        g_GumpManager.UpdateContent(Serial, 0, GT_TARGET_SYSTEM);
    }
}

void CGameObject::SetName(const std::string &newName)
{
    if (IsPlayer() && m_Name != newName)
    {
        if (g_GameState >= GS_GAME)
        {
            std::string title = "Ultima Online - " + newName;
            CServer *server = g_ServerList.GetSelectedServer();
            if (server != nullptr)
            {
                title += " (" + server->Name + ")";
            }
            g_GameWindow.SetTitle(title); // FIXME: remove this dependency from here
        }
        PLUGIN_EVENT(UOMSG_SET_PLAYER_NAME, newName.c_str());
    }
    m_Name = newName;
}

void CGameObject::DrawObjectHandlesTexture()
{
    if (m_TextureObjectHandles.Texture == 0)
    {
        if (NPC)
        {
            GenerateObjectHandlesTexture(wstr_from(m_Name));
        }
        else
        {
            auto name = wstr_from(m_Name);
            if (name.length() == 0u)
            {
                name = g_ClilocManager.Cliloc(g_Language)
                           ->GetW(1020000 + Graphic, true, g_Data.m_Static[Graphic].Name);
            }
            GenerateObjectHandlesTexture(name);
        }
    }

    int x = DrawX - g_ObjectHandlesWidthOffset;
    int y = DrawY;
    if (NPC)
    {
        CGameCharacter *gc = (CGameCharacter *)this;
        const auto dims = g_AnimationManager.GetAnimationDimensions(this);
        x += gc->OffsetX;
        y += gc->OffsetY - (gc->OffsetZ + dims.Height + dims.CenterY + s_objectHandleOffsetY);
    }
    else if (IsCorpse())
    {
        x += 20;
        y -= g_Game.GetStaticArtDimension(Graphic).Height + s_bodyHandleOffsetY;
    }
    else
    {
        y -= g_Game.GetStaticArtDimension(Graphic).Height;
    }

    if (g_ConfigManager.ObjectHandlesNoBodies && IsCorpse())
    {
    }
    else
    {
        m_TextureObjectHandles.Draw(x, y);
    }
}

void CGameObject::SelectObjectHandlesTexture()
{
    if (m_TextureObjectHandles.Texture != 0)
    {
        int x = DrawX - g_ObjectHandlesWidthOffset;
        int y = DrawY;

        if (NPC)
        {
            CGameCharacter *gc = (CGameCharacter *)this;
            AnimationFrameInfo dims = g_AnimationManager.GetAnimationDimensions(this);
            x += gc->OffsetX;
            y += gc->OffsetY - (gc->OffsetZ + dims.Height + dims.CenterY + s_objectHandleOffsetY);
        }
        else if (IsCorpse())
        {
            x += 20;
            y -= g_Game.GetStaticArtDimension(Graphic).Height + s_bodyHandleOffsetY;
        }
        else
        {
            y -= g_Game.GetStaticArtDimension(Graphic).Height;
        }

        x = g_MouseManager.Position.X - x;
        y = g_MouseManager.Position.Y - y;

        if (x < 0 || x >= g_ObjectHandlesWidth || y < 0 || y >= g_ObjectHandlesHeight)
        {
            return;
        }

        if (g_ObjectHandlesBackgroundPixels[(y * g_ObjectHandlesWidth) + x] != 0)
        {
            g_SelectedObject.Init(this);
            g_SelectedGameObjectHandle = Serial;
        }
    }
}

void CGameObject::GenerateObjectHandlesTexture(std::wstring text)
{
#ifndef NEW_RENDERER_ENABLED
    if (m_TextureObjectHandles.Texture != 0)
    {
        glDeleteTextures(1, &m_TextureObjectHandles.Texture);
        m_TextureObjectHandles.Texture = 0;
    }
#else
    if (m_TextureObjectHandles.Texture != RENDER_TEXTUREHANDLE_INVALID)
    {
        Render_DestroyTexture(m_TextureObjectHandles.Texture);
        m_TextureObjectHandles.Texture = RENDER_TEXTUREHANDLE_INVALID;
    }
#endif

    int width = g_ObjectHandlesWidth - 20;
    uint8_t font = 1;
    CTextSprite textTexture;
    uint16_t color = 0xFFFF;
    uint8_t cell = 30;
    TEXT_ALIGN_TYPE tat = TS_CENTER;
    uint16_t flags = 0;

    if (g_FontManager.GetWidthW(font, text) > width)
    {
        text = g_FontManager.GetTextByWidthW(font, text, width - 6, true);
    }

    std::vector<uint32_t> textData = g_FontManager.GeneratePixelsW(
        font, textTexture, text.c_str(), color, cell, width, tat, flags);

    if (textData.empty())
    {
        return;
    }

    static const int size = g_ObjectHandlesWidth * g_ObjectHandlesHeight;
    uint16_t pixels[size] = { 0 };

    memcpy(&pixels[0], &g_ObjectHandlesBackgroundPixels[0], size * 2);

    color = 0;
    if (NPC)
    {
        if (IsPlayer())
        {
            color = 0x0386;
        }
        else
        {
            color = g_ConfigManager.GetColorByNotoriety(GameCharacterPtr()->Notoriety);
        }

        if (color != 0u)
        {
            for (int x = 0; x < g_ObjectHandlesWidth; x++)
            {
                for (int y = 0; y < g_ObjectHandlesHeight; y++)
                {
                    uint16_t &pixel = pixels[(y * g_ObjectHandlesWidth) + x];
                    if (pixel != 0u)
                    {
                        const uint8_t r = (pixel & 0x1F);
                        const uint8_t g = ((pixel >> 5) & 0x1F);
                        const uint8_t b = ((pixel >> 10) & 0x1F);
                        if (r == g && r == b)
                        {
                            pixel = g_ColorManager.GetColor16(pixel, color) | 0x8000;
                        }
                    }
                }
            }
        }
    }

    const int maxHeight = textTexture.Height;
    for (int x = 0; x < width; x++)
    {
        const int gumpDataX = (int)x + 10;
        for (int y = 0; y < maxHeight; y++)
        {
            const int gumpDataY = (int)y + 1;
            if (gumpDataY >= g_ObjectHandlesHeight)
            {
                break;
            }

            uint32_t &pixel = textData[(y * textTexture.Width) + x];
            if (pixel != 0u)
            {
                uint8_t *bytes = (uint8_t *)&pixel;
                uint8_t buf = bytes[0];
                bytes[0] = bytes[3];
                bytes[3] = buf;
                buf = bytes[1];
                bytes[1] = bytes[2];
                bytes[2] = buf;
                pixels[(gumpDataY * g_ObjectHandlesWidth) + gumpDataX] =
                    g_ColorManager.Color32To16(pixel) | 0x8000;
            }
        }
    }
#ifndef NEW_RENDERER_ENABLED
    g_GL.BindTexture16(m_TextureObjectHandles, g_ObjectHandlesWidth, g_ObjectHandlesHeight, pixels);
#else
    m_TextureObjectHandles.Width = g_ObjectHandlesWidth;
    m_TextureObjectHandles.Height = g_ObjectHandlesHeight;
    m_TextureObjectHandles.Texture = Render_CreateTexture2D(
        g_ObjectHandlesWidth,
        g_ObjectHandlesHeight,
        TextureGPUFormat::TextureGPUFormat_RGB5_A1,
        pixels,
        TextureFormat::TextureFormat_Unsigned_A1_BGR5);
    assert(m_TextureObjectHandles.Texture != RENDER_TEXTUREHANDLE_INVALID);
#endif
}

void CGameObject::AddText(CTextData *msg)
{
    msg->Owner = this;
    m_TextControl->Add(msg);

    if (Container == 0xFFFFFFFF)
    {
        Changed = true;
    }
    else
    {
        UpdateTextCoordinates();
        FixTextCoordinates();
    }

    /*if (m_Clicked)
	{
		m_Clicked = false;

		if (IsPlayer()) //(NPC)
			msgname = m_Name + ": ";
	}*/

    g_Game.AddJournalMessage(msg, JournalPrefix);
}

uint16_t CGameObject::GetMountAnimation()
{
    return Graphic; // + UO->GetStaticPointer(Graphic)->Increment;
}

void CGameObject::Clear()
{
    if (!Empty())
    {
        CGameObject *obj = (CGameObject *)m_Items;
        while (obj != nullptr)
        {
            CGameObject *next = (CGameObject *)obj->m_Next;
            g_World->RemoveObject(obj);
            obj = next;
        }
        m_Items = nullptr;
    }
}

void CGameObject::ClearUnequipped()
{
    if (!Empty())
    {
        CGameObject *newFirstItem = nullptr;
        CGameObject *obj = (CGameObject *)m_Items;
        while (obj != nullptr)
        {
            CGameObject *next = (CGameObject *)obj->m_Next;
            if (((CGameItem *)obj)->Layer != OL_NONE)
            {
                if (newFirstItem == nullptr)
                {
                    newFirstItem = obj;
                }
            }
            else
            {
                g_World->RemoveObject(obj);
            }
            obj = next;
        }
        m_Items = newFirstItem;
    }
}

void CGameObject::ClearNotOpenedItems()
{
    if (!Empty())
    {
        CGameObject *obj = (CGameObject *)m_Items;
        while (obj != nullptr)
        {
            CGameObject *next = (CGameObject *)obj->m_Next;
            if (!obj->NPC && !((CGameItem *)obj)->Opened &&
                ((CGameItem *)obj)->Layer != OL_BACKPACK)
            {
                g_World->RemoveObject(obj);
            }
            obj = next;
        }
    }
}

bool CGameObject::Poisoned()
{
    if (g_Config.ClientVersion >= CV_7000)
    {
        return SA_Poisoned;
    }
    return (m_Flags & 0x04) != 0;
}

bool CGameObject::Flying()
{
    if (g_Config.ClientVersion >= CV_7000)
    {
        return (m_Flags & 0x04) != 0;
    }
    return false;
}

int CGameObject::IsGold(uint16_t graphic)
{
    switch (graphic)
    {
        case 0x0EED:
            return 1;
        /*case 0x0EEA:
            return 2;*/
        case 0x0EF0:
            return 3;
        default:
            break;
    }
    return 0;
}

uint16_t CGameObject::GetDrawGraphic(bool &doubleDraw)
{
    int index = IsGold(Graphic);
    uint16_t result = Graphic;
    const uint16_t graphicAssociateTable[3][3] = { { 0x0EED, 0x0EEE, 0x0EEF },
                                                   { 0x0EEA, 0x0EEB, 0x0EEC },
                                                   { 0x0EF0, 0x0EF1, 0x0EF2 } };

    if (index != 0)
    {
        int graphicIndex = (int)(Count > 1) + (int)(Count > 5);
        result = graphicAssociateTable[index - 1][graphicIndex];
    }
    else
    {
        doubleDraw = IsStackable() && (Count > 1);
    }
    return result;
}

void CGameObject::DrawEffects(int x, int y)
{
    if (NPC)
    {
        CGameCharacter *gc = GameCharacterPtr();
        x += gc->OffsetX;
        y += gc->OffsetY - (int)gc->OffsetZ - 3;
    }

    QFOR(effect, m_Effects, CGameEffect *)
    {
        effect->ApplyRenderMode();
        if (effect->EffectType == EF_LIGHTING)
        {
            const uint16_t graphic = 0x4E20 + effect->AnimIndex;
            CSize size = g_Game.GetGumpDimension(graphic);
            g_Game.DrawGump(graphic, effect->Color, x - (size.Width / 2), y - size.Height);
        }
        else
        {
            g_Game.DrawStaticArt(effect->GetCurrentGraphic(), effect->Color, x, y);
        }
        effect->RemoveRenderMode();
    }
}

void CGameObject::UpdateEffects()
{
    CGameEffect *effect = m_Effects;
    while (effect != nullptr)
    {
        CGameEffect *next = (CGameEffect *)effect->m_Next;
        effect->Update(this);
        effect = next;
    }
}

void CGameObject::AddEffect(CGameEffect *effect)
{
    if (m_Effects == nullptr)
    {
        m_Effects = effect;
        effect->m_Next = nullptr;
        effect->m_Prev = nullptr;
    }
    else
    {
        effect->m_Next = m_Effects;
        m_Effects->m_Prev = effect;
        effect->m_Prev = nullptr;
        m_Effects = effect;
    }
}

void CGameObject::RemoveEffect(CGameEffect *effect)
{
    if (effect->m_Prev == nullptr)
    {
        m_Effects = (CGameEffect *)effect->m_Next;
        if (m_Effects != nullptr)
        {
            m_Effects->m_Prev = nullptr;
        }
    }
    else
    {
        effect->m_Prev->m_Next = effect->m_Next;
        if (effect->m_Next != nullptr)
        {
            effect->m_Next->m_Prev = effect->m_Prev;
        }
    }

    effect->m_Next = nullptr;
    effect->m_Prev = nullptr;
    delete effect;
}

void CGameObject::AddObject(CGameObject *obj)
{
    g_World->RemoveFromContainer(obj);

    if (m_Next == nullptr)
    {
        m_Next = obj;
        m_Next->m_Prev = this;
        m_Next->m_Next = nullptr;
        ((CGameObject *)m_Next)->Container = Container;
    }
    else
    {
        CGameObject *item = (CGameObject *)m_Next;
        while (item->m_Next != nullptr)
        {
            item = (CGameObject *)item->m_Next;
        }

        item->m_Next = obj;
        obj->m_Next = nullptr;
        obj->m_Prev = item;
        obj->Container = Container;
    }
}

void CGameObject::AddItem(CGameObject *obj)
{
    if (obj->Container != 0xFFFFFFFF)
    {
        return;
    }

    g_World->RemoveFromContainer(obj);
    if (m_Items != nullptr)
    {
        CGameObject *item = (CGameObject *)Last();
        item->m_Next = obj;
        obj->m_Next = nullptr;
        obj->m_Prev = item;
    }
    else
    {
        m_Items = obj;
        m_Items->m_Next = nullptr;
        m_Items->m_Prev = nullptr;
    }
    obj->Container = Serial;
}

void CGameObject::Reject(CGameObject *obj)
{
    if (obj->Container != Serial)
    {
        return;
    }

    if (m_Items != nullptr)
    {
        if (((CGameObject *)m_Items)->Serial == obj->Serial)
        {
            if (m_Items->m_Next != nullptr)
            {
                m_Items = m_Items->m_Next;
                m_Items->m_Prev = nullptr;
            }
            else
            {
                m_Items = nullptr;
            }
        }
        else
        {
            if (obj->m_Next != nullptr)
            {
                if (obj->m_Prev != nullptr)
                {
                    obj->m_Prev->m_Next = obj->m_Next;
                    obj->m_Next->m_Prev = obj->m_Prev;
                }
                else
                { //WTF???
                    obj->m_Next->m_Prev = nullptr;
                }
            }
            else if (obj->m_Prev != nullptr)
            {
                obj->m_Prev->m_Next = nullptr;
            }
        }
    }

    obj->m_Next = nullptr;
    obj->m_Prev = nullptr;
    obj->Container = 0xFFFFFFFF;
}

CGameObject *CGameObject::GetTopObject()
{
    CGameObject *obj = this;
    while (obj->Container != 0xFFFFFFFF)
    {
        obj = g_World->FindWorldObject(obj->Container);
    }
    return obj;
}

CGameItem *CGameObject::FindLayer(int layer)
{
    QFOR(obj, m_Items, CGameItem *)
    {
        if (obj->Layer == layer)
        {
            return obj;
        }
    }
    return nullptr;
}

bool CGameObject::Caller()
{
    if (g_Config.ClientVersion >= CV_7000)
    {
        return pvpCaller;
    }
    return false;
}
