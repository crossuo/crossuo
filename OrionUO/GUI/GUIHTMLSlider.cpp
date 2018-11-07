// MIT License
// Copyright (C) August 2016 Hotride

CGUIHTMLSlider::CGUIHTMLSlider(
    CGUIHTMLGump *htmlGump,
    int serial,
    uint16_t graphic,
    uint16_t graphicSelected,
    uint16_t graphicPressed,
    uint16_t backgroundGraphic,
    bool compositeBackground,
    bool vertical,
    int x,
    int y,
    int lenght,
    int minValue,
    int maxValue,
    int value)
    : CGUISlider(
          serial,
          graphic,
          graphicSelected,
          graphicPressed,
          backgroundGraphic,
          compositeBackground,
          vertical,
          x,
          y,
          lenght,
          minValue,
          maxValue,
          value)
    , m_HTMLGump(htmlGump)
{
}

CGUIHTMLSlider::~CGUIHTMLSlider()
{
}

void CGUIHTMLSlider::CalculateOffset()
{
    DEBUG_TRACE_FUNCTION;
    CGUISlider::CalculateOffset();

    if (m_HTMLGump != nullptr)
    {
        Wisp::CPoint2Di currentOffset = m_HTMLGump->CurrentOffset;
        Wisp::CPoint2Di availableOffset = m_HTMLGump->AvailableOffset;

        if (Vertical)
            currentOffset.Y = (int)((availableOffset.Y * Percents) / 100.0f);
        else
            currentOffset.X = (int)((availableOffset.X * Percents) / 100.0f);

        m_HTMLGump->CurrentOffset = currentOffset;
    }
}

