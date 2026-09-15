#include "ecg_processor.h"

#define SAMPLE_RATE             500U
#define STARTUP_SAMPLES         250U
#define REFRACTORY_SAMPLES      125U

#define RR_MIN                  150U
#define RR_MAX                  1500U

#define QUALITY_TIMEOUT         1000U

#define FILTER_Q                8192L

/* Fixed-point filter coefficients */

/* Filter section 1 */
#define B0_1                    64L
#define B1_1                    128L
#define B2_1                    64L
#define A1_1                   (-14527L)
#define A2_1                    6698L

/* Filter section 2 */
#define B0_2                    8192L
#define B1_2                  (-16384L)
#define B2_2                    8192L
#define A1_2                  (-15826L)
#define A2_2                    7675L


static int32_t abs_value(int32_t value)
{
    if (value < 0)
    {
        return -value;
    }

    return value;
}


static int32_t moving_value(int32_t current,
                            int32_t input,
                            uint8_t shift)
{
    return current + ((input - current) >> shift);
}


static int32_t filter_section(int32_t input,
                              int32_t *z1,
                              int32_t *z2,
                              int32_t b0,
                              int32_t b1,
                              int32_t b2,
                              int32_t a1,
                              int32_t a2)
{
    int64_t temp;
    int32_t output;

    temp = ((int64_t)b0 * input) + *z1;
    output = (int32_t)(temp / FILTER_Q);

    *z1 = (int32_t)(((int64_t)b1 * input)
                  - ((int64_t)a1 * output)
                  + *z2);

    *z2 = (int32_t)(((int64_t)b2 * input)
                  - ((int64_t)a2 * output));

    return output;
}


static void update_bpm(ECG_Processor *p,
                       uint32_t peak_position)
{
    uint32_t rr;
    uint32_t total = 0U;
    uint32_t average;
    uint8_t i;

    /* First peak sets the reference */

    if (p->last_peak == 0U)
    {
        p->last_peak = peak_position;
        return;
    }

    rr = peak_position - p->last_peak;

    /* Check the RR range */

    if ((rr < RR_MIN) || (rr > RR_MAX))
    {
        return;
    }

    /* Check recent RR values */

    if (p->rr_count >= 2U)
    {
        for (i = 0U; i < p->rr_count; i++)
        {
            total += p->rr[i];
        }

        average = total / p->rr_count;

        if ((rr < average / 2U) ||
            (rr > average * 2U))
        {
            return;
        }
    }

    /* Save the valid RR interval */

    p->last_peak = peak_position;

    if (p->rr_count < 4U)
    {
        p->rr[p->rr_count] = (uint16_t)rr;
        p->rr_count++;
    }
    else
    {
        p->rr[0] = p->rr[1];
        p->rr[1] = p->rr[2];
        p->rr[2] = p->rr[3];
        p->rr[3] = (uint16_t)rr;
    }

    /* Calculate average RR */

    total = 0U;

    for (i = 0U; i < p->rr_count; i++)
    {
        total += p->rr[i];
    }

    average = total / p->rr_count;

    if (average != 0U)
    {
        /* BPM = 30000 / RR at 500 Hz */
        p->bpm =
            (uint16_t)((60U * SAMPLE_RATE) / average);
    }
}


static uint8_t find_peak(ECG_Processor *p,
                         int32_t amplitude)
{
    int32_t threshold;

    /* Calculate threshold */

    threshold =
        p->noise_level +
        ((35L * (p->signal_level -
                 p->noise_level)) / 100L);

    if (threshold < 20L)
    {
        threshold = 20L;
    }

    /* Check for a local maximum */

    if ((p->prev1 > p->prev2) &&
        (p->prev1 >= amplitude) &&
        (p->prev1 > threshold) &&
        (p->refractory == 0U))
    {
        p->signal_level =
            moving_value(p->signal_level,
                         p->prev1,
                         3U);

        p->refractory =
            REFRACTORY_SAMPLES;

        return 1U;
    }

    /* Update noise level */

    if ((p->prev1 > p->prev2) &&
        (p->prev1 >= amplitude))
    {
        p->noise_level =
            moving_value(p->noise_level,
                         p->prev1,
                         4U);
    }

    return 0U;
}


