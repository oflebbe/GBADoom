/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ivan Belokobylskiy
 * Copyright (c) 2023 Olaf Flebbe
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <hardware/clocks.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/pio.h>
#include <hardware/pwm.h>
#include <hardware/spi.h>
#include <math.h>
#include <pico/time.h>
#include <stdio.h>
#include <stdlib.h>

#include "st7789.h"
#include "st7789_parallel.pio.h"

typedef struct _ST7789_t {
  spi_inst_t *spi_obj;
  uint width;
  uint height;
  uint xstart;
  uint ystart;
#ifdef SMALL_SPI
  uint reset;
#endif
  uint dc;
  uint cs;
  uint backlight;
  uint parallel_sm;
  PIO parallel_pio;
  uint st_dma;
} ST7789_t;

static void write_blocking_parallel(ST7789_t *self, const uint8_t *src,
                                    size_t len) {
  while (len--) {
    uint32_t p = *src++ << 24;
    pio_sm_put_blocking(self->parallel_pio, self->parallel_sm, p);
    asm("nop;");
  }
}

static void write_blocking_dma(ST7789_t *self, const uint8_t *src, size_t len) {
  dma_channel_set_trans_count(self->st_dma, len, false);
  dma_channel_set_read_addr(self->st_dma, src, true);

  dma_channel_wait_for_finish_blocking(self->st_dma);
}

static void write_blocking(ST7789_t *self, const uint8_t *src, size_t len) {
  if (len > 100000) {
    write_blocking_dma(self, src, len);
    return;
  }
  if (self->spi_obj) {
    spi_write_blocking(self->spi_obj, src, len);
  } else {
    write_blocking_parallel(self, src, len);
  }
}

static void write_cmd(ST7789_t *self, uint8_t cmd, const uint8_t *data,
                      size_t len) {
  gpio_put(self->cs, 0);
  if (cmd) {
    gpio_put(self->dc, 0);
    write_blocking(self, &cmd, 1);
  }
  if (len > 0) {
    gpio_put(self->dc, 1);
    write_blocking(self, data, len);
  }
  gpio_put(self->cs, 1);
}

static void set_window(ST7789_t *self, uint16_t x0, uint16_t y0, uint16_t x1,
                       uint16_t y1) {

  uint8_t bufx[4] = {(x0 + self->xstart) >> 8, (x0 + self->xstart) & 0xFF,
                     (x1 + self->xstart) >> 8, (x1 + self->xstart) & 0xFF};
  uint8_t bufy[4] = {(y0 + self->ystart) >> 8, (y0 + self->ystart) & 0xFF,
                     (y1 + self->ystart) >> 8, (y1 + self->ystart) & 0xFF};
  write_cmd(self, ST7789_CASET, bufx, 4);
  write_cmd(self, ST7789_RASET, bufy, 4);
}

#ifdef SMALL_SPI
static void ST7789_hard_reset(ST7789_t *self) {
  gpio_put(self->cs, 0);
  gpio_put(self->reset, 1);
  sleep_ms(50);
  gpio_put(self->reset, 0);
  sleep_ms(50);
  gpio_put(self->reset, 1);
  sleep_ms(150);
  gpio_put(self->cs, 1);
}
#endif

static void ST7789_soft_reset(ST7789_t *self) {
  write_cmd(self, ST7789_SWRESET, NULL, 0);
  sleep_ms(150);
}

void ST7789_backlight(ST7789_t *self, uint8_t brightness) {
  // gamma correct the provided 0-255 brightness value onto a
  // 0-65535 range for the pwm counter
  float gamma = 2.8;
  uint16_t value =
      (uint16_t)(powf((float)(brightness) / 255.0f, gamma) * 65535.0f + 0.5f);
  pwm_set_gpio_level(self->backlight, value);
}


