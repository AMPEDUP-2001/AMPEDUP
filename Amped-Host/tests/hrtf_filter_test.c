#include "apu_state.h"
#include <stdio.h>

/* Reference each tap directly from the circular history. The optimized
 * history must produce the same samples for both separate and in-place IO. */
static void reference_frame(HrtfFilter *filter, float input[][2], float output[][2])
{
    for (int n = 0; n < HRTF_SAMPLES_PER_FRAME; n++) {
        hrtf_filter_step_parameters(filter);
        for (int ch = 0; ch < 2; ch++) {
            float *history = filter->ch[ch].buf;
            float delay = fmaxf(0.0f, ch ? -filter->itd_cur : filter->itd_cur);
            int whole = (int)delay;
            float fraction = delay - whole, sum = 0.0f;
            history[filter->buf_pos] = input[n][ch];
            for (int k = 0; k < HRTF_NUM_TAPS; k++) {
                int pos = (filter->buf_pos - whole - k + HRTF_BUFLEN) % HRTF_BUFLEN;
                float sample = history[pos];
                if (fraction > 0.0f)
                    sample = sample * (1 - fraction) + history[(pos + HRTF_BUFLEN - 1) % HRTF_BUFLEN] * fraction;
                sum += filter->ch[ch].hrir_coeff_cur[k] * sample;
            }
            output[n][ch] = sum;
        }
        filter->buf_pos = (filter->buf_pos + 1) % HRTF_BUFLEN;
    }
}

static float next_sample(uint32_t *state)
{
    *state = *state * 1664525u + 1013904223u;
    return (float)(*state >> 8) / 8388608.0f - 1.0f;
}

int main(void)
{
    HrtfFilter stock, candidate;
    uint32_t seed = 37;
    const float delays[] = {0, 42, -42, 1, -1, 0.5f, -13.75f, 80, -80};
    hrtf_filter_init(&stock);
    hrtf_filter_init(&candidate);
    for (int frame = 0; frame < 4096; frame++) {
        float input[HRTF_SAMPLES_PER_FRAME][2], expected[HRTF_SAMPLES_PER_FRAME][2], actual[HRTF_SAMPLES_PER_FRAME][2];
        if (frame % 47 == 0) {
            float coefficients[2][HRTF_NUM_TAPS];
            for (int c = 0; c < 2; c++)
                for (int k = 0; k < HRTF_NUM_TAPS; k++)
                    coefficients[c][k] = next_sample(&seed);
            float delay = delays[(frame / 47) % 9];
            hrtf_filter_set_target_params(&stock, coefficients, delay);
            hrtf_filter_set_target_params(&candidate, coefficients, delay);
        }
        if (frame % 89 == 0) {
            hrtf_filter_clear_history(&stock);
            hrtf_filter_clear_history(&candidate);
        }
        for (int n = 0; n < HRTF_SAMPLES_PER_FRAME; n++)
            for (int c = 0; c < 2; c++)
                input[n][c] = next_sample(&seed);
        memcpy(actual, input, sizeof input);
        reference_frame(&stock, input, expected);
        hrtf_filter_process(&candidate, frame % 2 ? actual : input, actual);
        if (memcmp(expected, actual, sizeof actual)) {
            fprintf(stderr, "Different ear sample at frame %d\n", frame);
            return 1;
        }
    }
    return 0;
}
