#include "led_strip.h"
#define WS2812_GPIO_PIN 15
#define WS2812_LED_COUNT 1
static led_strip_handle_t led_strip;

// LED state structure
typedef struct
{
    uint8_t r, g, b;
} led_color_t;

// Color definitions
#define DARKER_TEAL_R 0
#define DARKER_TEAL_G 100
#define DARKER_TEAL_B 100

#define RECEIVING_GREEN_R 0
#define RECEIVING_GREEN_G 255
#define RECEIVING_GREEN_B 50

#define SPEAKING_BLUE_R 0
#define SPEAKING_BLUE_G 150
#define SPEAKING_BLUE_B 255

// Animation parameters
#define TRANSITION_STEPS 10      // Steps for smooth transition
#define TRANSITION_DELAY_MS 20   // Delay between transition steps
#define BREATHING_PERIOD_MS 1000 // Full breathing cycle duration
#define BREATHING_STEPS 50       // Steps in breathing cycle

// Linear interpolation between two values
static uint8_t lerp_color(uint8_t start, uint8_t end, float t)
{
    return (uint8_t)(start + t * (end - start));
}

// Calculate breathing effect multiplier (sine wave based)
static float breathing_multiplier(uint32_t time_ms)
{
    float phase = (float)(time_ms % BREATHING_PERIOD_MS) / BREATHING_PERIOD_MS;
    // Use sine wave for smooth breathing, ranging from 0.3 to 1.0
    return 0.3f + 0.7f * (1.0f + sinf(2.0f * M_PI * phase)) / 2.0f;
}

// Apply color to LED strip
static void set_led_color(led_color_t color)
{
    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, color.r, color.g, color.b));
    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}

// Smooth transition between two colors
static void transition_to_color(led_color_t from, led_color_t to)
{
    for (int step = 0; step <= TRANSITION_STEPS; step++)
    {
        float t = (float)step / TRANSITION_STEPS;

        led_color_t current = {
            .r = lerp_color(from.r, to.r, t),
            .g = lerp_color(from.g, to.g, t),
            .b = lerp_color(from.b, to.b, t)};

        set_led_color(current);
        vTaskDelay(pdMS_TO_TICKS(TRANSITION_DELAY_MS));

        // Check for state changes during transition
        if (is_speaking)
            break; // Priority to speaking state
    }
}

void led_control_task(void *pvParameters)
{
    ESP_LOGI(TAG, "LED control task started");

    // Define color states
    led_color_t darker_teal = {DARKER_TEAL_R, DARKER_TEAL_G, DARKER_TEAL_B};
    led_color_t receiving_green = {RECEIVING_GREEN_R, RECEIVING_GREEN_G, RECEIVING_GREEN_B};
    led_color_t speaking_blue = {SPEAKING_BLUE_R, SPEAKING_BLUE_G, SPEAKING_BLUE_B};

    // Current state tracking
    typedef enum
    {
        STATE_IDLE,
        STATE_RECEIVING,
        STATE_SPEAKING,
        STATE_TRANSITION_TO_RECEIVING,
        STATE_TRANSITION_TO_IDLE,
        STATE_TRANSITION_TO_SPEAKING
    } led_state_t;

    led_state_t current_state = STATE_IDLE;
    led_state_t previous_state = STATE_IDLE;
    led_color_t current_color = darker_teal;

    // Set initial color
    set_led_color(darker_teal);

    uint32_t breathing_start_time = 0;

    while (!isShutdown)
    {
        uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;

        // Determine desired state based on flags
        led_state_t desired_state;
        if (is_speaking)
        {
            desired_state = STATE_SPEAKING;
        }
        else if (is_receiving)
        {
            desired_state = STATE_RECEIVING;
        }
        else
        {
            desired_state = STATE_IDLE;
        }

        // Handle state transitions
        if (desired_state != current_state)
        {
            ESP_LOGI(TAG, "State change: %d -> %d", current_state, desired_state);
            previous_state = current_state;

            switch (desired_state)
            {
            case STATE_SPEAKING:
                current_state = STATE_TRANSITION_TO_SPEAKING;
                breathing_start_time = current_time; // Reset breathing timer for blue
                break;
            case STATE_RECEIVING:
                current_state = STATE_TRANSITION_TO_RECEIVING;
                breathing_start_time = current_time; // Reset breathing timer for green
                break;
            case STATE_IDLE:
                current_state = STATE_TRANSITION_TO_IDLE;
                break;
            default:
                break;
            }
        }

        // Handle current state
        switch (current_state)
        {
        case STATE_TRANSITION_TO_SPEAKING:
            transition_to_color(current_color, speaking_blue);
            current_color = speaking_blue;
            current_state = STATE_SPEAKING;
            break;

        case STATE_TRANSITION_TO_RECEIVING:
            transition_to_color(current_color, receiving_green);
            current_color = receiving_green;
            current_state = STATE_RECEIVING;
            break;

        case STATE_TRANSITION_TO_IDLE:
            transition_to_color(current_color, darker_teal);
            current_color = darker_teal;
            current_state = STATE_IDLE;
            break;

        case STATE_SPEAKING:
            // Breathing effect with blue color
            {
                float multiplier = breathing_multiplier(current_time - breathing_start_time);
                led_color_t breathing_color = {
                    .r = (uint8_t)(speaking_blue.r * multiplier),
                    .g = (uint8_t)(speaking_blue.g * multiplier),
                    .b = (uint8_t)(speaking_blue.b * multiplier)};
                set_led_color(breathing_color);
                vTaskDelay(pdMS_TO_TICKS(BREATHING_PERIOD_MS / BREATHING_STEPS));
            }
            break;

        case STATE_RECEIVING:
            // Breathing effect with green color
            {
                float multiplier = breathing_multiplier(current_time - breathing_start_time);
                led_color_t breathing_color = {
                    .r = (uint8_t)(receiving_green.r * multiplier),
                    .g = (uint8_t)(receiving_green.g * multiplier),
                    .b = (uint8_t)(receiving_green.b * multiplier)};
                set_led_color(breathing_color);
                vTaskDelay(pdMS_TO_TICKS(BREATHING_PERIOD_MS / BREATHING_STEPS));
            }
            break;

        case STATE_IDLE:
        default:
            // Solid darker teal
            set_led_color(darker_teal);
            vTaskDelay(pdMS_TO_TICKS(50));
            break;
        }
    }
    vTaskDelete(NULL);
}