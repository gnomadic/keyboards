/*
 * drivers/display/gc9a01.c
 * 
 * Simple GC9A01 Round Display Driver for ZMK
 * Based on standard Zephyr display driver patterns
 */

#define DT_DRV_COMPAT gc9a01

#include <zephyr/device.h>
#include <zephyr/drivers/display.h>
#include <zephyr/drivers/spi.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/byteorder.h>

LOG_MODULE_REGISTER(gc9a01, CONFIG_DISPLAY_LOG_LEVEL);

/* GC9A01 Commands */
#define GC9A01_SWRESET  0x01
#define GC9A01_SLPOUT   0x11
#define GC9A01_DISPON   0x29
#define GC9A01_DISPOFF  0x28
#define GC9A01_CASET    0x2A
#define GC9A01_RASET    0x2B
#define GC9A01_RAMWR    0x2C
#define GC9A01_MADCTL   0x36
#define GC9A01_COLMOD   0x3A

struct gc9a01_config {
    struct spi_dt_spec spi;
    struct gpio_dt_spec dc_gpio;
    struct gpio_dt_spec reset_gpio;
    uint16_t width;
    uint16_t height;
    uint8_t rotation;
};

struct gc9a01_data {
    enum display_pixel_format pixel_format;
    enum display_orientation orientation;
};

static int gc9a01_transmit(const struct device *dev, uint8_t cmd, 
                          const void *data, size_t len) {
    const struct gc9a01_config *config = dev->config;
    int ret;
    
    /* Send command */
    ret = gpio_pin_set_dt(&config->dc_gpio, 0);
    if (ret < 0) return ret;
    
    struct spi_buf cmd_buf = {.buf = &cmd, .len = 1};
    struct spi_buf_set cmd_set = {.buffers = &cmd_buf, .count = 1};
    ret = spi_write_dt(&config->spi, &cmd_set);
    if (ret < 0) return ret;
    
    /* Send data if present */
    if (data && len > 0) {
        ret = gpio_pin_set_dt(&config->dc_gpio, 1);
        if (ret < 0) return ret;
        
        struct spi_buf data_buf = {.buf = (void *)data, .len = len};
        struct spi_buf_set data_set = {.buffers = &data_buf, .count = 1};
        ret = spi_write_dt(&config->spi, &data_set);
    }
    
    return ret;
}

static int gc9a01_set_window(const struct device *dev, uint16_t x1, uint16_t y1,
                            uint16_t x2, uint16_t y2) {
    uint8_t data[4];
    int ret;
    
    /* Column address */
    sys_put_be16(x1, &data[0]);
    sys_put_be16(x2, &data[2]);
    ret = gc9a01_transmit(dev, GC9A01_CASET, data, 4);
    if (ret < 0) return ret;
    
    /* Row address */
    sys_put_be16(y1, &data[0]);
    sys_put_be16(y2, &data[2]);
    ret = gc9a01_transmit(dev, GC9A01_RASET, data, 4);
    if (ret < 0) return ret;
    
    /* Memory write */
    return gc9a01_transmit(dev, GC9A01_RAMWR, NULL, 0);
}

static int gc9a01_write(const struct device *dev, const uint16_t x, const uint16_t y,
                       const struct display_buffer_descriptor *desc, const void *buf) {
    const struct gc9a01_config *config = dev->config;
    int ret;
    
    if (x + desc->width > config->width || y + desc->height > config->height) {
        LOG_ERR("Write out of bounds");
        return -EINVAL;
    }
    
    /* Set window */
    ret = gc9a01_set_window(dev, x, y, x + desc->width - 1, y + desc->height - 1);
    if (ret < 0) return ret;
    
    /* Write pixel data */
    ret = gpio_pin_set_dt(&config->dc_gpio, 1);
    if (ret < 0) return ret;
    
    struct spi_buf buf_tx = {.buf = (void *)buf, .len = desc->buf_size};
    struct spi_buf_set buf_set = {.buffers = &buf_tx, .count = 1};
    
    return spi_write_dt(&config->spi, &buf_set);
}

static int gc9a01_blanking_on(const struct device *dev) {
    return gc9a01_transmit(dev, GC9A01_DISPOFF, NULL, 0);
}

static int gc9a01_blanking_off(const struct device *dev) {
    return gc9a01_transmit(dev, GC9A01_DISPON, NULL, 0);
}

static void gc9a01_get_capabilities(const struct device *dev,
                                   struct display_capabilities *caps) {
    const struct gc9a01_config *config = dev->config;
    struct gc9a01_data *data = dev->data;
    
    memset(caps, 0, sizeof(struct display_capabilities));
    
    caps->x_resolution = config->width;
    caps->y_resolution = config->height;
    caps->supported_pixel_formats = PIXEL_FORMAT_RGB_565;
    caps->current_pixel_format = data->pixel_format;
    caps->current_orientation = data->orientation;
    caps->screen_info = SCREEN_INFO_MONO_VTILED;
}

