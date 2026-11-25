#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/spi.h>
#include <lvgl.h>


#include <zephyr/bluetooth/services/nus.h>
#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/gap.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/conn.h>


#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(ssd135x, CONFIG_DISPLAY_LOG_LEVEL);

#include "src/resources/img_96128_header.h"

#define DEVICE_NAME		CONFIG_BT_DEVICE_NAME
#define DEVICE_NAME_LEN		(sizeof(DEVICE_NAME) - 1)

static struct bt_conn *current_conn;
static struct k_work adv_work;

static const struct bt_data ad[] = {
	BT_DATA_BYTES(BT_DATA_FLAGS, (BT_LE_AD_GENERAL | BT_LE_AD_NO_BREDR)),
	BT_DATA(BT_DATA_NAME_COMPLETE, DEVICE_NAME, DEVICE_NAME_LEN),
};

static void disp_bt_data(char* data, uint16_t len);

static const struct bt_data sd[] = {
	BT_DATA_BYTES(BT_DATA_UUID128_ALL, BT_UUID_NUS_SRV_VAL),
};

static void adv_work_handler(struct k_work *w)
{
    bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
}

static void connected(struct bt_conn *conn, uint8_t err)
{
    if (!err) {
        current_conn = bt_conn_ref(conn);
    }
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
    if (conn == current_conn) {
        bt_conn_unref(current_conn);
        current_conn = NULL;
    }
    k_work_submit(&adv_work);
}

BT_CONN_CB_DEFINE(cb) = {
    .connected = connected,
    .disconnected = disconnected,
};

static void notif_enabled(bool enabled, void *ctx)
{
	ARG_UNUSED(ctx);

	printk("%s() - %s\n", __func__, (enabled ? "Enabled" : "Disabled"));
    
}

static void received(struct bt_conn *conn, const void *data, uint16_t len, void *ctx)
{
	ARG_UNUSED(conn);
	ARG_UNUSED(ctx);

	printk("%s() - Len: %d, Message: %.*s\n", __func__, len, len, (char *)data);
    disp_bt_data((char *)data, len);
}

struct bt_nus_cb nus_listener = {
	.notif_enabled = notif_enabled,
	.received = received,
};

#if 0
/*  UART 自定义配置 Start*/
#include <zephyr/drivers/uart.h>

/* ---------- UART30 业务串口 ---------- */
static const struct device *uart30_dev;

/* 回发固定报文 */
static const uint8_t rsp[] = {0x98, 0x98};

/* 接收缓冲：一次最多收 64 字节，够用可改大 */
static uint8_t uart30_rx_buf[64];
static volatile bool uart30_tx_busy;

/* 发送完成回调：发送完就把忙标志清掉 */


/* 接收回调：每收到一帧，回发 0x98 0x98 */
static void uart30_async_cb(const struct device *dev, struct uart_event *evt,
                           void *user_data)
{
    ARG_UNUSED(dev);
    ARG_UNUSED(user_data);

    switch (evt->type) {
    case UART_TX_DONE:
        uart30_tx_busy = false;
        break;

    case UART_TX_ABORTED:
        uart30_tx_busy = false;   /* 超时也算发送结束，要释放忙标志 */
        printk("UART TX aborted, sent %d bytes\n", evt->data.tx.len);
        break;

    case UART_RX_RDY:
        if (evt->data.rx.len > 0) {
            printk("UART RX %d bytes: %.*s\n",
                   evt->data.rx.len, evt->data.rx.len,
                   (char *)evt->data.rx.buf);

            /* 没有正在进行的发送，才回发 0x98 0x98 */
            if (!uart30_tx_busy) {
                uart30_tx_busy = true;
                int rc = uart_tx(dev, rsp, sizeof(rsp), SYS_FOREVER_MS);
                if (rc) {            /* -EBUSY 等说明发送没排进去，要释放标志 */
                    printk("uart_tx failed: %d\n", rc);
                    uart30_tx_busy = false;
                }
            }
            /* 把这段缓冲交还给驱动，准备接收下一批数据 */
            uart_rx_enable(dev, uart30_rx_buf, sizeof(uart30_rx_buf), 0);
        }
        break;

    case UART_RX_DISABLED:
        /* 接收被关闭（缓冲满了、超时了等），重新打开 */
        uart_rx_enable(dev, uart30_rx_buf, sizeof(uart30_rx_buf), 0);
        break;

    case UART_RX_BUF_REQUEST:
        /* 想要连续接收，可以再提供一个缓冲；这里用单缓冲，忽略即可 */
        break;

    case UART_RX_BUF_RELEASED:
        break;

    case UART_RX_STOPPED:
        printk("UART RX stopped, reason %d\n", evt->data.rx_stop.reason);
        uart_rx_enable(dev, uart30_rx_buf, sizeof(uart30_rx_buf), 0);
        break;

    default:
        break;
    }
}

