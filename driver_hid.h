#pragma once

#include <glib-object.h>

#include "hid_device.h"

G_BEGIN_DECLS

#define LIQUID_TYPE_DRIVER_HID (liquid_driver_hid_get_type())
G_DECLARE_DERIVABLE_TYPE(LiquidDriverHid, liquid_driver_hid, LIQUID, DRIVER_HID, GObject)

struct _LiquidDriverHidClass
{
    GObjectClass parent_class;

    gboolean (*input_report)(LiquidDriverHid *driver, GBytes *input_report);
    void (*device_error)(LiquidDriverHid *driver, GError *error);
};

LiquidHidDevice *
liquid_driver_hid_get_device(LiquidDriverHid *driver);

G_END_DECLS
