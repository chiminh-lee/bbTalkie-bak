#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32-spi-ssd1327.h"

// Helper macro for boundary checking
#define BOUNDS_CHECK(x, y) ((x) < SSD1327_WIDTH && (y) < SSD1327_HEIGHT)

void spi_oled_init(struct spi_ssd1327 *spi_ssd1327)
{
    spi_oled_reset(spi_ssd1327);

    /* Turn the display off (datasheet p. 49) */
    spi_oled_send_cmd(spi_ssd1327, 0xAE);

    /* Set Column Bounds */
    spi_oled_send_cmd(spi_ssd1327, 0x15);
    spi_oled_send_cmd(spi_ssd1327, 0x00);
    spi_oled_send_cmd(spi_ssd1327, 0x7F);

    /* Set Row Bounds */
    spi_oled_send_cmd(spi_ssd1327, 0x75);
    spi_oled_send_cmd(spi_ssd1327, 0x00);
    spi_oled_send_cmd(spi_ssd1327, 0x7F);

    /* Set Contrast Control (Effectively brightness control) */
    spi_oled_send_cmd_arg(spi_ssd1327, 0x81, 0xD4);

    /* Set Re-map: (Tell device how to handle data writes) */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xA0, 0x51);

    /* Set Display Start Line: 0 */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xA1, 0x7F);

    /* Set Display Offset: 0 */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xA2, 0x00);

    /* Set the oscillator frequency and the front clock divider ratio */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xB3, 0x51);

    /* Set phase 2 period length and the phase 1 period length */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xB1, 0x11);

    /* Set phase 3 period length */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xB6, 0x00);

    /* Enable/Disable Vdd regulator */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xAB, 0x01);

    /* Set Vcomh voltage */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xBE, 0x07);

    /* Set pre-charge voltage (phase 2) */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xBC, 0x05);

    /* Enable/disable second pre-charge, select external/internal VSL */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xD5, 0x62);

    /* Turn the display on */
    spi_oled_send_cmd(spi_ssd1327, 0xAF);

    // Initialize frame buffer
    spi_oled_framebuffer_init(spi_ssd1327);
    spi_ssd1327->auto_refresh = true;  // Default to auto refresh
    spi_ssd1327->display_mutex = xSemaphoreCreateMutex();
}

void spi_oled_deinit(struct spi_ssd1327 *spi_ssd1327)
{
    /* Clear the display buffer to prevent burn-in */
    spi_oled_send_cmd(spi_ssd1327, 0xA5);

    /* Turn the display off */
    spi_oled_send_cmd(spi_ssd1327, 0xAE);

    /* Disable Vdd regulator to save power */
    spi_oled_send_cmd_arg(spi_ssd1327, 0xAB, 0x00);

    /* Set display to sleep mode by setting all voltages to minimum */
    spi_oled_send_cmd_arg(spi_ssd1327, 0x81, 0x00);
    spi_oled_send_cmd_arg(spi_ssd1327, 0xBE, 0x00);
    spi_oled_send_cmd_arg(spi_ssd1327, 0xBC, 0x00);
    spi_oled_send_cmd_arg(spi_ssd1327, 0xD5, 0x60);
    spi_oled_send_cmd_arg(spi_ssd1327, 0xB3, 0x00);
    spi_oled_send_cmd_arg(spi_ssd1327, 0xB1, 0x11);
    spi_oled_send_cmd_arg(spi_ssd1327, 0xB6, 0x00);

    // Free frame buffer
    spi_oled_framebuffer_free(spi_ssd1327);
}

