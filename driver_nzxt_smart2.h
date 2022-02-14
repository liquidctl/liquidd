#pragma once

#include <glib-object.h>

#include "driver_hid.h"
#include "hid_device_info.h"

G_BEGIN_DECLS

#define LIQUID_TYPE_DRIVER_NZXT_SMART2 (liquid_driver_nzxt_smart2_get_type())
G_DECLARE_FINAL_TYPE(LiquidDriverNzxtSmart2, liquid_driver_nzxt_smart2, LIQUID, DRIVER_NZXT_SMART2, LiquidDriverHid)

gboolean
liquid_driver_nzxt_smart2_match(LiquidHidDeviceInfo *info);

LiquidDriverNzxtSmart2 *
liquid_driver_nzxt_smart2_new(LiquidHidDevice *device, LiquidHidDeviceInfo *info);

G_END_DECLS
