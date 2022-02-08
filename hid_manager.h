#pragma once

#include <glib-object.h>

#include <gudev/gudev.h>

#include "hid_device_info.h"

G_BEGIN_DECLS

#define LIQUID_TYPE_HID_MANAGER (liquid_hid_manager_get_type())
G_DECLARE_FINAL_TYPE(LiquidHidManager, liquid_hid_manager, LIQUID, HID_MANAGER, GObject)

LiquidHidManager *
liquid_hid_manager_new(GUdevClient *udev_client);

/* Intentionally has the same signature as (the future) 'device-added' signal */
typedef void (*LiquidHidManagerForEachDeviceCallback)(LiquidHidManager *manager,
                                                      LiquidHidDeviceInfo *info,
                                                      gpointer user_data);

void
liquid_hid_manager_for_each_device(LiquidHidManager *manager,
                                   LiquidHidManagerForEachDeviceCallback callback,
                                   gpointer user_data);

G_END_DECLS
