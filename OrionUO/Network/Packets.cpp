// MIT License
// Copyright (C) August 2016 Hotride

#include "../Config.h"

CPacket::CPacket(size_t size, bool autoResize)
    : Wisp::CDataWriter(size, autoResize)
{
}

void CPacket::Send()
{
    if (static_cast<unsigned int>(!m_Data.empty()) != 0u)
    {
        g_Orion.Send(m_Data);
    }
}

CPacketFirstLogin::CPacketFirstLogin()
    : CPacket(62)
{
    WriteUInt8(0x80);

    if (g_Config.TheAbyss)
    {
        m_Data[61] = 0xFF;
    }
    else
    {
        WriteString(g_MainScreen.m_Account->c_str(), 30, false);
        WriteString(g_MainScreen.m_Password->c_str(), 30, false);
        WriteUInt8(0xFF);
    }
}

CPacketSelectServer::CPacketSelectServer(uint8_t index)
    : CPacket(3)
{
    WriteUInt8(0xA0);
    WriteUInt8(0x00);
    WriteUInt8(index);
}

CPacketSecondLogin::CPacketSecondLogin()
    : CPacket(65)
{
    WriteUInt8(0x91);
    WriteDataLE(g_GameSeed, 4);
    WriteString(g_MainScreen.m_Account->c_str(), 30, false);

    int passLen = 30;

    if (g_Config.TheAbyss)
    {
        WriteUInt16BE(0xFF07);
        passLen = 28;
    }

    WriteString(g_MainScreen.m_Password->c_str(), passLen, false);
}

CPacketCreateCharacter::CPacketCreateCharacter(const string &name)
    : CPacket(104)
{
    int skillsCount = 3;
    uint32_t packetID = 0x00;

    if (g_Config.ClientVersion >= CV_70160)
    {
        skillsCount++;
        Resize(106, true);
        packetID = 0xF8;
    }

    WriteUInt8(packetID);
    WriteUInt32BE(0xEDEDEDED);
    WriteUInt16BE(0xFFFF);
    WriteUInt16BE(0xFFFF);
    WriteUInt8(0x00);
    WriteString(name, 30, false);

    //Move(30); //На самом деле, клиент пихает сюда пароль на 30 байт, но по какой-то причине (мб мусор в памяти) - идет то что идет
    WriteUInt16BE(0x0000); //?

    uint32_t clientFlag = 0;
    for (int i = 0; i < g_Config.ClientFlag; i++)
    {
        clientFlag |= (1 << i);
    }

    WriteUInt32BE(clientFlag); //clientflag
    WriteUInt32BE(0x00000001); //?
    WriteUInt32BE(0x00000000); //logincount

    CProfession *profession = (CProfession *)g_ProfessionManager.Selected;
    uint8_t val = (uint8_t)profession->DescriptionIndex;
    WriteUInt8(val); //profession
    Move(15);        //?

    if (g_Config.ClientVersion < CV_4011D)
    {
        val = (uint8_t)g_CreateCharacterManager.GetFemale();
    }
    else
    {
        val = (uint8_t)g_CreateCharacterManager.GetRace();

        if (g_Config.ClientVersion < CV_7000)
        {
            val--;
        }

        val = (val * 2) + (uint8_t)g_CreateCharacterManager.GetFemale();
    }

    WriteUInt8(val);
    val = profession->Str;
    WriteUInt8(val);
    val = profession->Dex;
    WriteUInt8(val);
    val = profession->Int;
    WriteUInt8(val);

    for (int i = 0; i < skillsCount; i++)
    {
        val = profession->GetSkillIndex((int)i);
        if (val == 0xFF)
        {
            //error, skill is not selected
            WriteUInt16BE(0x0000);
        }
        else
        {
            WriteUInt8(val);
            WriteUInt8(profession->GetSkillValue((int)i));
        }
    }

    WriteUInt16BE(g_CreateCharacterManager.SkinTone);
    WriteUInt16BE(g_CreateCharacterManager.GetHair(g_CreateCharacterManager.HairStyle).GraphicID);
    WriteUInt16BE(g_CreateCharacterManager.HairColor);
    WriteUInt16BE(g_CreateCharacterManager.GetBeard(g_CreateCharacterManager.BeardStyle).GraphicID);
    WriteUInt16BE(g_CreateCharacterManager.BeardColor);

    if (g_Config.ClientVersion >= CV_70160)
    {
        uint16_t location = g_SelectTownScreen.m_City->LocationIndex;

        WriteUInt16BE(location); //location
        WriteUInt16BE(0x0000);   //?

        uint16_t slot = 0xFFFF;
        for (int i = 0; i < g_CharacterList.Count; i++)
        {
            if (g_CharacterList.GetName(i).length() == 0u)
            {
                slot = (uint16_t)i;
                break;
            }
        }

        WriteUInt16BE(slot);
    }
    else
    {
        CServer *server = g_ServerList.GetSelectedServer();
        uint8_t serverIndex = 0;

        if (server != nullptr)
        {
            serverIndex = (uint8_t)server->Index;
        }

        WriteUInt8(serverIndex); //server index

        uint8_t location = g_SelectTownScreen.m_City->LocationIndex;
        if (g_Config.ClientVersion < CV_70130)
        {
            location--;
        }
        WriteUInt8(location); //location

        uint32_t slot = 0xFFFFFFFF;
        for (int i = 0; i < g_CharacterList.Count; i++)
        {
            if (g_CharacterList.GetName(i).length() == 0u)
            {
                slot = (uint32_t)i;
                break;
            }
        }

        WriteUInt32BE(slot);
    }
    WriteDataBE(g_ConnectionManager.GetClientIP(), 4);
    WriteUInt16BE(g_CreateCharacterManager.ShirtColor);
    WriteUInt16BE(g_CreateCharacterManager.PantsColor);
}

