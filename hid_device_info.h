#pragma once

#include <glib-object.h>

#include <gudev/gudev.h>

G_BEGIN_DECLS

#define LIQUID_TYPE_HID_DEVICE_INFO (liquid_hid_device_info_get_type())
G_DECLARE_FINAL_TYPE(LiquidHidDeviceInfo, liquid_hid_device_info, LIQUID, HID_DEVICE_INFO, GObject)

const gchar *
liquid_hid_device_info_get_hidraw_path(LiquidHidDeviceInfo *info);

unsigned int
liquid_hid_device_info_get_vendor_id(LiquidHidDeviceInfo *info);

unsigned int
liquid_hid_device_info_get_product_id(LiquidHidDeviceInfo *info);

LiquidHidDeviceInfo *
liquid_hid_device_info_new_for_udev_device(GUdevDevice *udev_device);

G_END_DECLS
