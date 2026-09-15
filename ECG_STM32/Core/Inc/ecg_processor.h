#ifndef ECG_PROCESSOR_H
#define ECG_PROCESSOR_H

#include "main.h"

typedef enum
{
    ECG_QUALITY_POOR = 0,
    ECG_QUALITY_FAIR,
    ECG_QUALITY_GOOD
} ECG_Quality;


typedef struct
{
    int16_t processed;
    uint8_t r_peak;
    uint16_t bpm;
    ECG_Quality quality;
} ECG_Result;


typedef struct
{
    uint32_t sample_index;

    /* Baseline/DC estimate */
    int32_t dc_estimate;

    /* Filter state */
    int32_t z1_1;
    int32_t z2_1;

    int32_t z1_2;
    int32_t z2_2;

    /* Peak detection history */
    int32_t prev2;
    int32_t prev1;

    /* Signal and noise estimates */
    int32_t signal_level;
    int32_t noise_level;

    /* R-peak refractory counter */
    uint16_t refractory;

    /* Last accepted R-peak position */
    uint32_t last_peak;

    /* Recent RR intervals */
    uint16_t rr[4];
    uint8_t rr_count;

    /* Current BPM */
    uint16_t bpm;

    /* Current signal quality */
    ECG_Quality quality;

} ECG_Processor;


/* Initialize ECG processor */
void ECG_Processor_Init(
    ECG_Processor *p);


/* Process one 12-bit ECG ADC sample */
ECG_Result ECG_Processor_Process(
    ECG_Processor *p,
    uint16_t sample);

#endif /* ECG_PROCESSOR_H */
