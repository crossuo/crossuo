// SPDX-License-Identifier: AGPL-3.0-or-later
// SPDX-FileCopyrightText: 2020 Jean-Martin Miljours
// SPDX-FileCopyrightText: 2020 Danny Angelo Carminati Grein

#include "Gump.h"

#define TRACKER_BOX_SIZE (60)
#define TRACKER_WIDTH ((8 * TRACKER_BOX_SIZE) + 5)
#define TRACKER_HEIGHT (TRACKER_BOX_SIZE + 5)

void ResourceTracker_LoadStaticContents(CMappedFile &reader);
void ResourceTracker_SaveStaticContents(Wisp::CBinaryFileWriter &writer);

struct TrackedItem
{
    uint16_t Graphic = 0;
    uint16_t Color = 0;
};

class CGumpResourceTracker : public CGump
{
private:
    int16_t CurrCol = -1;
    int16_t CurrRow = -1;
    bool DrawDebug = false;
    bool HasItemInGump = false;
    int StartResizeWidth = 0;
    int StartResizeHeight = 0;
    CGUIResizeButton *m_Resizer = nullptr;
    CGUIResizepic *m_Background = nullptr;
    CGUIChecktrans *m_Trans = nullptr;
    CGUIDataBox *m_DataBox = nullptr;

    void UpdateSize();
    void UpdateGrid();
    void UpdateItems();
    void UpdateCounters();
    int CountItemBackPack(uint16_t graphic);

public:
    CGumpResourceTracker(int x, int y, int w = 0, int h = 0);
    virtual ~CGumpResourceTracker() = default;
    uint16_t Rows = 0;
    uint16_t Cols = 0;

    virtual void Draw() override;
    virtual void PrepareContent() override;
    virtual void UpdateContent() override;
    virtual void OnLeftMouseButtonUp() override;
    virtual bool OnLeftMouseButtonDoubleClick() override;

    GUMP_RESIZE_START_EVENT_H override;
    GUMP_RESIZE_EVENT_H override;
    GUMP_RESIZE_END_EVENT_H override;
};
