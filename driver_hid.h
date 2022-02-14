#pragma once

#include "driver.h"
#include "hid_device.h"
#include "hid_device_info.h"

G_BEGIN_DECLS

#define LIQUID_TYPE_DRIVER_HID (liquid_driver_hid_get_type())
G_DECLARE_DERIVABLE_TYPE(LiquidDriverHid, liquid_driver_hid, LIQUID, DRIVER_HID, LiquidDriver)

struct _LiquidDriverHidClass
{
    LiquidDriverClass parent_class;

    gboolean (*input_report)(LiquidDriverHid *driver, GBytes *input_report);
    void (*device_error)(LiquidDriverHid *driver, GError *error);
};

LiquidHidDevice *
liquid_driver_hid_get_device(LiquidDriverHid *driver);

LiquidHidDeviceInfo *
liquid_driver_hid_get_device_info(LiquidDriverHid *driver);

G_END_DECLS