CPacketDeleteCharacter::CPacketDeleteCharacter(int charIndex)
    : CPacket(39)
{
    WriteUInt8(0x83);
    Move(30); //character password
    WriteUInt32BE(charIndex);
    WriteDataBE(g_ConnectionManager.GetClientIP(), 4);
}

CPacketSelectCharacter::CPacketSelectCharacter(int index, const string &name)
    : CPacket(73)
{
    int copyLen = (int)name.length();

    if (copyLen > 30)
    {
        copyLen = 30;
    }

    memcpy(&g_SelectedCharName[0], name.c_str(), copyLen);

    WriteUInt8(0x5D);
    WriteUInt32BE(0xEDEDEDED);
    WriteString(name, 30, false);
    Move(2);

    uint32_t clientFlag = 0;

    for (int i = 0; i < g_Config.ClientFlag; i++)
    {
        clientFlag |= (1 << i);
    }

    WriteUInt32BE(clientFlag);

    Move(24);
    WriteUInt32BE(index);
    WriteDataBE(g_ConnectionManager.GetClientIP(), 4);
}

CPacketPickupRequest::CPacketPickupRequest(uint32_t serial, uint16_t count)
    : CPacket(7)
{
    WriteUInt8(0x07);
    WriteUInt32BE(serial);
    WriteUInt16BE(count);
}

CPacketDropRequestOld::CPacketDropRequestOld(
    uint32_t serial, uint16_t x, uint16_t y, char z, uint32_t container)
    : CPacket(14)
{
    WriteUInt8(0x08);
    WriteUInt32BE(serial);
    WriteUInt16BE(x);
    WriteUInt16BE(y);
    WriteUInt8(z);
    WriteUInt32BE(container);
}

CPacketDropRequestNew::CPacketDropRequestNew(
    uint32_t serial, uint16_t x, uint16_t y, char z, uint8_t slot, uint32_t container)
    : CPacket(15)
{
    WriteUInt8(0x08);
    WriteUInt32BE(serial);
    WriteUInt16BE(x);
    WriteUInt16BE(y);
    WriteUInt8(z);
    WriteUInt8(slot);
    WriteUInt32BE(container);
}

CPacketEquipRequest::CPacketEquipRequest(uint32_t serial, uint8_t layer, uint32_t container)
    : CPacket(10)
{
    WriteUInt8(0x13);
    WriteUInt32BE(serial);
    WriteUInt8(layer);
    WriteUInt32BE(container);
}

CPacketChangeWarmode::CPacketChangeWarmode(uint8_t state)
    : CPacket(5)
{
    WriteUInt8(0x72);
    WriteUInt8(state);
    WriteUInt16BE(0x0032);
}

CPacketHelpRequest::CPacketHelpRequest()
    : CPacket(258)
{
    WriteUInt8(0x9B);
}

CPacketStatusRequest::CPacketStatusRequest(uint32_t serial)
    : CPacket(10)
{
    WriteUInt8(0x34);
    WriteUInt32BE(0xEDEDEDED);
    WriteUInt8(4);
    WriteUInt32BE(serial);
}

CPacketSkillsRequest::CPacketSkillsRequest(uint32_t serial)
    : CPacket(10)
{
    WriteUInt8(0x34);
    WriteUInt32BE(0xEDEDEDED);
    WriteUInt8(5);
    WriteUInt32BE(serial);
}

CPacketSkillsStatusChangeRequest::CPacketSkillsStatusChangeRequest(uint8_t skill, uint8_t state)
    : CPacket(6)
{
    WriteUInt8(0x3A);
    WriteUInt16BE(0x0006);
    WriteUInt16BE((uint16_t)skill);
    WriteUInt8(state);
}

CPacketClickRequest::CPacketClickRequest(uint32_t serial)
    : CPacket(5)
{
    WriteUInt8(0x09);
    WriteUInt32BE(serial);
}

CPacketDoubleClickRequest::CPacketDoubleClickRequest(uint32_t serial)
    : CPacket(5)
{
    WriteUInt8(0x06);
    WriteUInt32BE(serial);
}

CPacketAttackRequest::CPacketAttackRequest(uint32_t serial)
    : CPacket(5)
{
    WriteUInt8(0x05);
    WriteUInt32BE(serial);
}

CPacketClientVersion::CPacketClientVersion(const string &version)
    : CPacket(4 + version.length())
{
    WriteUInt8(0xBD);
    WriteUInt16BE(4 + (uint16_t)version.length());
    WriteString(version, (int)version.length(), false);
}

CPacketASCIISpeechRequest::CPacketASCIISpeechRequest(
    const char *text, SPEECH_TYPE type, uint16_t font, uint16_t color)
    : CPacket(1)
{
    size_t len = strlen(text);
    size_t size = 8 + len + 1;
    Resize(size, true);

    WriteUInt8(0x03);
    WriteUInt16BE((uint16_t)size);
    WriteUInt8(type);
    WriteUInt16BE(color);
    WriteUInt16BE(font);
    WriteString(text, len, false);
}