static int gc9a01_set_pixel_format(const struct device *dev,
                                  const enum display_pixel_format format) {
    struct gc9a01_data *data = dev->data;
    
    if (format != PIXEL_FORMAT_RGB_565) {
        return -ENOTSUP;
    }
    
    data->pixel_format = format;
    return 0;
}

static int gc9a01_set_orientation(const struct device *dev,
                                 const enum display_orientation orientation) {
    struct gc9a01_data *data = dev->data;
    
    if (orientation != DISPLAY_ORIENTATION_NORMAL) {
        return -ENOTSUP;
    }
    
    data->orientation = orientation;
    return 0;
}

static const struct display_driver_api gc9a01_api = {
    .blanking_on = gc9a01_blanking_on,
    .blanking_off = gc9a01_blanking_off,
    .write = gc9a01_write,
    .get_capabilities = gc9a01_get_capabilities,
    .set_pixel_format = gc9a01_set_pixel_format,
    .set_orientation = gc9a01_set_orientation,
};

static int gc9a01_init(const struct device *dev) {
    const struct gc9a01_config *config = dev->config;
    struct gc9a01_data *data = dev->data;
    int ret;
    
    LOG_INF("Initializing GC9A01 display");
    
    if (!spi_is_ready_dt(&config->spi)) {
        LOG_ERR("SPI device not ready");
        return -ENODEV;
    }
    
    /* Configure DC GPIO */
    ret = gpio_pin_configure_dt(&config->dc_gpio, GPIO_OUTPUT_INACTIVE);
    if (ret < 0) {
        LOG_ERR("Failed to configure DC GPIO");
        return ret;
    }
    
    /* Configure Reset GPIO if present */
    if (config->reset_gpio.port) {
        ret = gpio_pin_configure_dt(&config->reset_gpio, GPIO_OUTPUT_INACTIVE);
        if (ret < 0) {
            LOG_ERR("Failed to configure reset GPIO");
            return ret;
        }
        
        /* Hardware reset */
        gpio_pin_set_dt(&config->reset_gpio, 1);
        k_msleep(10);
        gpio_pin_set_dt(&config->reset_gpio, 0);
        k_msleep(10);
        gpio_pin_set_dt(&config->reset_gpio, 1);
        k_msleep(120);
    }
    
    /* Software reset */
    ret = gc9a01_transmit(dev, GC9A01_SWRESET, NULL, 0);
    if (ret < 0) return ret;
    k_msleep(120);
    
    /* Sleep out */
    ret = gc9a01_transmit(dev, GC9A01_SLPOUT, NULL, 0);
    if (ret < 0) return ret;
    k_msleep(5);
    
    /* Pixel format: RGB565 */
    uint8_t colmod = 0x55;
    ret = gc9a01_transmit(dev, GC9A01_COLMOD, &colmod, 1);
    if (ret < 0) return ret;
    
    /* Memory access control */
    uint8_t madctl = 0x00;
    switch (config->rotation) {
    case 90:  madctl = 0x60; break;
    case 180: madctl = 0xC0; break;
    case 270: madctl = 0xA0; break;
    default:  madctl = 0x00; break;
    }
    ret = gc9a01_transmit(dev, GC9A01_MADCTL, &madctl, 1);
    if (ret < 0) return ret;
    
    /* Display on */
    ret = gc9a01_transmit(dev, GC9A01_DISPON, NULL, 0);
    if (ret < 0) return ret;
    k_msleep(100);
    
    /* Initialize data */
    data->pixel_format = PIXEL_FORMAT_RGB_565;
    data->orientation = DISPLAY_ORIENTATION_NORMAL;
    
    LOG_INF("GC9A01 initialized (%dx%d)", config->width, config->height);
    return 0;
}

#define GC9A01_INIT(n) \
    static const struct gc9a01_config gc9a01_config_##n = { \
        .spi = SPI_DT_SPEC_INST_GET(n, SPI_WORD_SET(8) | SPI_OP_MODE_MASTER, 0), \
        .dc_gpio = GPIO_DT_SPEC_INST_GET(n, dc_gpios), \
        .reset_gpio = GPIO_DT_SPEC_INST_GET_OR(n, reset_gpios, {0}), \
        .width = DT_INST_PROP(n, width), \
        .height = DT_INST_PROP(n, height), \
        .rotation = DT_INST_PROP_OR(n, rotation, 0), \
    }; \
    static struct gc9a01_data gc9a01_data_##n; \
    DEVICE_DT_INST_DEFINE(n, gc9a01_init, NULL, &gc9a01_data_##n, \
                          &gc9a01_config_##n, POST_KERNEL, \
                          CONFIG_DISPLAY_INIT_PRIORITY, &gc9a01_api);

DT_INST_FOREACH_STATUS_OKAY(GC9A01_INIT)