/*  UART 自定义配置 End */
#endif

/* 全局 */
//#define IMG_W 96
//#define IMG_H 128
//#define IMG_BYTES (IMG_W * IMG_H * 2)  /* 24576 */

/* 全局 */
#define IMG_W 88
#define IMG_H 112
#define IMG_BYTES (IMG_W * IMG_H * 2)  

static const struct device *spi_dev;
static const struct device *gpio1_dev;
static const struct device *gpio2_dev;

#define MLED_CS    5
#define MLED_MOSI    2
#define MLED_DC    4
#define MLED_CLK    1
#define MLED_RST    2

/* ---------- 底层 SPI ---------- */
static void write_cmd(uint8_t cmd)
{
    gpio_pin_set(gpio2_dev, MLED_DC, 0);
    gpio_pin_set(gpio2_dev, MLED_CS, 0);

    struct spi_buf buf = { .buf = &cmd, .len = 1 };
    struct spi_buf_set tx = { .buffers = &buf, .count = 1 };
    struct spi_config cfg = {                          // ← 去掉 static
        .frequency = 64000000,
        .operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_LINES_SINGLE,
        .slave = 0,
    };
    int ret = spi_write(spi_dev, &cfg, &tx);
    if (ret != 0) {
        printk("write_cmd 0x%02x failed: %d\n", cmd, ret);
    }

    gpio_pin_set(gpio2_dev, MLED_CS, 1);
}

static void write_data(uint8_t data)
{
    gpio_pin_set(gpio2_dev, MLED_DC, 1);
    gpio_pin_set(gpio2_dev, MLED_CS, 0);

    struct spi_buf buf = { .buf = &data, .len = 1 };
    struct spi_buf_set tx = { .buffers = &buf, .count = 1 };
    struct spi_config cfg = {                          // ← 去掉 static
        .frequency = 64000000,
        .operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_LINES_SINGLE,
        .slave = 0,
    };
    int ret = spi_write(spi_dev, &cfg, &tx);
    if (ret != 0) {
        printk("write_data 0x%02x failed: %d\n", data, ret);
    }

    gpio_pin_set(gpio2_dev, MLED_CS, 1);
}

static void write_data16(uint8_t d0, uint8_t d1)
{
    gpio_pin_set(gpio2_dev, MLED_DC, 1);
    gpio_pin_set(gpio2_dev, MLED_CS, 0);

    uint8_t d[2] = {d0, d1};
    struct spi_buf buf = { .buf = d, .len = 2 };
    struct spi_buf_set tx = { .buffers = &buf, .count = 1 };
    struct spi_config cfg = {
        .frequency = 64000000,
        .operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_LINES_SINGLE,
        .slave = 0,
    };
    spi_write(spi_dev, &cfg, &tx);
    gpio_pin_set(gpio2_dev, MLED_CS, 1);
}

static void set_column(uint8_t start, uint8_t end)
{
    write_cmd(0x15);
    write_data16(start, end);
}

static void set_row(uint8_t start, uint8_t end)
{
    write_cmd(0x75);
    write_data16(start, end);
}

/* ---------- 屏幕初始化 ---------- */