CPacketUnicodeSpeechRequest::CPacketUnicodeSpeechRequest(
    const wchar_t *text, SPEECH_TYPE type, uint16_t font, uint16_t color, uint8_t *language)
    : CPacket(1)
{
    size_t len = lstrlenW(text);
    size_t size = 12;

    uint8_t typeValue = (uint8_t)type;

    vector<uint32_t> codes;
    g_SpeechManager.GetKeywords(text, codes);

    //encoded
    bool encoded = !codes.empty();
    string utf8string{};
    vector<uint8_t> codeBytes;
    if (encoded)
    {
        typeValue |= ST_ENCODED_COMMAND;
        utf8string = EncodeUTF8(wstring(text));
        len = (int)utf8string.length();
        size += len;
        size += 1; //null terminator

        int length = (int)codes.size();
        codeBytes.push_back(length >> 4);
        int num3 = length & 15;
        bool flag = false;
        int index = 0;

        while (index < length)
        {
            int keywordID = codes[index];

            if (flag)
            {
                codeBytes.push_back(keywordID >> 4);
                num3 = keywordID & 15;
            }
            else
            {
                codeBytes.push_back(((num3 << 4) | ((keywordID >> 8) & 15)));
                codeBytes.push_back(keywordID);
            }

            index++;
            flag = !flag;
        }

        if (!flag)
        {
            codeBytes.push_back(num3 << 4);
        }
        size += codeBytes.size();
    }
    else
    {
        size += len * 2;
        size += 2; //null terminator
    }

    Resize(size, true);

    WriteUInt8(0xAD);
    WriteUInt16BE((uint16_t)size);
    WriteUInt8(typeValue);
    WriteUInt16BE(color);
    WriteUInt16BE(font);
    WriteDataLE(language, 4);

    //Sallos aka PlayUO algorithm
    if (encoded)
    {
        for (int i = 0; i < (int)codeBytes.size(); i++)
        {
            WriteUInt8(codeBytes[i]);
        }
        WriteString(utf8string, len, true);
        // в данном случае надо посылать как utf8, так читает сервер.
    }
    else
    {
        WriteWString(text, len, true, true);
    }
}

CPacketCastSpell::CPacketCastSpell(int index)
    : CPacket(1)
{
    if (g_Config.ClientVersion >= CV_60142)
    {
        Resize(9, true);

        WriteUInt8(0xBF);
        WriteUInt16BE(0x0009);
        WriteUInt16BE(0x001C);
        WriteUInt16BE(0x0002);
        WriteUInt16BE(index);
    }
    else
    {
        char spell[10] = { 0 };
        sprintf_s(spell, "%i", index);

        size_t len = strlen(spell);
        size_t size = 5 + len;
        Resize(size, true);

        WriteUInt8(0x12);
        WriteUInt16BE((uint16_t)size);
        WriteUInt8(0x56);
        WriteString(spell, len, false);
    }
}

CPacketCastSpellFromBook::CPacketCastSpellFromBook(int index, uint32_t serial)
    : CPacket(1)
{
    char spell[25] = { 0 };
    sprintf_s(spell, "%i %d", index, (int)serial);

    size_t len = strlen(spell);
    size_t size = 5 + len;
    Resize(size, true);

    WriteUInt8(0x12);
    WriteUInt16BE((uint16_t)size);
    WriteUInt8(0x27);
    WriteString(spell, len, false);
}

CPacketUseSkill::CPacketUseSkill(int index)
    : CPacket(10)
{
    char skill[10] = { 0 };
    sprintf_s(skill, "%d 0", index);

    size_t len = strlen(skill);
    size_t size = 5 + len;
    Resize(size, true);

    WriteUInt8(0x12);
    WriteUInt16BE((uint16_t)size);
    WriteUInt8(0x24);
    WriteString(skill, len, false);
}

CPacketOpenDoor::CPacketOpenDoor()
    : CPacket(5)
{
    WriteUInt8(0x12);
    WriteUInt16BE(0x0005);
    WriteUInt8(0x58);
}

CPacketOpenSpellbook::CPacketOpenSpellbook(SPELLBOOK_TYPE type)
    : CPacket(6)
{
    WriteUInt8(0x12);
    WriteUInt16BE(0x0006);
    WriteUInt8(0x43);
    WriteUInt8(type + 30);
}

CPacketEmoteAction::CPacketEmoteAction(const char *action)
    : CPacket(1)
{
    size_t len = strlen(action);
    size_t size = 5 + len;
    Resize(size, true);

    WriteUInt8(0x12);
    WriteUInt16BE((uint16_t)size);
    WriteUInt8(0xC7);
    WriteString(action, len, false);
}

CPacketGumpResponse::CPacketGumpResponse(CGumpGeneric *gump, int code)
    : CPacket(1)
{
    int switchesCount = 0;
    int textLinesCount = 0;
    int textLinesLength = 0;

    QFOR(item, gump->m_Items, CBaseGUI *)
    {
        switch (item->Type)
        {
            case GOT_CHECKBOX:
            case GOT_RADIO:
            {
                if (((CGUICheckbox *)item)->Checked)
                {
                    switchesCount++;
                }

                break;
            }
            case GOT_TEXTENTRY:
            {
                CGUITextEntry *gte = (CGUITextEntry *)item;

                textLinesCount++;
                textLinesLength += ((int)gte->m_Entry.Length() * 2);

                break;
            }
            default:
                break;
        }
    }

    size_t size = 19 + (switchesCount * 4) + 4 + ((textLinesCount * 4) + textLinesLength);
    Resize(size, true);

    g_PacketManager.SetCachedGumpCoords(gump->ID, gump->GetX(), gump->GetY());

    WriteUInt8(0xB1);
    WriteUInt16BE((uint16_t)size);
    WriteUInt32BE(gump->Serial);
    WriteUInt32BE(gump->MasterGump != 0u ? gump->MasterGump : gump->ID);
    WriteUInt32BE(code);
    WriteUInt32BE(switchesCount);

    QFOR(item, gump->m_Items, CBaseGUI *)
    {
        if (item->Type == GOT_CHECKBOX || item->Type == GOT_RADIO)
        {
            if (((CGUICheckbox *)item)->Checked)
            {
                WriteUInt32BE(((CGUICheckbox *)item)->Serial);
            }
        }
    }

    WriteUInt32BE(textLinesCount);

    QFOR(item, gump->m_Items, CBaseGUI *)
    {
        if (item->Type == GOT_TEXTENTRY)
        {
            CGUITextEntry *entry = (CGUITextEntry *)item;

            WriteUInt16BE(entry->Serial - 1);
            size_t len =
                (((entry->m_Entry.Length()) < (MAX_TEXTENTRY_LENGTH)) ? (entry->m_Entry.Length()) :
                                                                        (MAX_TEXTENTRY_LENGTH));
            WriteUInt16BE((uint16_t)len);
            WriteWString(entry->m_Entry.Data(), len, true, false);
        }
    }
}

