// AGPLv3 License
// Copyright (c) 2020 Jean-Martin Miljours
// Copyright (c) 2020 CrossUO Team

#include "Gump.h"

class CGumpToolbar : public CGump
{
private:
    uint32_t Items[12];
    uint8_t m_Count = 0;
    uint16_t m_Clear = 0;
    void DrawContent();

public:
    CGumpToolbar(short x, short y);
    virtual ~CGumpToolbar();

    int Width = 515;
    int Height = 75;
    int CountItemBackPack(uint32_t graph); // TODO move this in item
    GUMP_BUTTON_EVENT_H override;

    virtual void PrepareContent() override;
    virtual void UpdateContent() override;
    virtual void GenerateFrame(bool stop) override;
    virtual bool OnLeftMouseButtonDoubleClick() override;
};