static void ssd1357_init(void)
{
    /* 硬件复位 */
    gpio_pin_configure(gpio1_dev, MLED_RST, GPIO_OUTPUT_HIGH);
    gpio_pin_set(gpio1_dev, MLED_RST, 0);
    k_msleep(10);
    gpio_pin_set(gpio1_dev, MLED_RST, 1);
    k_msleep(120);
#if 0 //2026-06-05
    //write_cmd(0xFD); write_data(0x12); // Unlock
    write_cmd(0xAE);                   // Display Off

    write_cmd(0x15); write_data(0x10);write_data(0x6F);
    write_cmd(0x75); write_data(0x00);write_data(0x7F);
    /* Remap: 镜像修正 + 扫描方向 */
    write_cmd(0xA0); write_data(0x74);write_data(0x00);

    write_cmd(0xA1); write_data(0x00); // Start line
    write_cmd(0xA2); write_data(0x00); // Offset

    write_cmd(0xA6); 

    write_cmd(0xB1); write_data(0xF4); //Phase 1 and 2 setting. It may need to fine tune based on panel performanc
    write_cmd(0xB3); write_data(0x20); // Clock

    write_cmd(0xB9); //Default linear LUT

    write_cmd(0xBB); write_data(0x1E); //VP voltage level setting. It may need to fine tune based on panel performance.
    write_cmd(0xBE); write_data(0x07);//VCOMH voltage level setting

    write_cmd(0xC1); 
    write_data(0x7F);
    write_data(0x7F);
    write_data(0x7F);

    write_cmd(0xC7); write_data(0x0F);
    write_cmd(0xCA); write_data(0x7F); // MUX = 128

    /* 列窗口偏移 0x20，让 64 列数据正好映射到物理屏幕的中间 */
    //set_column(0x20, 0x5F);
    //set_row(0x00, 0x7F);

    write_cmd(0xAF); // Display On
    #endif
#if 1 //2026-10-16
    write_cmd(0xAE);                   // Display Off

    write_cmd(0x15); write_data(0x14);write_data(0x6B);
    write_cmd(0x75); write_data(0x00);write_data(0x6F);

    /* Remap: 镜像修正 + 扫描方向 */
    //write_cmd(0xA0); write_data(0x60);write_data(0x00);
    write_cmd(0xA0); write_data(0b01110100);write_data(0b00000000);

    write_cmd(0xA1); write_data(0); // Start line
    //write_cmd(0xA2); write_data(0x00); // Offset
    write_cmd(0xA2); write_data(112); // Offset

    write_cmd(0xA6); 

    write_cmd(0xB1); write_data(0x84); //Phase 1 and 2 setting. It may need to fine tune based on panel performanc
    write_cmd(0xB3); write_data(0x00); // Clock

    write_cmd(0xB9); //Default linear LUT

    write_cmd(0xBB); write_data(0x1E); //VP voltage level setting. It may need to fine tune based on panel performance.
    write_cmd(0xBE); write_data(0x05);//VCOMH voltage level setting

    write_cmd(0xC1); 
    write_data(0x7F);
    write_data(0x7F);
    write_data(0x7F);

    write_cmd(0xC7); write_data(0x0F);
    write_cmd(0xCA); write_data(0x6F); // MUX = 128

    //write_cmd(0xA5); // 全亮
    //write_cmd(0xA4); // 全黑
    write_cmd(0xAF); // Display On
#endif
}

/* ---------- 填充颜色 ---------- */

#define COLOR_RED    0xF800
#define COLOR_GREEN  0x07E0
#define COLOR_BLUE   0x001F
#define COLOR_WHITE  0xFFFF

static void fill_color(uint16_t color)
{
    set_column(0x14, 0x6B);
    set_row(0x00, 0x6F);
    write_cmd(0x5C);

    gpio_pin_set(gpio2_dev, MLED_DC, 1);
    gpio_pin_set(gpio2_dev, MLED_CS, 0);

    uint8_t data[2] = {
        (color >> 8) & 0xFF,
        color & 0xFF
    };

    struct spi_buf buf = { .buf = data, .len = 2 };
    struct spi_buf_set tx = { .buffers = &buf, .count = 1 };

    static struct spi_config cfg = {
        .frequency = 64000000,
        .operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_LINES_SINGLE,
        .slave = 0,
    };

    for (int i = 0; i < IMG_W * IMG_H; i++) {
        spi_write(spi_dev, &cfg, &tx);
    }

    gpio_pin_set(gpio2_dev, MLED_CS, 1);
}

static uint8_t color_index = 0;