void spi_oled_reset(struct spi_ssd1327 *spi_ssd1327)
{
    gpio_set_level(spi_ssd1327->rst_pin_num, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(spi_ssd1327->rst_pin_num, 0);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    gpio_set_level(spi_ssd1327->rst_pin_num, 1);
    vTaskDelay(100 / portTICK_PERIOD_MS);
}

void spi_oled_send_cmd(struct spi_ssd1327 *spi_ssd1327, uint8_t cmd)
{
    spi_device_acquire_bus(*(spi_ssd1327->spi_handle), portMAX_DELAY);
    gpio_set_level(spi_ssd1327->dc_pin_num, 0);

    spi_transaction_t transaction;
    memset(&transaction, 0, sizeof(transaction));
    transaction.length = 8;
    transaction.tx_buffer = &cmd;

    ESP_ERROR_CHECK(spi_device_transmit(*(spi_ssd1327->spi_handle), &transaction));
    spi_device_release_bus(*(spi_ssd1327->spi_handle));
}

void spi_oled_send_cmd_arg(struct spi_ssd1327 *spi_ssd1327, uint8_t cmd, uint8_t arg)
{
    spi_device_acquire_bus(*(spi_ssd1327->spi_handle), portMAX_DELAY);
    gpio_set_level(spi_ssd1327->dc_pin_num, 0);

    uint8_t cmdarg[2] = {cmd, arg};

    spi_transaction_t transaction;
    memset(&transaction, 0, sizeof(transaction));
    transaction.length = 16;
    transaction.tx_buffer = &cmdarg[0];

    ESP_ERROR_CHECK(spi_device_transmit(*(spi_ssd1327->spi_handle), &transaction));
    spi_device_release_bus(*(spi_ssd1327->spi_handle));
}

void spi_oled_send_data(struct spi_ssd1327 *spi_ssd1327, void *data, uint32_t data_len_bits)
{
    uint8_t *data_ptr = (uint8_t *)data;
    uint32_t bits_remaining = data_len_bits;

    spi_device_acquire_bus(*(spi_ssd1327->spi_handle), portMAX_DELAY);
    gpio_set_level(spi_ssd1327->dc_pin_num, 1);

    while (bits_remaining > 0) {
        uint32_t bits_to_send = (bits_remaining > MAX_BITS_PER_TRANSFER) ? MAX_BITS_PER_TRANSFER : bits_remaining;

        spi_transaction_t transaction = {
            .length = bits_to_send,
            .tx_buffer = data_ptr,
        };

        ESP_ERROR_CHECK(spi_device_transmit(*(spi_ssd1327->spi_handle), &transaction));

        uint32_t bytes_sent = (bits_to_send + 7) / 8;
        data_ptr += bytes_sent;
        bits_remaining -= bits_to_send;
    }

    spi_device_release_bus(*(spi_ssd1327->spi_handle));
}


// Frame buffer management functions
bool spi_oled_framebuffer_init(struct spi_ssd1327 *spi_ssd1327)
{
    if (spi_ssd1327->framebuffer) {
        free(spi_ssd1327->framebuffer);
    }
    
    spi_ssd1327->framebuffer = malloc(SSD1327_BUFFER_SIZE);
    if (!spi_ssd1327->framebuffer) {
        return false;
    }
    
    // Clear the frame buffer
    memset(spi_ssd1327->framebuffer, 0x00, SSD1327_BUFFER_SIZE);
    return true;
}

void spi_oled_framebuffer_free(struct spi_ssd1327 *spi_ssd1327)
{
    if (spi_ssd1327->framebuffer) {
        free(spi_ssd1327->framebuffer);
        spi_ssd1327->framebuffer = NULL;
    }
}

void spi_oled_framebuffer_clear(struct spi_ssd1327 *spi_ssd1327, ssd1327_gs_t color)
{
    if (!spi_ssd1327->framebuffer) return;
    
    uint8_t pixel_byte = (color << 4) | color;  // Both pixels same color
    memset(spi_ssd1327->framebuffer, pixel_byte, SSD1327_BUFFER_SIZE);
    
    if (spi_ssd1327->auto_refresh) {
        spi_oled_framebuffer_refresh(spi_ssd1327);
    }
}

void spi_oled_framebuffer_refresh(struct spi_ssd1327 *spi_ssd1327)
{
    if (!spi_ssd1327->framebuffer || !spi_ssd1327->display_mutex) return;
    
    // Take mutex with timeout to prevent deadlock
    if (xSemaphoreTake(spi_ssd1327->display_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        
        // Set full screen address window
        spi_oled_send_cmd(spi_ssd1327, 0x15);  // Set Column Address
        spi_oled_send_cmd(spi_ssd1327, 0x00);  // Column start
        spi_oled_send_cmd(spi_ssd1327, 0x3F);  // Column end (128/2 - 1 = 63)
        
        spi_oled_send_cmd(spi_ssd1327, 0x75);  // Set Row Address
        spi_oled_send_cmd(spi_ssd1327, 0x00);  // Row start
        spi_oled_send_cmd(spi_ssd1327, 0x7F);  // Row end (128 - 1 = 127)
        
        // Send entire frame buffer
        spi_oled_send_data(spi_ssd1327, spi_ssd1327->framebuffer, SSD1327_BUFFER_SIZE * 8);
        
        // Release mutex
        xSemaphoreGive(spi_ssd1327->display_mutex);
    }
}

void spi_oled_framebuffer_refresh_region(struct spi_ssd1327 *spi_ssd1327, 
                                       uint8_t x, uint8_t y, 
                                       uint8_t width, uint8_t height)
{
    if (!spi_ssd1327->framebuffer || !spi_ssd1327->display_mutex) return;
    
    // Boundary checks
    if (x >= SSD1327_WIDTH || y >= SSD1327_HEIGHT) return;
    if (x + width > SSD1327_WIDTH) width = SSD1327_WIDTH - x;
    if (y + height > SSD1327_HEIGHT) height = SSD1327_HEIGHT - y;
    
    // Take mutex with timeout
    if (xSemaphoreTake(spi_ssd1327->display_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        
        uint8_t start_col = x / 2;
        uint8_t end_col = (x + width - 1) / 2;
        
        spi_oled_send_cmd(spi_ssd1327, 0x15);
        spi_oled_send_cmd(spi_ssd1327, start_col);
        spi_oled_send_cmd(spi_ssd1327, end_col);
        
        spi_oled_send_cmd(spi_ssd1327, 0x75);
        spi_oled_send_cmd(spi_ssd1327, y);
        spi_oled_send_cmd(spi_ssd1327, y + height - 1);
        
        // Send region data row by row
        uint16_t bytes_per_row = end_col - start_col + 1;
        for (uint8_t row = 0; row < height; row++) {
            uint16_t offset = ((y + row) * (SSD1327_WIDTH / 2)) + start_col;
            spi_oled_send_data(spi_ssd1327, &spi_ssd1327->framebuffer[offset], bytes_per_row * 8);
        }
        
        // Release mutex
        xSemaphoreGive(spi_ssd1327->display_mutex);
    }
}

// Pixel manipulation functions
void spi_oled_set_pixel(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y, ssd1327_gs_t gs)
{
    if (!spi_ssd1327->framebuffer || !BOUNDS_CHECK(x, y)) return;
    
    uint16_t byte_idx = (y * (SSD1327_WIDTH / 2)) + (x / 2);
    uint8_t pixel_pos = x % 2;
    
    if (pixel_pos == 0) {
        // Left pixel (high 4 bits)
        spi_ssd1327->framebuffer[byte_idx] = (spi_ssd1327->framebuffer[byte_idx] & 0x0F) | (gs << 4);
    } else {
        // Right pixel (low 4 bits)
        spi_ssd1327->framebuffer[byte_idx] = (spi_ssd1327->framebuffer[byte_idx] & 0xF0) | gs;
    }
}

ssd1327_gs_t spi_oled_get_pixel(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y)
{
    if (!spi_ssd1327->framebuffer || !BOUNDS_CHECK(x, y)) return 0;
    
    uint16_t byte_idx = (y * (SSD1327_WIDTH / 2)) + (x / 2);
    uint8_t pixel_pos = x % 2;
    
    if (pixel_pos == 0) {
        return (spi_ssd1327->framebuffer[byte_idx] >> 4) & 0x0F;
    } else {
        return spi_ssd1327->framebuffer[byte_idx] & 0x0F;
    }
}

void spi_oled_clear_region(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y, 
                         uint8_t width, uint8_t height)
{
    for (uint8_t dy = 0; dy < height; dy++) {
        for (uint8_t dx = 0; dx < width; dx++) {
            spi_oled_set_pixel(spi_ssd1327, x + dx, y + dy, 0);
        }
    }
}

// Drawing functions
void spi_oled_draw_square(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y, 
                         uint8_t width, uint8_t height, ssd1327_gs_t gs)
{
    for (uint8_t dy = 0; dy < height; dy++) {
        for (uint8_t dx = 0; dx < width; dx++) {
            spi_oled_set_pixel(spi_ssd1327, x + dx, y + dy, gs);
        }
    }
    
    if (spi_ssd1327->auto_refresh) {
        spi_oled_framebuffer_refresh_region(spi_ssd1327, x, y, width, height);
    }
}

void spi_oled_draw_circle(struct spi_ssd1327 *spi_ssd1327, uint8_t cx, uint8_t cy, 
                         uint8_t radius, ssd1327_gs_t gs)
{
    int16_t x = radius;
    int16_t y = 0;
    int16_t decision = 1 - x;
    
    while (y <= x) {
        // Draw 8 octants
        spi_oled_set_pixel(spi_ssd1327, cx + x, cy + y, gs);
        spi_oled_set_pixel(spi_ssd1327, cx + y, cy + x, gs);
        spi_oled_set_pixel(spi_ssd1327, cx - y, cy + x, gs);
        spi_oled_set_pixel(spi_ssd1327, cx - x, cy + y, gs);
        spi_oled_set_pixel(spi_ssd1327, cx - x, cy - y, gs);
        spi_oled_set_pixel(spi_ssd1327, cx - y, cy - x, gs);
        spi_oled_set_pixel(spi_ssd1327, cx + y, cy - x, gs);
        spi_oled_set_pixel(spi_ssd1327, cx + x, cy - y, gs);
        
        y++;
        if (decision <= 0) {
            decision += 2 * y + 1;
        } else {
            x--;
            decision += 2 * (y - x) + 1;
        }
    }
    
    if (spi_ssd1327->auto_refresh) {
        uint8_t refresh_size = radius * 2 + 1;
        spi_oled_framebuffer_refresh_region(spi_ssd1327, 
                                          cx - radius, cy - radius, 
                                          refresh_size, refresh_size);
    }
}

void spi_oled_draw_line(struct spi_ssd1327 *spi_ssd1327, uint8_t x0, uint8_t y0, 
                       uint8_t x1, uint8_t y1, ssd1327_gs_t gs)
{
    int16_t dx = abs(x1 - x0);
    int16_t dy = abs(y1 - y0);
    int16_t sx = (x0 < x1) ? 1 : -1;
    int16_t sy = (y0 < y1) ? 1 : -1;
    int16_t err = dx - dy;
    
    int16_t x = x0, y = y0;
    
    while (true) {
        spi_oled_set_pixel(spi_ssd1327, x, y, gs);
        
        if (x == x1 && y == y1) break;
        
        int16_t e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
    
    if (spi_ssd1327->auto_refresh) {
        uint8_t min_x = (x0 < x1) ? x0 : x1;
        uint8_t min_y = (y0 < y1) ? y0 : y1;
        uint8_t width = abs(x1 - x0) + 1;
        uint8_t height = abs(y1 - y0) + 1;
        spi_oled_framebuffer_refresh_region(spi_ssd1327, min_x, min_y, width, height);
    }
}

// Text drawing functions
uint16_t spi_oled_get_text_width(const variable_font_t *font, const char *text)
{
    if (!text || !font) return 0;
    
    uint16_t width = 0;
    const char *str = text;
    
    while (*str) {
        char c = *str++;
        if (c >= 32 && c <= 126) {
            uint8_t char_idx = c - 32;
            width += font->widths[char_idx];
            if (*str) width++;  // Add spacing except for last character
        }
    }
    
    return width;
}

void spi_oled_drawText(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y,
                      const variable_font_t *font, ssd1327_gs_t gs, const char *text)
{
    if (!text || !font || !spi_ssd1327->framebuffer) return;
    
    uint16_t char_x = x;
    const char *str = text;
    
    while (*str) {
        char c = *str++;
        if (c < 32 || c > 126) continue;
        
        uint8_t char_idx = c - 32;
        uint8_t char_width = font->widths[char_idx];
        uint16_t data_offset = font->offsets[char_idx];
        uint8_t bytes_per_row = (char_width + 7) / 8;
        
        // Render character
        for (uint8_t row = 0; row < font->height; row++) {
            for (uint8_t col = 0; col < char_width; col++) {
                uint8_t byte_idx = col / 8;
                uint8_t bit_idx = col % 8;
                uint8_t font_byte = font->data[data_offset + row * bytes_per_row + byte_idx];
                
                if (font_byte & (1 << (7 - bit_idx))) {
                    spi_oled_set_pixel(spi_ssd1327, char_x + col, y + row, gs);
                }
            }
        }
        
        char_x += char_width + 1;  // Character spacing
    }
    
    if (spi_ssd1327->auto_refresh) {
        uint16_t text_width = spi_oled_get_text_width(font, text);
        spi_oled_framebuffer_refresh_region(spi_ssd1327, x, y, text_width, font->height);
    }
}

void spi_oled_drawTextWithShadow(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y,
                                 const variable_font_t *font, ssd1327_gs_t gs, 
                                 ssd1327_gs_t gs_shadow, int8_t offset_x, int8_t offset_y,
                                 const char *text)
{
    if (!text || !font) return;
    
    bool original_auto_refresh = spi_ssd1327->auto_refresh;
    spi_ssd1327->auto_refresh = false;  // Disable auto refresh during drawing
    
    // Draw shadow first (if shadow grayscale > 0)
    if (gs_shadow > 0) {
        spi_oled_drawText(spi_ssd1327, x + offset_x, y + offset_y, font, gs_shadow, text);
    }
    
    // Draw main text
    spi_oled_drawText(spi_ssd1327, x, y, font, gs, text);
    
    spi_ssd1327->auto_refresh = original_auto_refresh;
    
    if (spi_ssd1327->auto_refresh) {
        // Calculate refresh region that encompasses both text and shadow
        uint16_t text_width = spi_oled_get_text_width(font, text);
        
        int16_t min_x = (offset_x < 0) ? x + offset_x : x;
        int16_t min_y = (offset_y < 0) ? y + offset_y : y;
        int16_t max_x = (offset_x > 0) ? x + text_width + offset_x : x + text_width;
        int16_t max_y = (offset_y > 0) ? y + font->height + offset_y : y + font->height;
        
        // Ensure bounds are within display
        if (min_x < 0) min_x = 0;
        if (min_y < 0) min_y = 0;
        if (max_x > SSD1327_WIDTH) max_x = SSD1327_WIDTH;
        if (max_y > SSD1327_HEIGHT) max_y = SSD1327_HEIGHT;
        
        uint8_t refresh_width = max_x - min_x;
        uint8_t refresh_height = max_y - min_y;
        
        spi_oled_framebuffer_refresh_region(spi_ssd1327, min_x, min_y, refresh_width, refresh_height);
    }
}

void spi_oled_drawImage(struct spi_ssd1327 *spi_ssd1327, uint8_t x, uint8_t y,
                       uint8_t width, uint8_t height, const uint8_t *image)
{
    if (!image || !spi_ssd1327->framebuffer) return;
    
    uint8_t image_bytes_per_row = (width + 1) / 2;
    
    for (uint8_t row = 0; row < height; row++) {
        if (y + row >= SSD1327_HEIGHT) break;
        
        for (uint8_t col = 0; col < width; col++) {
            if (x + col >= SSD1327_WIDTH) break;
            
            uint8_t byte_idx = col / 2;
            uint8_t pixel_pos = col % 2;
            uint8_t image_byte = image[row * image_bytes_per_row + byte_idx];
            
            ssd1327_gs_t pixel_value;
            if (pixel_pos == 0) {
                pixel_value = (image_byte >> 4) & 0x0F;  // Left pixel
            } else {
                pixel_value = image_byte & 0x0F;         // Right pixel
            }
            
            spi_oled_set_pixel(spi_ssd1327, x + col, y + row, pixel_value);
        }
    }
    
    if (spi_ssd1327->auto_refresh) {
        spi_oled_framebuffer_refresh_region(spi_ssd1327, x, y, width, height);
    }
}

// Utility functions
void spi_oled_set_auto_refresh(struct spi_ssd1327 *spi_ssd1327, bool auto_refresh)
{
    spi_ssd1327->auto_refresh = auto_refresh;
}