void ST7789_blit_buffer(ST7789_t *self, const uint8_t *buf, size_t buf_len,
                        int16_t x, int16_t y, int16_t w, int16_t h) {
  set_window(self, x, y, x + w - 1, y + h - 1);
  write_cmd(self, ST7789_RAMWR, buf, buf_len);
}

static struct _ST7789_t st7789;

#ifdef SMALL_SPI
ST7789_t *ST7789_spi_create(spi_inst_t *spi_obj, int16_t width, int16_t height,
                            uint cs, uint reset, uint dc, uint backlight,
                            uint tx, uint sck) {

  // YUCK
  ST7789_t *self = &st7789;
  self->spi_obj = spi_obj;
  self->width = width;
  self->height = height;

  self->xstart = 0;
  self->ystart = 0;

  self->reset = reset;
  self->dc = dc;
  self->cs = cs;
  self->backlight = backlight;

  // init dditional pins
  gpio_init(self->cs);
  gpio_set_dir(self->cs, GPIO_OUT);

  gpio_init(self->dc);
  gpio_set_dir(self->dc, GPIO_OUT);

  gpio_init(self->reset);
  gpio_set_dir(self->reset, GPIO_OUT);

  // BL PWM Config
  pwm_config pwm_cfg = pwm_get_default_config();
  pwm_set_wrap(pwm_gpio_to_slice_num(self->backlight), 65535);
  pwm_init(pwm_gpio_to_slice_num(self->backlight), &pwm_cfg, true);
  gpio_set_function(self->backlight, GPIO_FUNC_PWM);
  ST7789_backlight(self, 0); // Turn backlight off initially

  spi_init(self->spi_obj, 50000000);

  gpio_set_function(tx, GPIO_FUNC_SPI);
  gpio_set_function(sck, GPIO_FUNC_SPI);

  spi_set_format(self->spi_obj, 8, SPI_CPOL_1, SPI_CPHA_0, SPI_MSB_FIRST);

  self->st_dma = dma_claim_unused_channel(true);
  dma_channel_config dma_config = dma_channel_get_default_config(self->st_dma);
  channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_8);
  channel_config_set_write_increment(&dma_config, false);
  channel_config_set_read_increment(&dma_config, true); 

  channel_config_set_dreq(&dma_config, spi_get_dreq(self->spi_obj, true));
  dma_channel_set_config(self->st_dma, &dma_config, false);
  dma_channel_set_write_addr(self->st_dma, &spi_get_hw(self->spi_obj)->dr, false);


  ST7789_hard_reset(self);
  ST7789_soft_reset(self);
  write_cmd(self, ST7789_SLPOUT, NULL, 0);

  const uint8_t color_mode[] = {COLOR_MODE_65K | COLOR_MODE_16BIT};
  write_cmd(self, ST7789_COLMOD, color_mode, 1);
  sleep_ms(10);
  const uint8_t madctl[] = {ST7789_MADCTL_ML | ST7789_MADCTL_RGB};
  write_cmd(self, ST7789_MADCTL, madctl, 1);

  write_cmd(self, ST7789_INVON, NULL, 0);
  sleep_ms(10);
  write_cmd(self, ST7789_NORON, NULL, 0);
sleep_ms(10);
  write_cmd(self, ST7789_DISPON, NULL, 0);
