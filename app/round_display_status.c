/*
 * Copyright (c) 2024 ZMK Community  
 * SPDX-License-Identifier: MIT
 *
 * ZMK Round Display Status Module
 * 
 * Production-ready custom display module for Seeed XIAO Round Display
 * Features:
 * - "Hello World" centered display
 * - Circular battery indicator with color coding
 * - Bluetooth connection status with icon
 * - Real-time updates on state changes
 * - Optimized for 240x240 circular display
 * - Modular widget system for easy customization
 */

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/display.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>

#include <lvgl.h>

#include <zmk/display.h>
#include <zmk/events/battery_state_changed.h>
#include <zmk/events/ble_active_profile_changed.h>
#include <zmk/events/endpoint_changed.h>
#include <zmk/battery.h>
#include <zmk/ble.h>
#include <zmk/endpoints.h>
#include <zmk/event_manager.h>

LOG_MODULE_REGISTER(round_display_status, CONFIG_ZMK_LOG_LEVEL);

// Display dimensions
#define DISPLAY_WIDTH  240
#define DISPLAY_HEIGHT 240
#define CENTER_X       (DISPLAY_WIDTH / 2)
#define CENTER_Y       (DISPLAY_HEIGHT / 2)

// Color definitions (RGB565)
#define COLOR_BLACK     LV_COLOR_MAKE(0x00, 0x00, 0x00)
#define COLOR_WHITE     LV_COLOR_MAKE(0xFF, 0xFF, 0xFF)
#define COLOR_RED       LV_COLOR_MAKE(0xFF, 0x00, 0x00)
#define COLOR_GREEN     LV_COLOR_MAKE(0x00, 0xFF, 0x00)
#define COLOR_BLUE      LV_COLOR_MAKE(0x00, 0x80, 0xFF)
#define COLOR_YELLOW    LV_COLOR_MAKE(0xFF, 0xFF, 0x00)
#define COLOR_CYAN      LV_COLOR_MAKE(0x00, 0xFF, 0xFF)
#define COLOR_GRAY      LV_COLOR_MAKE(0x80, 0x80, 0x80)
#define COLOR_DARK_GRAY LV_COLOR_MAKE(0x40, 0x40, 0x40)

// Widget references
static lv_obj_t *screen;
static lv_obj_t *hello_label;
static lv_obj_t *world_label;
static lv_obj_t *battery_arc;
static lv_obj_t *battery_label;
static lv_obj_t *ble_icon;
static lv_obj_t *status_label;
static lv_obj_t *border_arc;

// Update work
static struct k_work_delayable status_update_work;
static struct k_work_delayable screen_refresh_work;

/* Get battery level with bounds checking */
static int get_battery_level(void)
{
    int level = zmk_battery_state_of_charge();
    if (level < 0) {
        return 0; // Unknown battery level
    }
    return CLAMP(level, 0, 100);
}

/* Get battery color based on level */
static lv_color_t get_battery_color(int level)
{
    if (level < 20) return COLOR_RED;
    if (level < 50) return COLOR_YELLOW;
    return COLOR_GREEN;
}

/* Check if BLE is connected */
static bool is_ble_connected(void)
{
    return zmk_ble_active_profile_is_connected();
}

/* Update battery indicator widget */
static void update_battery_widget(void)
{
    int battery_level = get_battery_level();
    lv_color_t battery_color = get_battery_color(battery_level);
    
    // Update arc (circular battery indicator)
    lv_arc_set_value(battery_arc, battery_level);
    lv_obj_set_style_arc_color(battery_arc, battery_color, LV_PART_INDICATOR);
    
    // Update percentage text
    lv_label_set_text_fmt(battery_label, "%d%%", battery_level);
    lv_obj_set_style_text_color(battery_label, battery_color, 0);
    
    LOG_DBG("Battery widget updated: %d%%", battery_level);
}

/* Update Bluetooth status widget */
static void update_ble_widget(void)
{
    bool connected = is_ble_connected();
    lv_color_t ble_color = connected ? COLOR_BLUE : COLOR_GRAY;
    
    // Update BLE icon (simple filled circle for now)
    lv_obj_set_style_bg_color(ble_icon, ble_color, 0);
    
    // Update status text
    const char *status_text = connected ? "CONNECTED" : "DISCONNECTED";
    lv_color_t status_color = connected ? COLOR_GREEN : COLOR_RED;
    
    lv_label_set_text(status_label, status_text);
    lv_obj_set_style_text_color(status_label, status_color, 0);
    
    LOG_DBG("BLE widget updated: %s", status_text);
}

/* Update main display content */
static void update_display_content(void)
{
    update_battery_widget();
    update_ble_widget();
    
    LOG_DBG("Display content updated");
}