CPacketVirtureGumpResponse::CPacketVirtureGumpResponse(CGump *gump, int code)
    : CPacket(15)
{
    g_PacketManager.SetCachedGumpCoords(gump->ID, gump->GetX(), gump->GetY());

    WriteUInt8(0xB1);
    WriteUInt16BE(0x000F);
    WriteUInt32BE(gump->Serial);
    WriteUInt32BE(0x000001CD);
    WriteUInt32BE(code);
}

CPacketMenuResponse::CPacketMenuResponse(CGump *gump, int code)
    : CPacket(13)
{
    WriteUInt8(0x7D);
    WriteUInt32BE(gump->Serial);
    WriteUInt16BE(gump->ID);

    if (code != 0)
    {
        WriteUInt16BE(code);

        QFOR(item, gump->m_Items, CBaseGUI *)
        {
            if (item->Serial == code && item->Type == GOT_MENUOBJECT)
            {
                WriteUInt16BE(item->Graphic);
                WriteUInt16BE(item->Color);

                break;
            }
        }
    }
}

CPacketGrayMenuResponse::CPacketGrayMenuResponse(CGump *gump, int code)
    : CPacket(13)
{
    WriteUInt8(0x7D);
    WriteUInt32BE(gump->Serial);
    WriteUInt16BE(gump->ID);
    WriteUInt16BE(code);
}

CPacketTradeResponse::CPacketTradeResponse(CGumpSecureTrading *gump, int code)
    : CPacket(17)
{
    WriteUInt8(0x6F);
    WriteUInt16BE(17);

    if (code == 1) //Закрываем окно
    {
        WriteUInt8(0x01);
        WriteUInt32BE(gump->ID);
    }
    else if (code == 2) //Ткнули на чекбокс
    {
        WriteUInt8(0x02);
        WriteUInt32BE(gump->ID);
        WriteUInt32BE(static_cast<int>(gump->StateMy));
    }
}

CPacketLogoutNotification::CPacketLogoutNotification()
    : CPacket(5)
{
    WriteUInt8(0x01);
    WriteUInt32BE(0xFFFFFFFF);
}

CPacketTextEntryDialogResponse::CPacketTextEntryDialogResponse(
    CGumpTextEntryDialog *gump, CEntryText *entry, bool code)
    : CPacket(1)
{
    size_t len = entry->Length();
    size_t size = 12 + len + 1;
    Resize(size, true);

    WriteUInt8(0xAC);
    WriteUInt16BE((uint16_t)size);
    WriteUInt32BE(gump->Serial);
    WriteUInt8(gump->ButtonID);
    WriteUInt8(0);
    WriteUInt8(code ? 0x01 : 0x00);

    WriteUInt16BE((uint16_t)len + 1);

    WriteString(entry->c_str(), len);
}

CPacketRenameRequest::CPacketRenameRequest(uint32_t serial, const string &newName)
    : CPacket(35)
{
    WriteUInt8(0x75);
    WriteUInt32BE(serial);
    WriteString(newName, newName.length(), false);
}

CPacketTipRequest::CPacketTipRequest(uint16_t id, uint8_t flag)
    : CPacket(4)
{
    WriteUInt8(0xA7);
    WriteUInt16BE(id);
    WriteUInt8(flag);
}

CPacketASCIIPromptResponse::CPacketASCIIPromptResponse(const char *text, size_t len, bool cancel)
    : CPacket(1)
{
    size_t size = 15 + len + 1;
    Resize(size, true);

    WriteDataLE(g_LastASCIIPrompt, 11);
    pack16(&m_Data[0] + 1, (uint16_t)size);
    WriteUInt32BE((uint32_t)((bool)!cancel));

    WriteString(text, len);
}

CPacketUnicodePromptResponse::CPacketUnicodePromptResponse(
    const wchar_t *text, size_t len, const string &lang, bool cancel)
    : CPacket(1)
{
    size_t size = 19 + (len * 2);
    Resize(size, true);

    WriteDataLE(g_LastUnicodePrompt, 11);
    pack16(&m_Data[0] + 1, (uint16_t)size);
    WriteUInt32BE((uint32_t)((bool)!cancel));
    WriteString(lang, 4, false);

    WriteWString(text, len, false, false);
}

CPacketDyeDataResponse::CPacketDyeDataResponse(uint32_t serial, uint16_t graphic, uint16_t color)
    : CPacket(9)
{
    WriteUInt8(0x95);
    WriteUInt32BE(serial);
    WriteUInt16BE(graphic);
    WriteUInt16BE(color);
}

CPacketProfileRequest::CPacketProfileRequest(uint32_t serial)
    : CPacket(8)
{
    WriteUInt8(0xB8);
    WriteUInt16BE(0x0008);
    WriteUInt8(0);
    WriteUInt32BE(serial);
}