static void color_timer_expiry(struct k_timer *timer)
{
    const uint16_t colors[4] = {
        COLOR_RED,
        COLOR_GREEN,
        COLOR_BLUE,
        COLOR_WHITE
    };

    fill_color(colors[color_index]);
    color_index = (color_index + 1) % 4;
}

static K_TIMER_DEFINE(color_timer, color_timer_expiry, NULL);

/* ---------- 填充颜色 ---------- */


/* ---------- 清屏 ---------- */
static void clear_screen(void)
{
    /* 窗口 = IMG_W × IMG_H = 88 × 128，居中 */
    set_column(0x14, 0x6B);     // 0x14 ~ 0x6B (88列)
    set_row(0x00, 0x6F);        // 0x00 ~ 0x6F (112行)
    write_cmd(0x5C);

    write_cmd(0x25);  // Clear Window
    write_data(0x14); // start column
    write_data(0x00); // start row  
    write_data(0x6B); // end column
    write_data(0x6F); // end row

    gpio_pin_set(gpio2_dev, MLED_DC, 1);
    gpio_pin_set(gpio2_dev, MLED_CS, 0);

    uint8_t black[2] = {0x00, 0x00};
    struct spi_buf buf = { .buf = black, .len = 2 };
    struct spi_buf_set tx = { .buffers = &buf, .count = 1 };
    struct spi_config cfg = {
        .frequency = 64000000,
        .operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_LINES_SINGLE,
        .slave = 0,
    };

    for (int i = 0; i < IMG_W * IMG_H; i++) {
        spi_write(spi_dev, &cfg, &tx);
    }

    gpio_pin_set(gpio2_dev, MLED_CS, 1);
}

/* ---------- LVGL flush ---------- */
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map)
{
    set_column(area->x1 + 0x14, area->x2 + 0x14);
    set_row(area->y1 , area->y2 );
    write_cmd(0x5C);

    printk("flush: (%d,%d)-(%d,%d)\n", area->x1, area->y1, area->x2, area->y2);

    gpio_pin_set(gpio2_dev, MLED_DC, 1);
    gpio_pin_set(gpio2_dev, MLED_CS, 0);

    int w = area->x2 - area->x1 + 1;
    int h = area->y2 - area->y1 + 1;
    struct spi_buf buf = { .buf = px_map, .len = w * h * 2 };
    struct spi_buf_set tx = { .buffers = &buf, .count = 1 };
    static struct spi_config cfg = {
        .frequency = 64000000,
        .operation = SPI_OP_MODE_MASTER | SPI_WORD_SET(8) | SPI_LINES_SINGLE,
        .slave = 0,
    };
    spi_write(spi_dev, &cfg, &tx);

    gpio_pin_set(gpio2_dev, MLED_CS, 1);

    lv_display_flush_ready(disp);
    
    printk("Screen cleared, starting LVGL...\n");
}


#include <zephyr/kernel.h>

static lv_obj_t *img, *label;
static uint8_t img_index = 0;
static lv_display_t *disp;
static bool img_pending = false;

static const lv_image_dsc_t *imgs[5] = {
    &img_88112_261123_youwe,
    &img_88112_261123_youwe2,
    &img_88112_261123_tel,
    &img_88112_261123_carolie,
    &img_88112_261123_hr
};

static void img_switch_work(struct k_work *work)
{
    img_index = ((img_index + 1) % ARRAY_SIZE(imgs));
    lv_image_set_src(img, imgs[img_index]);    
    //img_pending = true;
    lv_refr_now(disp); 
    printk("img_index:%d\n", img_index);
}

static void disp_bt_data(char* data, uint16_t len)
{
    label = lv_label_create(lv_screen_active());
    lv_label_set_text(label,(char *)data);
    lv_obj_set_style_text_color(label, lv_color_hex(0x00FF00), 0); 
    lv_obj_set_style_text_font(label, &lv_font_montserrat_8, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);
}

static K_WORK_DEFINE(img_switch_work_item, img_switch_work);

static void img_timer_expiry(struct k_timer *timer)
{
    k_work_submit(&img_switch_work_item);
}

static K_TIMER_DEFINE(img_timer, img_timer_expiry, NULL);

