#include "hid_device_info.h"

#include <stdio.h>

#include <linux/input.h>
#include "glibconfig.h"

struct _LiquidHidDeviceInfo
{
    GObject parent;

    gchar *hidraw_path;
    guint32 vendor_id;
    guint32 product_id;
};

G_DEFINE_FINAL_TYPE(LiquidHidDeviceInfo, liquid_hid_device_info, G_TYPE_OBJECT)

enum
{
    PROP_0,
    PROP_HIDRAW_PATH,
    PROP_VENDOR_ID,
    PROP_PRODUCT_ID,
    N_PROPERTIES
};

static GParamSpec *pspecs[N_PROPERTIES];

static void
liquid_hid_device_info_get_property(GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
    LiquidHidDeviceInfo *info = LIQUID_HID_DEVICE_INFO(object);

    switch (property_id)
    {
    case PROP_HIDRAW_PATH:
        g_value_set_string(value, info->hidraw_path);
        break;

    case PROP_VENDOR_ID:
        g_value_set_uint(value, info->vendor_id);
        break;

    case PROP_PRODUCT_ID:
        g_value_set_uint(value, info->product_id);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
liquid_hid_device_info_set_property(GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
    LiquidHidDeviceInfo *info = LIQUID_HID_DEVICE_INFO(object);

    switch (property_id)
    {
    case PROP_HIDRAW_PATH:
        g_clear_pointer(&info->hidraw_path, g_free);
        info->hidraw_path = g_value_dup_string(value);
        break;

    case PROP_VENDOR_ID:
        info->vendor_id = g_value_get_uint(value);
        break;

    case PROP_PRODUCT_ID:
        info->product_id = g_value_get_uint(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
    }
}

static void
liquid_hid_device_info_finalize(GObject *object)
{
    LiquidHidDeviceInfo *info = LIQUID_HID_DEVICE_INFO(object);

    g_clear_pointer(&info->hidraw_path, g_free);

    G_OBJECT_CLASS(liquid_hid_device_info_parent_class)->finalize(object);
}

static void
liquid_hid_device_info_init(LiquidHidDeviceInfo *info G_GNUC_UNUSED)
{
}

static void
liquid_hid_device_info_class_init(LiquidHidDeviceInfoClass *class)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(class);

    gobject_class->finalize = liquid_hid_device_info_finalize;
    gobject_class->get_property = liquid_hid_device_info_get_property;
    gobject_class->set_property = liquid_hid_device_info_set_property;

    pspecs[PROP_HIDRAW_PATH]
        = g_param_spec_string("hidraw-path", /* name */
                              "hidraw device path", /* nick */
                              "hidraw device path", /* blurb */
                              NULL, /* default_value */
                              G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    pspecs[PROP_VENDOR_ID]
        = g_param_spec_uint("vendor-id", /* name */
                            "Vendor ID", /* nick */
                            "Vendor ID", /* blurb */
                            0, /* minimum */
                            G_MAXUINT, /* maximum */
                            0, /* default_value */
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    pspecs[PROP_PRODUCT_ID]
        = g_param_spec_uint("product-id", /* name */
                            "Product ID", /* nick */
                            "Product ID", /* blurb */
                            0, /* minimum */
                            G_MAXUINT, /* maximum */
                            0, /* default_value */
                            G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY | G_PARAM_STATIC_STRINGS);

    g_object_class_install_properties(gobject_class, N_PROPERTIES, pspecs);
}

const gchar *
liquid_hid_device_info_get_hidraw_path(LiquidHidDeviceInfo *info)
{
    g_return_val_if_fail(LIQUID_IS_HID_DEVICE_INFO(info), NULL);

    return info->hidraw_path;
}

unsigned int
liquid_hid_device_info_get_vendor_id(LiquidHidDeviceInfo *info)
{
    g_return_val_if_fail(LIQUID_IS_HID_DEVICE_INFO(info), 0);

    return info->vendor_id;
}

unsigned int
liquid_hid_device_info_get_product_id(LiquidHidDeviceInfo *info)
{
    g_return_val_if_fail(LIQUID_IS_HID_DEVICE_INFO(info), 0);

    return info->product_id;
}

LiquidHidDeviceInfo *
liquid_hid_device_info_new_for_udev_device(GUdevDevice *udev_device)
{
    const gchar *hidraw_path = g_udev_device_get_device_file(udev_device);

    if (hidraw_path == NULL)
    {
        return NULL;
    }

    g_autoptr(GUdevDevice) parent = g_udev_device_get_parent_with_subsystem(udev_device, "hid", NULL);

    if (parent == NULL)
    {
        return NULL;
    }

    const gchar *hid_id = g_udev_device_get_property(parent, "HID_ID");

    if (hid_id == NULL)
    {
        return NULL;
    }

    unsigned int bus;
    unsigned int vendor;
    unsigned int product;

    if (sscanf(hid_id, "%x:%x:%x", &bus, &vendor, &product) != 3)
    {
        return NULL;
    }

    if (bus != BUS_USB)
    {
        return NULL;
    }

    return g_object_new(LIQUID_TYPE_HID_DEVICE_INFO,
                        "hidraw-path",
                        hidraw_path,
                        "vendor-id",
                        vendor,
                        "product-id",
                        product,
                        NULL);
}