CPacketProfileUpdate::CPacketProfileUpdate(uint32_t serial, const wchar_t *text, size_t len)
    : CPacket(1)
{
    size_t size = 12 + (len * 2);
    Resize(size, true);

    WriteUInt8(0xB8);
    WriteUInt16BE((uint16_t)size);
    WriteUInt8(1);
    WriteUInt32BE(serial);
    WriteUInt16BE(0x0001);
    WriteUInt16BE((uint16_t)len);
    WriteWString(text, len, true, false);
}

CPacketCloseStatusbarGump::CPacketCloseStatusbarGump(uint32_t serial)
    : CPacket(9)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x0009);
    WriteUInt16BE(0x000C);
    WriteUInt32BE(serial);
}

CPacketPartyInviteRequest::CPacketPartyInviteRequest()
    : CPacket(10)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x000a);
    WriteUInt16BE(0x0006);
    WriteUInt8(0x01);
    WriteUInt32BE(0x00000000);
}

CPacketPartyRemoveRequest::CPacketPartyRemoveRequest(uint32_t serial)
    : CPacket(10)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x000a);
    WriteUInt16BE(0x0006);
    WriteUInt8(0x02);
    WriteUInt32BE(serial);
}

CPacketPartyChangeLootTypeRequest::CPacketPartyChangeLootTypeRequest(uint8_t type)
    : CPacket(7)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x0007);
    WriteUInt16BE(0x0006);
    WriteUInt8(0x06);
    WriteUInt8(type);
}

CPacketPartyAccept::CPacketPartyAccept(uint32_t serial)
    : CPacket(10)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x000a);
    WriteUInt16BE(0x0006);
    WriteUInt8(0x08);
    WriteUInt32BE(serial);
}

CPacketPartyDecline::CPacketPartyDecline(uint32_t serial)
    : CPacket(10)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x000a);
    WriteUInt16BE(0x0006);
    WriteUInt8(0x09);
    WriteUInt32BE(serial);
}

CPacketPartyMessage::CPacketPartyMessage(const wchar_t *text, size_t len, uint32_t serial)
    : CPacket(1)
{
    size_t size = 10 + (len * 2) + 2;
    Resize(size, true);

    WriteUInt8(0xBF);
    WriteUInt16BE((uint16_t)size);
    WriteUInt16BE(0x0006);

    if (serial != 0u) //Private message to member
    {
        WriteUInt8(0x03);
        WriteUInt32BE(serial);
    }
    else
    { //Message to full party
        WriteUInt8(0x04);
    }

    WriteWString(text, len, true, false);
}

CPacketGameWindowSize::CPacketGameWindowSize()
    : CPacket(13)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x000D);
    WriteUInt16BE(0x0005);
    WriteUInt32BE(g_ConfigManager.GameWindowWidth);
    WriteUInt32BE(g_ConfigManager.GameWindowHeight);
}

CPacketClientViewRange::CPacketClientViewRange(uint8_t range)
    : CPacket(2)
{
    WriteUInt8(0xC8);

    if (range < MIN_VIEW_RANGE)
    {
        range = MIN_VIEW_RANGE;
    }
    else if (range > g_MaxViewRange)
    {
        range = g_MaxViewRange;
    }

    WriteUInt8(range);
}

CPacketBulletinBoardRequestMessage::CPacketBulletinBoardRequestMessage(
    uint32_t serial, uint32_t msgSerial)
    : CPacket(12)
{
    WriteUInt8(0x71);
    WriteUInt16BE(0x000C);
    WriteUInt8(0x03);
    WriteUInt32BE(serial);
    WriteUInt32BE(msgSerial);
}

CPacketBulletinBoardRequestMessageSummary::CPacketBulletinBoardRequestMessageSummary(
    uint32_t serial, uint32_t msgSerial)
    : CPacket(12)
{
    WriteUInt8(0x71);
    WriteUInt16BE(0x000C);
    WriteUInt8(0x04);
    WriteUInt32BE(serial);
    WriteUInt32BE(msgSerial);
}

CPacketBulletinBoardPostMessage::CPacketBulletinBoardPostMessage(
    uint32_t serial, uint32_t replySerial, const char *subject, const char *message)
    : CPacket(1)
{
    size_t subjectLen = strlen(subject);
    size_t size = 14 + subjectLen + 1;

    int lines = 1;

    const auto msgLen = (int)strlen(message);
    int len = 0;

    for (int i = 0; i < msgLen; i++)
    {
        if (message[i] == '\n')
        {
            len++;

            size += len + 1;
            len = 0;
            lines++;
        }
        else
        {
            len++;
        }
    }

    size += len + 2;

    Resize(size, true);

    WriteUInt8(0x71);
    WriteUInt16BE((uint16_t)size);
    WriteUInt8(0x05);
    WriteUInt32BE(serial);
    WriteUInt32BE(replySerial);

    WriteUInt8((uint8_t)subjectLen + 1);
    WriteString(subject, subjectLen, false);
    WriteUInt8(0);

    WriteUInt8(lines);
    len = 0;
    char *msgPtr = (char *)message;

    for (int i = 0; i < msgLen; i++)
    {
        if (msgPtr[len] == '\n')
        {
            len++;

            WriteUInt8(len);

            if (len > 1)
            {
                WriteString(msgPtr, len - 1, false);
            }

            WriteUInt8(0);

            msgPtr += len;
            len = 0;
        }
        else
        {
            len++;
        }
    }

    WriteUInt8(len + 1);
    WriteString(msgPtr, len, false);
    WriteUInt8(0);
}

