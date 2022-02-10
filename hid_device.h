#pragma once

#include <gio/gio.h>

G_BEGIN_DECLS

#define LIQUID_TYPE_HID_DEVICE (liquid_hid_device_get_type())
G_DECLARE_FINAL_TYPE(LiquidHidDevice, liquid_hid_device, LIQUID, HID_DEVICE, GObject)

LiquidHidDevice *
liquid_hid_device_new_for_fd(int fd, guint max_input_report_size);

LiquidHidDevice *
liquid_hid_device_new_for_path(const char *path, guint max_input_report_size, GError **error);

gboolean
liquid_hid_device_output_report(LiquidHidDevice *device, const void *buffer, gsize count, GError **error);

G_END_DECLS
