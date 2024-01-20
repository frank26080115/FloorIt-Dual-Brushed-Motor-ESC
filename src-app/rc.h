#pragma once

#include "main.h"
#include "cereal.h"

class RcChannel
{
    public:
        virtual void init(void);
        virtual void task(void);
        virtual int16_t read(void);
        virtual bool is_alive(void);
        virtual bool has_new(bool clr);
};

extern int16_t rc_pulse_map(uint16_t x);