CPacketBulletinBoardRemoveMessage::CPacketBulletinBoardRemoveMessage(
    uint32_t serial, uint32_t msgSerial)
    : CPacket(12)
{
    WriteUInt8(0x71);
    WriteUInt16BE(0x000C);
    WriteUInt8(0x06);
    WriteUInt32BE(serial);
    WriteUInt32BE(msgSerial);
}

CPacketAssistVersion::CPacketAssistVersion(uint32_t version, const string &clientVersion)
    : CPacket(1)
{
    size_t size = 7 + clientVersion.length() + 1;
    Resize(size, true);

    WriteUInt8(0xBE);
    WriteUInt16BE((uint16_t)size);
    WriteUInt32BE(version);
    WriteString(clientVersion, clientVersion.length());
}

CPacketRazorAnswer::CPacketRazorAnswer()
    : CPacket(4)
{
    WriteUInt8(0xF0);
    WriteUInt16BE(0x0004);
    WriteUInt8(0xFF);
}

CPacketLanguage::CPacketLanguage(const string &lang)
    : CPacket(1)
{
    size_t size = 5 + lang.length() + 1;
    Resize(size, true);

    WriteUInt8(0xBF);
    WriteUInt16BE(0x0009);
    WriteUInt16BE(0x000B);
    WriteString(lang, lang.length(), false);
}

CPacketClientType::CPacketClientType()
    : CPacket(10)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x000A);
    WriteUInt16BE(0x000F);
    WriteUInt8(0x0A);

    uint32_t clientFlag = 0;

    for (int i = 0; i < g_Config.ClientFlag; i++)
    {
        clientFlag |= (1 << i);
    }

    WriteUInt32BE(clientFlag);
}

CPacketRequestPopupMenu::CPacketRequestPopupMenu(uint32_t serial)
    : CPacket(9)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x0009);
    WriteUInt16BE(0x0013);
    WriteUInt32BE(serial);
}

CPacketPopupMenuSelection::CPacketPopupMenuSelection(uint32_t serial, uint16_t menuID)
    : CPacket(11)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x000B);
    WriteUInt16BE(0x0015);
    WriteUInt32BE(serial);
    WriteUInt16BE(menuID);
}

CPacketOpenChat::CPacketOpenChat(const wstring &name)
    : CPacket(64)
{
    WriteUInt8(0xB5);

    size_t len = name.length();

    if (len > 0)
    {
        if (len > 30)
        {
            len = 30;
        }

        WriteWString(name, len, false, false);
    }
}

CPacketMapMessage::CPacketMapMessage(
    uint32_t serial, MAP_MESSAGE action, uint8_t pin, short x, short y)
    : CPacket(11)
{
    WriteUInt8(0x56);
    WriteUInt32BE(serial);
    WriteUInt8(action);
    WriteUInt8(pin);
    WriteUInt16BE(x);
    WriteUInt16BE(y);
}

CPacketGuildMenuRequest::CPacketGuildMenuRequest()
    : CPacket(10)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x000A);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x0028);
    WriteUInt8(0x0A);
}

CPacketQuestMenuRequest::CPacketQuestMenuRequest()
    : CPacket(10)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x000A);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x0032);
    WriteUInt8(0x0A);
}

CPacketEquipLastWeapon::CPacketEquipLastWeapon()
    : CPacket(10)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x000A);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x001E);
    WriteUInt8(0x0A);
}

CPacketVirtureRequest::CPacketVirtureRequest(int buttonID)
    : CPacket(23)
{
    WriteUInt8(0xB1);
    WriteUInt16BE(0x0017);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt32BE(0x000001CD);
    WriteUInt32BE(buttonID);
    WriteUInt32BE(0x00000001);
    WriteUInt32BE(g_PlayerSerial);
}

CPacketInvokeVirtureRequest::CPacketInvokeVirtureRequest(uint8_t id)
    : CPacket(6)
{
    WriteUInt8(0x12);
    WriteUInt16BE(0x0006);
    WriteUInt8(0xF4);
    WriteUInt8(id);
    WriteUInt8(0x00);
}

CPacketMegaClilocRequestOld::CPacketMegaClilocRequestOld(int serial)
    : CPacket(9)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(9); //size
    WriteUInt16BE(0x0010);
    WriteUInt32BE(serial);
}

CPacketMegaClilocRequest::CPacketMegaClilocRequest(vector<uint32_t> &list)
    : CPacket(1)
{
    size_t len = list.size();

    if (len > 50)
    {
        len = 50;
    }

    size_t size = 3 + (len * 4);
    Resize(size, true);

    WriteUInt8(0xD6);
    WriteUInt16BE((uint16_t)size);

    for (int i = 0; i < (int)len; i++)
    {
        WriteUInt32BE(list[i]);
    }

    if ((int)list.size() > 50)
    {
        list.erase(list.begin(), list.begin() + 50);
    }
    else
    {
        list.clear();
    }
}

CPacketChangeStatLockStateRequest::CPacketChangeStatLockStateRequest(uint8_t stat, uint8_t state)
    : CPacket(7)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x0007);
    WriteUInt16BE(0x001A);
    WriteUInt8(stat);
    WriteUInt8(state);
}

CPacketBookHeaderChangeOld::CPacketBookHeaderChangeOld(CGumpBook *gump)
    : CPacket(99)
{
    string title = EncodeUTF8(gump->m_EntryTitle->m_Entry.Data());
    string author = EncodeUTF8(gump->m_EntryAuthor->m_Entry.Data());

    WriteUInt8(0xD4);
    WriteUInt32BE(gump->Serial);
    WriteUInt16BE((uint16_t)0x0000); //flags
    WriteUInt16BE((uint16_t)gump->PageCount);
    WriteString(gump->m_EntryTitle->m_Entry.GetTextA(), 60);
    WriteString(gump->m_EntryAuthor->m_Entry.GetTextA(), 30);
}

