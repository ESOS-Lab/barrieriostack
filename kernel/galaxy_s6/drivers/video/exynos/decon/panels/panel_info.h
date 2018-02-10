#ifndef __PANEL_INFO_H__
#define __PANEL_INFO_H__

#if defined(CONFIG_PANEL_S6E3HA2_DYNAMIC)
#include "s6e3ha2_s6e3ha0_wqhd_param.h"

#elif defined(CONFIG_PANEL_S6E3HF2_DYNAMIC)
#include "s6e3hf2_wqhd_param.h"

#else
#error "ERROR !! Check LCD Panel Header File"
#endif

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#endif

#endif
