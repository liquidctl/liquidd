#pragma once

#include <glib-object.h>

#include "hid_device.h"
#include "hid_device_info.h"

G_BEGIN_DECLS

#define LIQUID_TYPE_DRIVER_NZXT_SMART2 (liquid_driver_nzxt_smart2_get_type())
G_DECLARE_FINAL_TYPE(LiquidDriverNzxtSmart2, liquid_driver_nzxt_smart2, LIQUID, DRIVER_NZXT_SMART2, GObject)

gboolean
liquid_driver_nzxt_smart2_match(LiquidHidDeviceInfo *info);

LiquidDriverNzxtSmart2 *
liquid_driver_nzxt_smart2_new(LiquidHidDevice *device);

gboolean
liquid_driver_nzxt_smart2_init_device(LiquidDriverNzxtSmart2 *driver, GError **error);

G_END_DECLS