CPacketBookHeaderChange::CPacketBookHeaderChange(CGumpBook *gump)
    : CPacket(1)
{
    string title = EncodeUTF8(gump->m_EntryTitle->m_Entry.Data());
    string author = EncodeUTF8(gump->m_EntryAuthor->m_Entry.Data());
    auto titlelen = (uint16_t)title.length();
    auto authorlen = (uint16_t)author.length();
    size_t size = 16 + title.length() + author.length();
    Resize(size, true);

    WriteUInt8(0xD4);
    WriteUInt16BE((uint16_t)size);
    WriteUInt32BE(gump->Serial);
    WriteUInt16BE((uint16_t)0x0000); //flags
    WriteUInt16BE((uint16_t)gump->PageCount);
    WriteUInt16BE(titlelen);
    if (titlelen != 0u)
    {
        const char *str = title.c_str();

        for (int i = 0; i < titlelen; i++)
        {
            char ch = *(str + i);
            *Ptr++ = ch;
        }
        *Ptr = 0;
    }
    WriteUInt16BE(authorlen);
    if (authorlen != 0u)
    {
        const char *str = author.c_str();

        for (int i = 0; i < authorlen; i++)
        {
            char ch = *(str + i);
            *Ptr++ = ch;
        }
        *Ptr = 0;
    }
}

CPacketBookPageData::CPacketBookPageData(CGumpBook *gump, int page)
    : CPacket(1)
{
    int lineCount = 0;

    CGUITextEntry *entry = gump->GetEntry(page);

    if (entry != nullptr)
    {
        CEntryText &textEntry = entry->m_Entry;
        string data = EncodeUTF8(textEntry.Data());
        size_t len = data.length();
        size_t size = 9 + 4 + 1;

        if (len != 0u)
        {
            size += len;
            const char *str = data.c_str();

            for (int i = 0; i < (int)len; i++)
            {
                if (*(str + i) == '\n')
                {
                    lineCount++;
                }
            }

            if (str[len - 1] != '\n')
            {
                lineCount++;
            }
        }

        Resize(size, true);

        WriteUInt8(0x66);
        WriteUInt16BE((uint16_t)size);
        WriteUInt32BE(gump->Serial);
        WriteUInt16BE(0x0001);

        WriteUInt16BE(page);
        WriteUInt16BE(lineCount);

        if (len != 0u)
        {
            const char *str = data.c_str();

            for (int i = 0; i < (int)len; i++)
            {
                char ch = *(str + i);

                if (ch == '\n')
                {
                    ch = 0;
                }

                *Ptr++ = ch;
            }

            *Ptr = 0;
        }
    }
}

CPacketBookPageDataRequest::CPacketBookPageDataRequest(int serial, int page)
    : CPacket(13)
{
    WriteUInt8(0x66);
    WriteUInt16BE(0x000D);
    WriteUInt32BE(serial);
    WriteUInt16BE(0x0001);
    WriteUInt16BE(page);
    WriteUInt16BE(0xFFFF);
}

CPacketBuyRequest::CPacketBuyRequest(CGumpShop *gump)
    : CPacket(1)
{
    size_t size = 8;
    int count = 0;

    QFOR(item, gump->m_ItemList[1]->m_Items, CBaseGUI *)
    {
        if (item->Type == GOT_SHOPRESULT)
        {
            size += 7;
            count++;
        }
    }

    Resize(size, true);

    WriteUInt8(0x3B);
    WriteUInt16BE((uint16_t)size);
    WriteUInt32BE(gump->Serial);

    if (count != 0)
    {
        WriteUInt8(0x02);

        QFOR(item, gump->m_ItemList[1]->m_Items, CBaseGUI *)
        {
            if (item->Type == GOT_SHOPRESULT)
            {
                WriteUInt8(0x1A);
                WriteUInt32BE(item->Serial);
                WriteUInt16BE(((CGUIShopResult *)item)->m_MinMaxButtons->Value);
            }
        }
    }
    else
    {
        WriteUInt8(0x00);
    }
}

CPacketSellRequest::CPacketSellRequest(CGumpShop *gump)
    : CPacket(1)
{
    size_t size = 9;
    int count = 0;

    QFOR(item, gump->m_ItemList[1]->m_Items, CBaseGUI *)
    {
        if (item->Type == GOT_SHOPRESULT)
        {
            size += 6;
            count++;
        }
    }

    Resize(size, true);

    WriteUInt8(0x9F);
    WriteUInt16BE((uint16_t)size);
    WriteUInt32BE(gump->Serial);
    WriteUInt16BE(count);

    QFOR(item, gump->m_ItemList[1]->m_Items, CBaseGUI *)
    {
        if (item->Type == GOT_SHOPRESULT)
        {
            WriteUInt32BE(item->Serial);
            WriteUInt16BE(((CGUIShopResult *)item)->m_MinMaxButtons->Value);
        }
    }
}

CPacketUseCombatAbility::CPacketUseCombatAbility(uint8_t index)
    : CPacket(15)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x000F);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x0019);
    WriteUInt32BE(0x00000000);
    WriteUInt8(index);
    WriteUInt8(0x0A);
}

CPacketTargetSelectedObject::CPacketTargetSelectedObject(
    int useObjectSerial, int targetObjectSerial)
    : CPacket(13)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x000D);
    WriteUInt16BE(0x002C);
    WriteUInt32BE(useObjectSerial);
    WriteUInt32BE(targetObjectSerial);
}

