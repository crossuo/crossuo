// MIT License
// Copyright (C) August 2016 Hotride

#ifndef GumpQuestionH
#define GumpQuestionH

#include "Gump.h"

class CGumpQuestion : public CGump
{
    uint8_t Variant = 0;

private:
    static const int ID_GQ_BUTTON_CANCEL = 1;
    static const int ID_GQ_BUTTON_OKAY = 2;

public:
    CGumpQuestion(uint32_t serial, short x, short y, uint8_t variant);
    virtual ~CGumpQuestion();

    static const int ID_GQ_STATE_QUIT = 1;
    static const int ID_GQ_STATE_ATTACK_REQUEST = 2;

    GUMP_BUTTON_EVENT_H;
};

#endif
