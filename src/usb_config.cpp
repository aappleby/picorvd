#include "tusb.h"

//------------------------------------------------------------------------------

tusb_desc_device_t const desc_device = {
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    // Use Interface Association Descriptor (IAD) for CDC
    // As required by USB Specs IAD's subclass must be common class (2) and
    // protocol must be IAD (1)
    .bDeviceClass       = TUSB_CLASS_MISC,
    .bDeviceSubClass    = MISC_SUBCLASS_COMMON,
    .bDeviceProtocol    = MISC_PROTOCOL_IAD,
    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = 0xCAFE,
    .idProduct          = 0x4001,
    .bcdDevice          = 0x0100,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void) {
  return (uint8_t const *) &desc_device;
}

//------------------------------------------------------------------------------

#define CONFIG_TOTAL_LEN    (TUD_CONFIG_DESC_LEN + TUD_CDC_DESC_LEN)
#define EPNUM_CDC_0_NOTIF   0x81
#define EPNUM_CDC_0_OUT     0x02
#define EPNUM_CDC_0_IN      0x82

uint8_t const desc_fs_configuration[] =
{
  // Config number, interface count, string index, total length, attribute,
  // power in mA
  TUD_CONFIG_DESCRIPTOR(1, 2, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // 1st CDC: Interface number, string index, EP notification address and size,
  // EP data address (out, in) and size.
  TUD_CDC_DESCRIPTOR(0, 4, EPNUM_CDC_0_NOTIF, 8, EPNUM_CDC_0_OUT, EPNUM_CDC_0_IN, 64),
};

uint8_t const * tud_descriptor_configuration_cb(uint8_t index) {
  (void) index;
  return desc_fs_configuration;
}

//------------------------------------------------------------------------------

char const* string_desc_arr [] =
{
  "TanjentTron",    // 1: Manufacturer
  "TanjentTronDev", // 2: Product
  "314159",         // 3: Serials, should use chip ID
  "TanjentTronCDC", // 4: CDC Interface
};
static const int string_count = sizeof(string_desc_arr)/sizeof(string_desc_arr[0]);

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid) {
  static uint16_t _desc_str[32];
  (void) langid;

  if ( index == 0) {
    _desc_str[0] = 0x0304;
    _desc_str[1] = 0x0409;
    return _desc_str;
  }

  index = index - 1;
  if (index >= string_count) return NULL;
  const char* str = string_desc_arr[index];

  // first byte is length (including header), second byte is string type
  int len = strlen(str);
  _desc_str[0] = (TUSB_DESC_STRING << 8) | (2 + len*2);

  for(int i = 0; i < len; i++) {
    _desc_str[i + 1] = str[i];
  }

  return _desc_str;
}

//------------------------------------------------------------------------------