static void update_quality(ECG_Processor *p)
{
    uint8_t score = 0U;

    /* Check signal level */

    if ((p->signal_level > 0L) &&
        (p->signal_level >=
         (p->noise_level * 3L)))
    {
        score++;
    }

    /* Check RR consistency */

    if (p->rr_count >= 3U)
    {
        uint32_t total = 0U;
        uint16_t minimum = 65535U;
        uint16_t maximum = 0U;
        uint32_t average;
        uint8_t i;

        for (i = 0U; i < p->rr_count; i++)
        {
            uint16_t value = p->rr[i];

            total += value;

            if (value < minimum)
            {
                minimum = value;
            }

            if (value > maximum)
            {
                maximum = value;
            }
        }

        average = total / p->rr_count;

        if ((maximum - minimum) <=
            (average / 4U))
        {
            score++;
        }
    }

    /* Check recent peak */

    if (p->last_peak != 0U)
    {
        uint32_t age =
            p->sample_index - p->last_peak;

        if (age <= QUALITY_TIMEOUT)
        {
            score++;
        }
    }

    /* Set quality */

    if (score >= 3U)
    {
        p->quality = ECG_QUALITY_GOOD;
    }
    else if (score == 2U)
    {
        p->quality = ECG_QUALITY_FAIR;
    }
    else
    {
        p->quality = ECG_QUALITY_POOR;
    }
}


void ECG_Processor_Init(ECG_Processor *p)
{
    uint8_t i;

    p->sample_index = 0U;
    p->dc_estimate = 0;

    p->z1_1 = 0;
    p->z2_1 = 0;
    p->z1_2 = 0;
    p->z2_2 = 0;

    p->prev2 = 0;
    p->prev1 = 0;

    p->signal_level = 100;
    p->noise_level = 10;

    p->refractory = 0U;
    p->last_peak = 0U;

    p->rr_count = 0U;
    p->bpm = 0U;

    p->quality = ECG_QUALITY_POOR;

    for (i = 0U; i < 4U; i++)
    {
        p->rr[i] = 0U;
    }
}


ECG_Result ECG_Processor_Process(
    ECG_Processor *p,
    uint16_t sample)
{
    ECG_Result result;

    int32_t centered;
    int32_t filtered;
    int32_t amplitude;

    uint8_t peak = 0U;

    /* Remove DC component */

    if (p->sample_index == 0U)
    {
        p->dc_estimate =
            (int32_t)sample;
    }

    p->dc_estimate =
        moving_value(
            p->dc_estimate,
            (int32_t)sample,
            8U);

    centered =
        (int32_t)sample -
        p->dc_estimate;

    /* First filter section */

    filtered =
        filter_section(
            centered,
            &p->z1_1,
            &p->z2_1,
            B0_1,
            B1_1,
            B2_1,
            A1_1,
            A2_1);

    /* Second filter section */

    filtered =
        filter_section(
            filtered,
            &p->z1_2,
            &p->z2_2,
            B0_2,
            B1_2,
            B2_2,
            A1_2,
            A2_2);

    /* Get signal magnitude */

    amplitude =
        abs_value(filtered);

    /* Start peak detection after startup */

    if (p->sample_index >= STARTUP_SAMPLES)
    {
        peak =
            find_peak(
                p,
                amplitude);

        if (peak != 0U)
        {
            /* Peak is from previous sample */

            update_bpm(
                p,
                p->sample_index - 1U);
        }
    }

    /* Update refractory counter */

    if (p->refractory > 0U)
    {
        p->refractory--;
    }

    /* Save previous values */

    p->prev2 =
        p->prev1;

    p->prev1 =
        amplitude;

    /* Update sample count */

    p->sample_index++;

    /* Update signal quality */

    update_quality(p);

    /* Return current result */

    result.processed =
        (int16_t)filtered;

    result.r_peak =
        peak;

    result.bpm =
        p->bpm;

    result.quality =
        p->quality;

    return result;
}
