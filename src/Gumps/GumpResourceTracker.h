// AGPLv3 License
// Copyright (c) 2020 Jean-Martin Miljours
// Copyright (c) 2020 CrossUO Team

#include "Gump.h"

class CGumpResourceTracker : public CGump
{
private:
    uint16_t **items{ 0 };
    uint8_t boxDimension = 60;
    uint16_t oldRow = 0;
    uint16_t oldCol = 0;
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
    void ResetTable();
    uint16_t **ResizeTable(int row, int col);

public:
    CGumpResourceTracker(short x, short y);
    virtual ~CGumpResourceTracker();

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
