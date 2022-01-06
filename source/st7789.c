/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ivan Belokobylskiy
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

#define __ST7789_VERSION__  "0.1.5"

#include <stdlib.h>
#include <stdio.h>
#include <hardware/spi.h>
#include <hardware/gpio.h>
#include "st7789.h"


#define _swap_int16_t(a, b) { int16_t t = a; a = b; b = t; }
#define ABS(N) (((N)<0)?(-(N)):(N))

#define CS_LOW()     { if(self->cs) {mp_hal_pin_write(self->cs, 0);} }
#define CS_HIGH()    { if(self->cs) {mp_hal_pin_write(self->cs, 1);} }
#define DC_LOW()     mp_hal_pin_write(self->dc, 0)
#define DC_HIGH()    mp_hal_pin_write(self->dc, 1)
#define RESET_LOW()  mp_hal_pin_write(self->reset, 0)
#define RESET_HIGH() mp_hal_pin_write(self->reset, 1)
#define DISP_HIGH()  mp_hal_pin_write(self->backlight, 1)
#define DISP_LOW()   mp_hal_pin_write(self->backlight, 0)


// this is the actual C-structure for our new object
typedef struct _ST7789_t {
    spi_inst_t *spi_obj;
    uint8_t width;
    uint8_t height;
    uint8_t xstart;
    uint8_t ystart;
    hal_pin_t reset;
    hal_pin_t dc;
    hal_pin_t cs;
    hal_pin_t backlight;
} ST7789_t;


static void mp_hal_pin_write(uint pin , uint value  ) {
    if ( 0 != pin) {
        gpio_put( pin, value == 1);
    }
}

/* methods start */

static void write_cmd(ST7789_t *self, uint8_t cmd, const uint8_t *data, int len) {
    CS_LOW()
    if (cmd) {
        DC_LOW();
        spi_write_blocking(self->spi_obj, &cmd, 1);
    }
    if (len > 0) {
        DC_HIGH();
        spi_write_blocking(self->spi_obj, data, len);
    }
    CS_HIGH()
}

static void set_window(ST7789_t *self, uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    if (x0 > x1 || x1 >= self->width) {
        return;
    }
    if (y0 > y1 || y1 >= self->height) {
        return;
    }
    uint8_t bufx[4] = {(x0+self->xstart) >> 8, (x0+self->xstart) & 0xFF, (x1+self->xstart) >> 8, (x1+self->xstart) & 0xFF};
    uint8_t bufy[4] = {(y0+self->ystart) >> 8, (y0+self->ystart) & 0xFF, (y1+self->ystart) >> 8, (y1+self->ystart) & 0xFF};
    write_cmd(self, ST7789_CASET, bufx, 4);
    write_cmd(self, ST7789_RASET, bufy, 4);
    write_cmd(self, ST7789_RAMWR, NULL, 0);
}

static void fill_color_buffer(spi_inst_t *spi_obj, uint16_t color, int length) {
    uint8_t hi = color >> 8, lo = color;
    const int buffer_pixel_size = 128;
    int chunks = length / buffer_pixel_size;
    int rest = length % buffer_pixel_size;

    uint8_t buffer[buffer_pixel_size * 2]; // 128 pixels
    // fill buffer with color data
    for (int i = 0; i < length && i < buffer_pixel_size; i++) {
        buffer[i*2] = hi;
        buffer[i*2 + 1] = lo;
    }

    if (chunks) {
        for (int j = 0; j < chunks; j ++) {
            spi_write_blocking(spi_obj, buffer, buffer_pixel_size*2);
        }
    }
    if (rest) {
        spi_write_blocking(spi_obj, buffer, rest*2);
    }
}


void ST7789_draw_pixel(ST7789_t *self, uint8_t x, uint8_t y, uint16_t color) {
    uint8_t hi = color >> 8, lo = color;
    set_window(self, x, y, x, y);
    DC_HIGH();
    CS_LOW();
    spi_write_blocking(self->spi_obj, &hi, 1);
    spi_write_blocking(self->spi_obj, &lo, 1);
    CS_HIGH();
}


