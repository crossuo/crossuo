// AGPLv3 License
// Copyright (c) 2020 Jean-Martin Miljours
// Copyright (c) 2020 CrossUO Team

#include "Gump.h"

struct TrackedItem
{
    uint16_t graphic = 0;
    uint16_t color = 0;
};

class CGumpResourceTracker : public CGump
{
private:
    static constexpr int BOX_SIZE = 60;
    static constexpr int GRID_SIZE = 20;
    static constexpr int MAX_WIDTH = GRID_SIZE * BOX_SIZE;
    static constexpr int MAX_HEIGHT = GRID_SIZE * BOX_SIZE;
    TrackedItem items[GRID_SIZE][GRID_SIZE]{};
    uint16_t numRow = 0;
    uint16_t numCol = 0;
    int16_t currCol = -1;
    int16_t currRow = -1;
    bool drawDebug = false;
    bool hasItemInGump = false;
    static constexpr int ID_GWM_RESIZE = 2;
    int m_StartResizeWidth = 0;
    int m_StartResizeHeight = 0;
    CGUIResizeButton *m_Resizer = nullptr;
    CGUIResizepic *m_Background = nullptr;
    CGUIChecktrans *m_Trans = nullptr;
    CGUIDataBox *m_gridDataBoxGUI = nullptr;
    CGUIDataBox *m_itemsDataBoxGui = nullptr;

    void DrawText();
    void DrawGrid();
    void UpdateSize();
    void DeleteGrid();
    void DrawItem();

public:
    CGumpResourceTracker(short x, short y);
    virtual ~CGumpResourceTracker() = default;

    int Width = 515;
    int Height = 75;
    int CountItemBackPack(uint32_t graph); // TODO move this in item

    virtual void Draw() override;
    virtual void PrepareContent() override;
    virtual void UpdateContent() override;
    virtual void OnLeftMouseButtonUp() override;
    virtual void OnLeftMouseButtonDown() override;
    virtual bool OnLeftMouseButtonDoubleClick() override;

    GUMP_BUTTON_EVENT_H override;
    GUMP_RESIZE_START_EVENT_H override;
    GUMP_RESIZE_EVENT_H override;
    GUMP_RESIZE_END_EVENT_H override;
};