/* Create battery indicator widget */
static void create_battery_widget(lv_obj_t *parent)
{
    // Create battery arc indicator (top-left area)
    battery_arc = lv_arc_create(parent);
    lv_obj_set_size(battery_arc, 60, 60);
    lv_obj_set_pos(battery_arc, 20, 20);
    lv_arc_set_range(battery_arc, 0, 100);
    lv_arc_set_bg_angles(battery_arc, 0, 360);
    lv_arc_set_angles(battery_arc, 270, 270); // Start from top
    lv_obj_set_style_arc_width(battery_arc, 8, LV_PART_MAIN);
    lv_obj_set_style_arc_width(battery_arc, 8, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(battery_arc, COLOR_DARK_GRAY, LV_PART_MAIN);
    lv_obj_remove_style(battery_arc, NULL, LV_PART_KNOB); // Remove knob
    
    // Create battery percentage label
    battery_label = lv_label_create(parent);
    lv_obj_set_pos(battery_label, 30, 90);
    lv_obj_set_style_text_font(battery_label, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(battery_label, COLOR_WHITE, 0);
}

/* Create BLE status widget */
static void create_ble_widget(lv_obj_t *parent)
{
    // Create BLE icon (simple circle in top-right)
    ble_icon = lv_obj_create(parent);
    lv_obj_set_size(ble_icon, 16, 16);
    lv_obj_set_pos(ble_icon, DISPLAY_WIDTH - 30, 20);
    lv_obj_set_style_radius(ble_icon, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(ble_icon, 0, 0);
    lv_obj_set_style_bg_opa(ble_icon, LV_OPA_COVER, 0);
}

/* Create main "Hello World" display */
static void create_hello_world_widget(lv_obj_t *parent)
{
    // Create "HELLO" label
    hello_label = lv_label_create(parent);
    lv_label_set_text(hello_label, "HELLO");
    lv_obj_set_style_text_font(hello_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(hello_label, COLOR_WHITE, 0);
    lv_obj_center(hello_label);
    lv_obj_set_pos(hello_label, CENTER_X - 35, CENTER_Y - 30);
    
    // Create "WORLD" label  
    world_label = lv_label_create(parent);
    lv_label_set_text(world_label, "WORLD");
    lv_obj_set_style_text_font(world_label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(world_label, COLOR_WHITE, 0);
    lv_obj_center(world_label);
    lv_obj_set_pos(world_label, CENTER_X - 35, CENTER_Y + 5);
}

/* Create status text widget */
static void create_status_widget(lv_obj_t *parent)
{
    status_label = lv_label_create(parent);
    lv_obj_set_pos(status_label, CENTER_X - 40, DISPLAY_HEIGHT - 40);
    lv_obj_set_style_text_font(status_label, &lv_font_montserrat_10, 0);
}

/* Create decorative border */
static void create_border_widget(lv_obj_t *parent)
{
    border_arc = lv_arc_create(parent);
    lv_obj_set_size(border_arc, DISPLAY_WIDTH - 10, DISPLAY_HEIGHT - 10);
    lv_obj_center(border_arc);
    lv_arc_set_range(border_arc, 0, 100);
    lv_arc_set_value(border_arc, 100);
    lv_arc_set_bg_angles(border_arc, 0, 360);
    lv_arc_set_angles(border_arc, 0, 360);
    lv_obj_set_style_arc_width(border_arc, 2, LV_PART_MAIN);
    lv_obj_set_style_arc_width(border_arc, 2, LV_PART_INDICATOR);
    lv_obj_set_style_arc_color(border_arc, COLOR_CYAN, LV_PART_MAIN);
    lv_obj_set_style_arc_color(border_arc, COLOR_CYAN, LV_PART_INDICATOR);
    lv_obj_remove_style(border_arc, NULL, LV_PART_KNOB);
}

/* Initialize the custom display screen */
static void init_round_display_screen(void)
{
    // Create main screen
    screen = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(screen, COLOR_BLACK, 0);
    
    // Create all widgets
    create_border_widget(screen);
    create_hello_world_widget(screen);
    create_battery_widget(screen);
    create_ble_widget(screen);
    create_status_widget(screen);
    
    // Load the screen
    lv_scr_load(screen);
    
    // Initial content update
    update_display_content();
    
    LOG_INF("Round display screen initialized");
}

/* Work handler for status updates */
static void status_update_work_handler(struct k_work *work)
{
    update_display_content();
    
    // Schedule next update in 5 seconds
    k_work_schedule(&status_update_work, K_SECONDS(5));
}

/* Work handler for screen refresh */
static void screen_refresh_work_handler(struct k_work *work)
{
    lv_task_handler();
}

/* ZMK event listener for immediate updates */
static int round_display_event_listener(const zmk_event_t *eh)
{
    // Trigger immediate update on relevant events
    k_work_cancel_delayable(&status_update_work);
    k_work_schedule(&status_update_work, K_NO_WAIT);
    return ZMK_EV_EVENT_BUBBLE;
}

/* Register event listeners */
ZMK_LISTENER(round_display_status_listener, round_display_event_listener);
ZMK_SUBSCRIPTION(round_display_status_listener, zmk_battery_state_changed);
ZMK_SUBSCRIPTION(round_display_status_listener, zmk_ble_active_profile_changed);
ZMK_SUBSCRIPTION(round_display_status_listener, zmk_endpoint_changed);

/* Module initialization */
static int round_display_status_init(void)
{
    const struct device *display_dev = DEVICE_DT_GET(DT_CHOSEN(zephyr_display));
    
    if (!device_is_ready(display_dev)) {
        LOG_ERR("Display device not ready");
        return -ENODEV;
    }
    
    LOG_INF("Round display status module initializing...");
    
    // Initialize LVGL screen
    init_round_display_screen();
    
    // Initialize work handlers
    k_work_init_delayable(&status_update_work, status_update_work_handler);
    k_work_init_delayable(&screen_refresh_work, screen_refresh_work_handler);
    
    // Start periodic updates
    k_work_schedule(&status_update_work, K_SECONDS(1));
    k_work_schedule(&screen_refresh_work, K_MSEC(50)); // 20 FPS refresh
    
    LOG_INF("Round display status module initialized successfully");
    return 0;
}

/* Conditional compilation based on configuration */
#if IS_ENABLED(CONFIG_ROUND_DISPLAY_STATUS)
SYS_INIT(round_display_status_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
#endif