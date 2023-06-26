#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#ifdef RP2040

#include "d_event.h"
#include "d_main.h"
#include "doomdef.h"
#include "doomtype.h"

#include "global_data.h"

#include "tables.h"

#include "i_system_e32.h"

#include "lprintf.h"

#include "hardware/spi.h"
#include "pico/runtime.h"
#include "pico/stdlib.h"
#include "st7789.h"

//**************************************************************************************

static int8_t scan_list[] = {22, 21, 20, 19, 18};
static int8_t keyd_list_line1[] = {KEYD_SELECT, KEYD_UP, KEYD_RIGHT, KEYD_DOWN,
                                   KEYD_LEFT};
static int8_t keyd_list_line2[] = {KEYD_START, KEYD_A, KEYD_R, KEYD_B, KEYD_L};
static int8_t pressed_line1[] = {0, 0, 0, 0, 0};
static int8_t pressed_line2[] = {0, 0, 0, 0, 0};

void I_InitScreen_e32() {
  // two scan lines
  gpio_init(17);
  gpio_init(16);
  gpio_set_dir(17, GPIO_OUT);
  gpio_set_dir(16, GPIO_OUT);

  // 4 + 1input lines, pulled down
  for (int i = 0; i < sizeof(scan_list); i++) {
    int pin = scan_list[i];
    gpio_init(pin);
    gpio_set_dir(pin, GPIO_IN);
    // pull down
    gpio_set_pulls(pin, 0, 1);
  }
}

//**************************************************************************************

void I_ClearWindow_e32() {}

//**************************************************************************************

static unsigned short buffer1[MAX_SCREENHEIGHT * MAX_SCREENWIDTH];

unsigned short *I_GetBackBuffer() { return &buffer1[0]; }

//**************************************************************************************

unsigned short *I_GetFrontBuffer() { return &buffer1[0]; }

//**************************************************************************************

static ST7789_t *sobj;

#ifdef SMALL_SPI
#define WIDTH 240
#define HEIGHT 240

#define PIN_CS 29
#define PIN_DC 28
#define PIN_RST 27
#define PIN_BL 26
#else
#define WIDTH 320
#define HEIGHT 240
#define PIN_CS 10
#define PIN_DC 11
// NO RST
#define PIN_BL 2
#define PIN_RD 13
#define PIN_D0 14
#define PIN_WR 12
#endif

// One line
static uint16_t st_buffer[WIDTH];
static uint16_t pal_ram[256];

void I_CreateWindow_e32() {
#ifdef SMALL_SPI
  sobj = ST7789_spi_create(spi_default, WIDTH, HEIGHT, PIN_CS, PIN_RST, PIN_DC,
                           PIN_BL, PICO_DEFAULT_SPI_TX_PIN,
                           PICO_DEFAULT_SPI_SCK_PIN);
#else
  sobj = ST7789_parallel_create(WIDTH, HEIGHT, PIN_CS, PIN_DC, PIN_BL, PIN_WR,
                                PIN_RD, PIN_D0);
#endif
  ST7789_backlight(sobj, 255);

  uint16_t *st_ptr = st_buffer;
  for (int i = 0; i < WIDTH; i++) {
    *st_ptr++ = 0;
  }
  for (int j = 0; j < HEIGHT; j++) {
    ST7789_blit_buffer(sobj, (char *)st_buffer, WIDTH * 2, 0, j, WIDTH, 1);
  }
}

//**************************************************************************************

void I_CreateBackBuffer_e32() { I_CreateWindow_e32(); }

//**************************************************************************************

void I_FinishUpdate_e32(const byte *srcBuffer, const byte *pallete,
                        const unsigned int width, const unsigned int height) {
  for (int i = 0; i < 256; i++) {
    unsigned int r = *pallete++;
    unsigned int g = *pallete++;
    unsigned int b = *pallete++;

    uint16_t p = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xFF) >> 3);
    uint16_t t = (p >> 8) | (p << 8);
    pal_ram[i] = t;
  }

  for (int j = 0; j < height; j++) {
    uint16_t *st_ptr = st_buffer;
    for (int i = 0; i < width * 2; i++) {
      *st_ptr++ = pal_ram[*srcBuffer++];
    }
    ST7789_blit_buffer(sobj, (char *)st_buffer, width * 4, (WIDTH - MAX_SCREENWIDTH*2) / 2,
                       (HEIGHT - MAX_SCREENHEIGHT) / 2 + j, width * 2, 1);
  }
}

//**************************************************************************************

void I_SetPallete_e32(const byte *pallete) {}

//**************************************************************************************

void I_ProcessKeyEvents() {

  event_t ev = {0, 0, 0, 0};
  gpio_put(16, 1);
  sleep_us(1); // wait for gpio changes to get into effect
  for (int i = 0; i < sizeof(scan_list); i++) {
    int pin = scan_list[i];
    bool status = gpio_get(pin);
    if (status) {
      if (!pressed_line1[i]) {
        pressed_line1[i] = 1;
        ev.type = ev_keydown;
        ev.data1 = keyd_list_line1[i];
        D_PostEvent(&ev);
      }
    } else {
      if (pressed_line1[i]) {
        pressed_line1[i] = 0;
        ev.type = ev_keyup;
        ev.data1 = keyd_list_line1[i];
        D_PostEvent(&ev);
      }
    }
  }
  gpio_put(17, 1);
  gpio_put(16, 0);
  sleep_us(1); // wait for gpio changes to get into effect
  for (int i = 0; i < sizeof(scan_list); i++) {
    int pin = scan_list[i];
    bool status = gpio_get(pin);
    if (status) {
      if (!pressed_line2[i]) {
        pressed_line2[i] = 1;
        ev.type = ev_keydown;
        ev.data1 = keyd_list_line2[i];
        D_PostEvent(&ev);
      }
    } else {
      if (pressed_line2[i]) {
        pressed_line2[i] = 0;
        ev.type = ev_keyup;
        ev.data1 = keyd_list_line2[i];
        D_PostEvent(&ev);
      }
    }
  }
  gpio_put(17, 0);
}

//**************************************************************************************

#define MAX_MESSAGE_SIZE 1024

void I_Error(const char *error, ...) {
  char msg[MAX_MESSAGE_SIZE];

  va_list v;
  va_start(v, error);

  vsprintf(msg, error, v);

  va_end(v);

  printf("%s", msg);
}

//**************************************************************************************

void I_Quit_e32() {}

//**************************************************************************************

#endif