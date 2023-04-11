#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "pico/binary_info.h"
#include "build/singlewire.pio.h"
#include "hardware/clocks.h"
#include "SLDebugger.h"
#include "GDBServer.h"
#include "utils.h"
#include "WCH_Regs.h"
#include <assert.h>
#include <ctype.h>
#include "tests.h"
#include "tusb.h"

//------------------------------------------------------------------------------

SLDebugger sl;
GDBServer server;

const uint SWD_PIN = 28;

extern "C" {
  //void stdio_usb_out_chars(const char *buf, int length);
  //int  stdio_usb_in_chars (char *buf, int length);
  //uint32_t tud_cdc_n_available(uint8_t itf);
  //bool tusb_init(void);

  void uart_write_blocking(uart_inst_t *uart, const uint8_t *src, size_t len);
  void uart_read_blocking (uart_inst_t *uart, uint8_t *dst, size_t len);
};

void ser_put(char c) {
  if (c == '\n') ser_put('\r');
  uart_write_blocking(uart0, (uint8_t*)&c, 1);
}

//------------------------------------------------------------------------------

void dispatch_command(const char* command) {
  CHECK(sl.sanity());

  bool handled = false;

  if (strcmp(command, "status") == 0)       { handled = true; sl.print_status(); }
  if (strcmp(command, "wipe_chip") == 0)    { handled = true; sl.wipe_chip(); }
  if (strcmp(command, "write_flash") == 0)  { handled = true; test_flash_write(sl); }
  if (strcmp(command, "unhalt") == 0)       { handled = true; sl.unhalt(); }
  if (strcmp(command, "halt") == 0)         { handled = true; sl.halt(); }
  if (strcmp(command, "reset_dbg") == 0)    { handled = true; sl.reset_dbg(); }
  if (strcmp(command, "reset_cpu_and_halt") == 0)    { handled = true; sl.reset_cpu_and_halt(); }
  if (strcmp(command, "clear_err") == 0)    { handled = true; sl.clear_err(); }

  if (strcmp(command, "lock_flash") == 0)   { handled = true; sl.lock_flash(); }
  if (strcmp(command, "unlock_flash") == 0) { handled = true; sl.unlock_flash(); }

  if (strcmp(command, "dump_flash") == 0) {
    uint32_t buf[512];
    sl.get_block_aligned(0x08000000, buf, 2048);
    for (int y = 0; y < 64; y++) {
      for (int x = 0; x < 8; x++) {
        printf("0x%08x ", buf[x + 8*y]);
      }
      printf("\n");
    }
    handled = true;
  }

  if (strcmp(command, "dump_ram") == 0) {
    uint32_t buf[512];
    sl.get_block_aligned(0x20000000, buf, 2048);
    for (int y = 0; y < 64; y++) {
      for (int x = 0; x < 8; x++) {
        printf("0x%08x ", buf[x + 8*y]);
      }
      printf("\n");
    }
  }

  if (strcmp(command, "step") == 0) {
    printf("DPC before step 0x%08x\n", sl.get_csr(CSR_DPC));
    sl.step();
    printf("DPC after step  0x%08x\n", sl.get_csr(CSR_DPC));
    handled = true;
  }

  if (strcmp(command, "set_bp") == 0) {
    int index = sl.set_breakpoint(0x00000102);
    printf("Breakpoint %d set\n", index);
    handled = true;
  }

  if (strcmp(command, "clear_bp") == 0) {
    int index = sl.clear_breakpoint(0x00000102);
    printf("Breakpoint %d cleared\n", index);
    handled = true;
  }

  if (strcmp(command, "dump_bp") == 0) {
    sl.dump_breakpoints();
    handled = true;
  }

  if (strcmp(command, "patch_flash") == 0) {
    sl.patch_flash();
    handled = true;
  }

  if (strcmp(command, "unpatch_flash") == 0) {
    sl.unpatch_flash();
    handled = true;
  }

  if (!handled) {
    printf("Command %s not handled\n", command);
  }

  auto acs = Reg_ABSTRACTCS(sl.get_dbg(DM_ABSTRACTCS));
  if (acs.BUSY) {
    printf("BUSY   = %d\n", acs.BUSY);
  }
  if (acs.CMDER) {
    printf("CMDERR = %d\n", acs.CMDER);
    sl.clear_err();
  }

  CHECK(sl.sanity());
}

//------------------------------------------------------------------------------

void update_console(char ser_in) {
  static char command[256] = {0};
  static uint8_t cursor = 0;

  if (ser_in == 8) {
    printf("\b \b");
    cursor--;
    command[cursor] = 0;
    return;
  }
  else if (ser_in == '\n' || ser_in == '\r') {
    command[cursor] = 0;
    printf("\n");
    uint32_t time_a = time_us_32();
    printf("Command %s\n", command);
    dispatch_command(command);
    uint32_t time_b = time_us_32();
    printf("Command took %d us\n", time_b - time_a);
    cursor = 0;
    command[cursor] = 0;
    printf(">> ");
  }
  else {
    printf("%c", ser_in);
    command[cursor++] = ser_in;
    return;
  }
}

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

int main() {
  // Enable non-USB serial port on gpio 0/1 for meta-debug output :D
  stdio_uart_init_full(uart0, 1000000, 0, 1);

  // Init USB
  tud_init(BOARD_TUD_RHPORT);

  printf("PicoDebug 0.0.2\n");

  sl.init_pio(SWD_PIN);
  sl.reset_dbg();

  printf("<starting gdb server>\n");
  server.init(&sl);
  server.state = GDBServer::RECV_PREFIX;

  printf("<starting console>\n");
  printf("\n>> ");

  uint32_t last_ping = time_us_32();
  uint32_t last_dmstatus = sl.get_dbg(DM_DMSTATUS);
  bool old_connected = false;

  while (1) {
    tud_task(); // tinyusb device task

    // Update debug interface status

    uint32_t timestamp = time_us_32();
    if (timestamp - last_ping > 1000000) {
      uint32_t dmstatus = sl.get_dbg(DM_DMSTATUS);
      if (dmstatus != last_dmstatus) {
        printf("\n### DMSTATUS 0x%08x\n", dmstatus);
        last_dmstatus = dmstatus;
      }
      last_ping = timestamp;
    }

    // Update GDB connection status
    {
      bool new_connected = tud_cdc_n_connected(0);

      if (!old_connected && new_connected) {
        printf("GDB connected\n");
        server.sl->halt();
      }
      if (old_connected && !new_connected) {
        printf("GDB disconnected\n");
      }

      old_connected = new_connected;
    }

    // Update GDB stub
    {
      char byte_out = 0;
      bool byte_oe = 0;

      char b = 0;
      int len = 0;

      // ok this "available" check is required for some reason
      if (tud_cdc_n_available(0)) {
        len = tud_cdc_n_read(0, &b, 1);
      }

      if (len) {
        if (isprint(b)) {
          printf("%c", b);
        }
        else {
          printf("{%02x}", b);
        }
      }

      server.update(true, b, len == 1, byte_out, byte_oe);

      if (byte_oe) {
        printf("%c", isprint(byte_out) ? byte_out : '_');
        tud_cdc_n_write(0, &byte_out, 1);
        tud_cdc_n_write_flush(0);
      }
    }

    // Update uart console
    if (uart_is_readable(uart0)) {
      char r = 0;
      uart_read_blocking(uart0, (uint8_t*)&r, 1);
      update_console(r);
    }
  }


  return 0;
}

//------------------------------------------------------------------------------