void ST7789_hline(ST7789_t *self, uint8_t x, uint8_t y, uint16_t w, uint16_t color) {
    set_window(self, x, y, x + w - 1, y);
    DC_HIGH();
    CS_LOW();
    fill_color_buffer(self->spi_obj, color, w);
    CS_HIGH();
}

 void ST7789_vline(ST7789_t *self, uint8_t x, uint8_t y, uint16_t w, uint16_t color) {
    set_window(self, x, y, x, y + w - 1);
    DC_HIGH();
    CS_LOW();
    fill_color_buffer(self->spi_obj, color, w);
    CS_HIGH();
}


static void ST7789_hard_reset(ST7789_t *self) {
    CS_LOW();
    RESET_HIGH();
    sleep_ms(50);
    RESET_LOW();
    sleep_ms(50);
    RESET_HIGH();
    sleep_ms(150);
    CS_HIGH();
}

static void ST7789_soft_reset(ST7789_t *self) {
    write_cmd(self, ST7789_SWRESET, NULL, 0);
    sleep_ms(150);
}

/*
static mp_obj_t ST7789_sleep_mode(mp_obj_t self_in, mp_obj_t value) {
    ST7789_t *self = MP_OBJ_TO_PTR(self_in);
    if(mp_obj_is_true(value)) {
        write_cmd(self, ST7789_SLPIN, NULL, 0);
    } else {
        write_cmd(self, ST7789_SLPOUT, NULL, 0);
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_2(ST7789_sleep_mode_obj, ST7789_sleep_mode);

*/

static void ST7789_inversion_mode(ST7789_t *self, bool value) {
    if(value) {
        write_cmd(self, ST7789_INVON, NULL, 0);
    } else {
        write_cmd(self, ST7789_INVOFF, NULL, 0);
    }
}


void ST7789_fill_rect(ST7789_t *self, uint8_t x, uint8_t y, uint8_t w, uint8_t h, int16_t color) {
    set_window(self, x, y, x + w - 1, y + h - 1);
    DC_HIGH();
    CS_LOW();
    fill_color_buffer(self->spi_obj, color, w * h);
    CS_HIGH();
}


void ST7789_line(ST7789_t *self, int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t color) {

    bool steep = ABS(y1 - y0) > ABS(x1 - x0);
    if (steep) {
        _swap_int16_t(x0, y0);
        _swap_int16_t(x1, y1);
    }

    if (x0 > x1) {
        _swap_int16_t(x0, x1);
        _swap_int16_t(y0, y1);
    }

    uint8_t dx = x1 - x0, dy = ABS(y1 - y0);
    int16_t err = dx >> 1, ystep = -1, xs = x0, dlen = 0;

    if (y0 < y1) ystep = 1;

    // Split into steep and not steep for FastH/V separation
    if (steep) {
        for (; x0 <= x1; x0++) {
        dlen++;
        err -= dy;
        if (err < 0) {
            err += dx;
            if (dlen == 1) ST7789_draw_pixel(self, y0, xs, color);
            else ST7789_vline(self, y0, xs, dlen, color);
            dlen = 0; y0 += ystep; xs = x0 + 1;
        }
        }
        if (dlen) ST7789_vline(self, y0, xs, dlen, color);
    }
    else
    {
        for (; x0 <= x1; x0++) {
        dlen++;
        err -= dy;
        if (err < 0) {
            err += dx;
            if (dlen == 1) ST7789_draw_pixel(self, xs, y0, color);
            else ST7789_hline(self, xs, y0, dlen, color);
            dlen = 0; y0 += ystep; xs = x0 + 1;
        }
        }
        if (dlen) ST7789_hline(self, xs, y0, dlen, color);
    }
}