sleep_ms(100);
  return self;
}
#else
ST7789_t *ST7789_parallel_create(int16_t width, int16_t height, uint cs,
                                 uint dc, uint backlight, uint wr_sck,
                                 uint rd_sck, uint d0) {
  // YUCK
  ST7789_t *self = &st7789;
  self->spi_obj = NULL;
  self->width = width;
  self->height = height;
  self->xstart = 0;
  self->ystart = 0;

  self->dc = dc;
  self->cs = cs;
  self->backlight = backlight;

  gpio_init(self->cs);
  gpio_set_dir(self->cs, GPIO_OUT);

  gpio_init(self->dc);
  gpio_set_dir(self->dc, GPIO_OUT);

#ifdef PIN_RST
  gpio_init(PIN_RST);
  gpio_set_dir(PIN_RST, GPIO_OUT);
#endif

  // BL PWM Config
  pwm_config pwm_cfg = pwm_get_default_config();
  pwm_set_wrap(pwm_gpio_to_slice_num(self->backlight), 65535);
  pwm_init(pwm_gpio_to_slice_num(self->backlight), &pwm_cfg, true);
  gpio_set_function(self->backlight, GPIO_FUNC_PWM);
  ST7789_backlight(self, 0); // Turn backlight off initially

  self->parallel_pio = pio1;
  self->parallel_sm = pio_claim_unused_sm(self->parallel_pio, true);
  uint parallel_offset =
      pio_add_program(self->parallel_pio, &st7789_parallel_program);
  pio_gpio_init(self->parallel_pio, wr_sck);

  gpio_set_function(rd_sck, GPIO_FUNC_SIO);
  gpio_set_dir(rd_sck, GPIO_OUT);

  for (int i = 0; i < 8; i++) {
    gpio_set_function(d0 + i, GPIO_FUNC_SIO);
    gpio_set_dir(d0 + i, GPIO_OUT);
  }

  for (int i = 0u; i < 8; i++) {
    pio_gpio_init(self->parallel_pio, d0 + i);
  }

  pio_sm_set_consecutive_pindirs(self->parallel_pio, self->parallel_sm, d0, 8,
                                 true);
  pio_sm_set_consecutive_pindirs(self->parallel_pio, self->parallel_sm, wr_sck,
                                 1, true);

  pio_sm_config c = st7789_parallel_program_get_default_config(parallel_offset);

  sm_config_set_out_pins(&c, d0, 8);
  sm_config_set_sideset_pins(&c, wr_sck);
  sm_config_set_fifo_join(&c, PIO_FIFO_JOIN_TX);
  sm_config_set_out_shift(&c, false, true, 8);

  // Determine clock divider
  const uint32_t max_pio_clk = 32 * MHZ;
  const uint32_t sys_clk_hz = clock_get_hz(clk_sys);
  const uint32_t clk_div = (sys_clk_hz + max_pio_clk - 1) / max_pio_clk;
  sm_config_set_clkdiv(&c, clk_div);

  pio_sm_init(self->parallel_pio, self->parallel_sm, parallel_offset, &c);
  pio_sm_set_enabled(self->parallel_pio, self->parallel_sm, true);

  // Allocate, config and init an dma
  self->st_dma = dma_claim_unused_channel(true);
  dma_channel_config dma_config = dma_channel_get_default_config(self->st_dma);
  channel_config_set_transfer_data_size(&dma_config, DMA_SIZE_8);
  channel_config_set_write_increment(&dma_config, false);
  channel_config_set_read_increment(&dma_config, true); 
  channel_config_set_bswap(&dma_config, false);

  channel_config_set_dreq(
      &dma_config, pio_get_dreq(self->parallel_pio, self->parallel_sm, true));
  dma_channel_configure(self->st_dma, &dma_config,
                        &self->parallel_pio->txf[self->parallel_sm], NULL, 0,
                        false);

  gpio_put(rd_sck, 1);

  ST7789_soft_reset(self);
  const uint8_t color_mode[] = {COLOR_MODE_65K | COLOR_MODE_16BIT};
  write_cmd(self, ST7789_COLMOD, color_mode, 1);

  write_cmd(self, ST7789_INVON, NULL, 0);  // set inversion mode
  write_cmd(self, ST7789_SLPOUT, NULL, 0); // leave sleep mode
  write_cmd(self, ST7789_DISPON, NULL, 0); // turn display on

  const uint8_t madctl[] = { ST7789_MADCTL_MY | ST7789_MADCTL_MV |
                             ST7789_MADCTL_RGB};
  write_cmd(self, ST7789_MADCTL, madctl, 1);

  return self;
}

#endif