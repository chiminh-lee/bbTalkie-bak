// AGC Configuration
#define TARGET_RMS 6000      // Target RMS level (adjust based on your needs)
#define AGC_ATTACK 0.1f      // How fast to reduce gain when loud
#define AGC_RELEASE 0.01f    // How fast to increase gain when quiet
#define MIN_GAIN 0.1f        // Minimum gain multiplier
#define MAX_GAIN 8.0f        // Maximum gain multiplier

typedef struct {
    float current_gain;
    float target_rms;
    float attack_rate;
    float release_rate;
} agc_t;