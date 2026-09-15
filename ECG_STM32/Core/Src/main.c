#include "main.h"
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"
#include "circular_buffer.h"
#include "ecg_processor.h"

#include <stdio.h>
#include <string.h>

void SystemClock_Config(void);

static CircularBuffer ecg_buffer;
static ECG_Processor ecg_processor;
static ECG_Result latest_result;

static uint16_t sample;
static uint32_t uart_counter = 0U;
static char uart_buffer[96];


static const char *get_quality_text(ECG_Quality quality)
{
    if (quality == ECG_QUALITY_GOOD)
    {
        return "GOOD";
    }

    if (quality == ECG_QUALITY_FAIR)
    {
        return "FAIR";
    }

    return "POOR";
}


int main(void)
{
    HAL_Init();

    SystemClock_Config();

    MX_GPIO_Init();
    MX_ADC1_Init();
    MX_TIM3_Init();
    MX_USART1_UART_Init();

    /* Initialize ECG modules */

    CircularBuffer_Init(&ecg_buffer);
    ECG_Processor_Init(&ecg_processor);

    /* Start ADC */

    if (HAL_ADC_Start_IT(&hadc1) != HAL_OK)
    {
        Error_Handler();
    }

    /* Start sampling timer */

    if (HAL_TIM_Base_Start(&htim3) != HAL_OK)
    {
        Error_Handler();
    }

    /* Send startup message */

    {
        const char start_message[] =
            "STM32 ECG PROCESSOR START\r\n";

        HAL_UART_Transmit(
            &huart1,
            (uint8_t *)start_message,
            sizeof(start_message) - 1U,
            HAL_MAX_DELAY
        );
    }

    while (1)
    {
        /* Process available samples */

        while (CircularBuffer_Pop(&ecg_buffer, &sample) != 0U)
        {
            latest_result = ECG_Processor_Process(
                &ecg_processor,
                sample
            );

            /* Send one report for every 50 samples at 500 Hz this gives 10 reports per second. */

            uart_counter++;

            if (uart_counter >= 50U)
            {
                snprintf(
                    uart_buffer,
                    sizeof(uart_buffer),
                    "ECG=%d,R=%u,BPM=%u,Q=%s\r\n",
                    (int)latest_result.processed,
                    (unsigned int)latest_result.r_peak,
                    (unsigned int)latest_result.bpm,
                    get_quality_text(latest_result.quality)
                );

                HAL_UART_Transmit(
                    &huart1,
                    (uint8_t *)uart_buffer,
                    strlen(uart_buffer),
                    HAL_MAX_DELAY
                );

                uart_counter = 0U;
            }
        }
    }
}


/* Store ADC sample in the buffer */

void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *hadc)
{
    if (hadc->Instance == ADC1)
    {
        uint16_t adc_sample;

        adc_sample =
            (uint16_t)HAL_ADC_GetValue(hadc);

        CircularBuffer_Push(
            &ecg_buffer,
            adc_sample
        );
    }
}


/* Configure system clock */

void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSE;

    RCC_OscInitStruct.HSEState =
        RCC_HSE_ON;

    RCC_OscInitStruct.HSEPredivValue =
        RCC_HSE_PREDIV_DIV1;

    RCC_OscInitStruct.HSIState =
        RCC_HSI_ON;

    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_ON;

    RCC_OscInitStruct.PLL.PLLSource =
        RCC_PLLSOURCE_HSE;

    RCC_OscInitStruct.PLL.PLLMUL =
        RCC_PLL_MUL9;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_PLLCLK;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV2;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_2) != HAL_OK)
    {
        Error_Handler();
    }

    PeriphClkInit.PeriphClockSelection =
        RCC_PERIPHCLK_ADC;

    PeriphClkInit.AdcClockSelection =
        RCC_ADCPCLK2_DIV8;

    if (HAL_RCCEx_PeriphCLKConfig(
            &PeriphClkInit) != HAL_OK)
    {
        Error_Handler();
    }
}


/* Stop here on hardware error */

void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
    }
}


#ifdef USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
    (void)file;
    (void)line;
}

#endif