void  ST7789_blit_buffer(ST7789_t *self, const uint8_t* buf, int buf_len, int16_t x, int16_t y, int16_t w, int16_t h, int16_t color) {
    set_window(self, x, y, x + w -1 , y + h-1);
    DC_HIGH();
    CS_LOW();

    spi_write_blocking( self->spi_obj, buf, buf_len);
    CS_HIGH();
}


void ST7789_init(ST7789_t *self) {
    ST7789_hard_reset(self);
    ST7789_soft_reset(self);
    write_cmd(self, ST7789_SLPOUT, NULL, 0);

    const uint8_t color_mode[] = { COLOR_MODE_65K | COLOR_MODE_16BIT};
    write_cmd(self, ST7789_COLMOD, color_mode, 1);
    sleep_ms(10);
    const uint8_t madctl[] = { ST7789_MADCTL_ML | ST7789_MADCTL_RGB };
    write_cmd(self, ST7789_MADCTL, madctl, 1);

    write_cmd(self, ST7789_INVON, NULL, 0);
    sleep_ms(10);
    write_cmd(self, ST7789_NORON, NULL, 0);
    sleep_ms(10);


    ST7789_fill_rect( self, 0, 0, self->width, self->height, BLACK);
    write_cmd(self, ST7789_DISPON, NULL, 0);
    sleep_ms(100);
}


void ST7789_on(ST7789_t *self) {
    DISP_HIGH();
    sleep_ms(10);
}


void ST7789_off(ST7789_t *self) {
    DISP_LOW();
    sleep_ms(10);
}


/*
static mp_obj_t ST7789_rect(size_t n_args, const mp_obj_t *args) {
    ST7789_t *self = MP_OBJ_TO_PTR(args[0]);
    mp_int_t x = mp_obj_get_int(args[1]);
    mp_int_t y = mp_obj_get_int(args[2]);
    mp_int_t w = mp_obj_get_int(args[3]);
    mp_int_t h = mp_obj_get_int(args[4]);
    mp_int_t color = mp_obj_get_int(args[5]);

    ST7789_hline(self, x, y, w, color);
    ST7789_vline(self, x, y, h, color);
    ST7789_hline(self, x, y + h - 1, w, color);
    ST7789_vline(self, x + w - 1, y, h, color);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(ST7789_rect_obj, 6, 6, ST7789_rect);
*/

static struct _ST7789_t  st7789;

ST7789_t *ST7789_create(spi_inst_t *spi_obj, int16_t width, int16_t height, uint8_t reset, uint8_t dc, uint8_t backlight) {
    // YUCK
    ST7789_t *self = &st7789;
    self->spi_obj = spi_obj;
    self->width = width;
    self->height = height;

    if (self->width == 240 && self->height == 240) {
        self->xstart = ST7789_240x240_XSTART;
        self->ystart = ST7789_240x240_YSTART;
    } else if (self->width == 135 && self->height == 240) {
        self->xstart = ST7789_135x240_XSTART;
        self->ystart = ST7789_135x240_YSTART;
    } else {
        printf("Unsupported display. Only 240x240 and 135x240 are supported without xstart and ystart provided");
    }

    self->reset = reset;
    self->dc = dc;
    // YUCK
    self->cs = 19;
    self->backlight = backlight;
    return self;
}

static uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3);
}

static void map_bitarray_to_rgb565(uint8_t const *bitarray, uint8_t *buffer, int length, int width,
                                  uint16_t color, uint16_t bg_color) {
    int row_pos = 0;
    for (int i = 0; i < length; i++) {
        uint8_t byte = bitarray[i];
        for (int bi = 7; bi >= 0; bi--) {
            uint8_t b = byte & (1 << bi);
            uint16_t cur_color = b ? color : bg_color;
            *buffer = (cur_color & 0xff00) >> 8;
            buffer ++;
            *buffer = cur_color & 0xff;
            buffer ++;

            row_pos ++;
            if (row_pos >= width) {
                row_pos = 0;
                break;
            }
        }
    }
}