static void bt_ready(int err)
{
    if (err) {
        printk("BT init failed: %d\n", err);
        return;
    }
    printk("BT ready\n");
    int r = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
    printk("adv_start ret=%d\n", r);
}

int main(void)
{
	int err;

	printk("Sample - Bluetooth Peripheral NUS\n");

	err = bt_nus_cb_register(&nus_listener, NULL);
	if (err) {
		printk("Failed to register NUS callback: %d\n", err);
		return err;
	}

	err = bt_enable(bt_ready);
	if (err) {
		printk("Failed to enable bluetooth: %d\n", err);
		return err;
	}
/*
	err = bt_le_adv_start(BT_LE_ADV_CONN_FAST_1, ad, ARRAY_SIZE(ad), sd, ARRAY_SIZE(sd));
	if (err) {
		printk("Failed to start advertising: %d\n", err);
		return err;
	}
*/
    k_work_init(&adv_work, adv_work_handler);

    spi_dev = DEVICE_DT_GET(DT_NODELABEL(spi00));
    gpio2_dev = DEVICE_DT_GET(DT_NODELABEL(gpio2));
    gpio1_dev = DEVICE_DT_GET(DT_NODELABEL(gpio1));

    if (!device_is_ready(spi_dev) || !device_is_ready(gpio2_dev) || !device_is_ready(gpio1_dev)) {
        printk("FATAL: device not ready!\n");
        return 0;
    }

    gpio_pin_configure(gpio2_dev, MLED_CS, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(gpio2_dev, MLED_DC, GPIO_OUTPUT_HIGH);
    gpio_pin_configure(gpio1_dev, MLED_RST, GPIO_OUTPUT_HIGH);

    gpio_pin_configure(gpio1_dev, 7, GPIO_INPUT | GPIO_PULL_UP);
    gpio_pin_set(gpio1_dev, 7, 1);

    gpio_pin_configure(gpio1_dev, 9, GPIO_OUTPUT_HIGH);
    gpio_pin_set(gpio1_dev, 9, 1);

    gpio_pin_configure(gpio1_dev, 8, GPIO_OUTPUT_HIGH);
    gpio_pin_set(gpio1_dev, 8, 1);

    printk("Starting SSD1357 init...\n");
    ssd1357_init();
    printk("SSD1357 init done, clearing screen...\n");
    clear_screen();
    printk("Screen cleared, starting LVGL...\n");
#if 1
    /* ---------- LVGL 初始化（只一次） ---------- */
    lv_init();

    disp = lv_display_create(IMG_W, IMG_H);
    //lv_display_set_rotation(disp, LV_DISPLAY_ROTATION_0);
    static uint8_t draw_buf[IMG_W * IMG_H * 2] __aligned(4);    
    memset(draw_buf, 0x00, sizeof(draw_buf));

    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf),
                           LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, lvgl_flush_cb);

#if 0
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
    label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "LDSK");
    lv_obj_set_style_text_color(label, lv_color_hex(0x00FF00), 0); 
    lv_obj_set_style_text_font(label, &lv_font_montserrat_8, 0);
    lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 0);

    k_msleep(3000);

#endif
    img = lv_image_create(lv_screen_active());
    lv_image_set_src(img, imgs[0]);
    lv_obj_align(img, LV_ALIGN_TOP_MID, 0, 0);
    
/* ========== 首页三色引导 ========== */
    printk("Boot sequence: Red -> Green -> Blue\n");
    
    fill_color(COLOR_RED);
    k_msleep(1000);
    fill_color(COLOR_GREEN);
    k_msleep(1000);
    fill_color(COLOR_BLUE);
    k_msleep(1000);
    clear_screen();   // 清掉蓝色，避免和 LVGL 画面叠加
    /* ========== 引导结束 ========== */

    uint32_t counter = 0, index_ds = 0;
    k_timer_start(&img_timer, K_SECONDS(3), K_SECONDS(3));  // 每 3 秒触发
    //k_timer_start(&img_timer, K_MSEC(10), K_MSEC(10));  // 每 300 毫秒触发
#endif

    //fill_color(COLOR_RED);   // 上电先显示红色
    //k_timer_start(&color_timer, K_SECONDS(1), K_SECONDS(1));

    while (1) {
        lv_timer_handler();
        k_msleep(10);

    }
}