CPacketToggleGargoyleFlying::CPacketToggleGargoyleFlying()
    : CPacket(11)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x000B);
    WriteUInt16BE(0x0032);
    WriteUInt16BE(0x0001);
    WriteUInt32BE(0x00000000);
}

CPacketCustomHouseDataReq::CPacketCustomHouseDataReq(int serial)
    : CPacket(9)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x09);
    WriteUInt16BE(0x1E);
    WriteUInt32BE(serial);
}

CPacketStunReq::CPacketStunReq()
    : CPacket(5)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x05);
    WriteUInt16BE(0x09);
}

CPacketDisarmReq::CPacketDisarmReq()
    : CPacket(5)
{
    WriteUInt8(0xBF);
    WriteUInt16BE(0x05);
    WriteUInt16BE(0x0A);
}

CPacketResend::CPacketResend()
    : CPacket(3)
{
    WriteUInt8(0x22);
}

CPacketWalkRequest::CPacketWalkRequest(uint8_t direction, uint8_t sequence, int fastWalkKey)
    : CPacket(7)
{
    WriteUInt8(0x02);
    WriteUInt8(direction);
    WriteUInt8(sequence);
    WriteUInt32BE(fastWalkKey);
}

CPacketCustomHouseBackup::CPacketCustomHouseBackup()
    : CPacket(10)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x000A);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x0002);
    WriteUInt8(0x0A);
}

CPacketCustomHouseRestore::CPacketCustomHouseRestore()
    : CPacket(10)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x000A);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x0003);
    WriteUInt8(0x0A);
}

CPacketCustomHouseCommit::CPacketCustomHouseCommit()
    : CPacket(10)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x000A);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x0004);
    WriteUInt8(0x0A);
}

CPacketCustomHouseBuildingExit::CPacketCustomHouseBuildingExit()
    : CPacket(10)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x000A);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x000C);
    WriteUInt8(0x0A);
}

CPacketCustomHouseGoToFloor::CPacketCustomHouseGoToFloor(uint8_t floor)
    : CPacket(15)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x000F);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x0012);
    WriteUInt32BE(0);
    WriteUInt8(floor);
    WriteUInt8(0x0A);
}

CPacketCustomHouseSync::CPacketCustomHouseSync()
    : CPacket(10)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x000A);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x000E);
    WriteUInt8(0x0A);
}

CPacketCustomHouseClear::CPacketCustomHouseClear()
    : CPacket(10)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x000A);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x0010);
    WriteUInt8(0x0A);
}

CPacketCustomHouseRevert::CPacketCustomHouseRevert()
    : CPacket(10)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x000A);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x001A);
    WriteUInt8(0x0A);
}

CPacketCustomHouseResponse::CPacketCustomHouseResponse()
    : CPacket(10)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x000A);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x000A);
    WriteUInt8(0x0A);
}

CPacketCustomHouseAddItem::CPacketCustomHouseAddItem(uint16_t graphic, int x, int y)
    : CPacket(25)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x0019);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x0006);
    WriteUInt8(0x00);
    WriteUInt32BE(graphic);
    WriteUInt8(0x00);
    WriteUInt32BE(x);
    WriteUInt8(0x00);
    WriteUInt32BE(y);
    WriteUInt8(0x0A);
}

CPacketCustomHouseDeleteItem::CPacketCustomHouseDeleteItem(uint16_t graphic, int x, int y, int z)
    : CPacket(30)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x001E);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x0005);
    WriteUInt8(0x00);
    WriteUInt32BE(graphic);
    WriteUInt8(0x00);
    WriteUInt32BE(x);
    WriteUInt8(0x00);
    WriteUInt32BE(y);
    WriteUInt8(0x00);
    WriteUInt32BE(z);
    WriteUInt8(0x0A);
}

CPacketCustomHouseAddRoof::CPacketCustomHouseAddRoof(uint16_t graphic, int x, int y, int z)
    : CPacket(30)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x001E);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x0013);
    WriteUInt8(0x00);
    WriteUInt32BE(graphic);
    WriteUInt8(0x00);
    WriteUInt32BE(x);
    WriteUInt8(0x00);
    WriteUInt32BE(y);
    WriteUInt8(0x00);
    WriteUInt32BE(z);
    WriteUInt8(0x0A);
}

CPacketCustomHouseDeleteRoof::CPacketCustomHouseDeleteRoof(uint16_t graphic, int x, int y, int z)
    : CPacket(30)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x001E);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x0014);
    WriteUInt8(0x00);
    WriteUInt32BE(graphic);
    WriteUInt8(0x00);
    WriteUInt32BE(x);
    WriteUInt8(0x00);
    WriteUInt32BE(y);
    WriteUInt8(0x00);
    WriteUInt32BE(z);
    WriteUInt8(0x0A);
}

CPacketCustomHouseAddStair::CPacketCustomHouseAddStair(uint16_t graphic, int x, int y)
    : CPacket(25)
{
    WriteUInt8(0xD7);
    WriteUInt16BE(0x0019);
    WriteUInt32BE(g_PlayerSerial);
    WriteUInt16BE(0x000D);
    WriteUInt8(0x00);
    WriteUInt32BE(graphic);
    WriteUInt8(0x00);
    WriteUInt32BE(x);
    WriteUInt8(0x00);
    WriteUInt32BE(y);
    WriteUInt8(0x0A);
}

CPacketOrionVersion::CPacketOrionVersion(int version)
    : CPacket(9)
{
    WriteUInt8(0xFC);
    WriteUInt16BE(9);
    WriteUInt16BE(OCT_ORION_VERSION);
    WriteUInt32BE(version);
}
