#include "circular_buffer.h"

void CircularBuffer_Init(CircularBuffer *buffer)
{
    if (buffer == NULL)
    {
        return;
    }

    buffer->head = 0U;
    buffer->tail = 0U;
    buffer->overflow = 0U;
}


uint8_t CircularBuffer_Push(
    CircularBuffer *buffer,
    uint16_t sample)
{
    uint16_t next_head;

    if (buffer == NULL)
    {
        return 0U;
    }

    next_head = buffer->head + 1U;

    if (next_head >= ECG_BUFFER_SIZE)
    {
        next_head = 0U;
    }

    /* Check if the buffer is full */

    if (next_head == buffer->tail)
    {
        /* Drop the oldest sample */

        buffer->tail++;

        if (buffer->tail >= ECG_BUFFER_SIZE)
        {
            buffer->tail = 0U;
        }

        buffer->overflow = 1U;
    }

    buffer->data[buffer->head] = sample;
    buffer->head = next_head;

    return 1U;
}


uint8_t CircularBuffer_Pop(
    CircularBuffer *buffer,
    uint16_t *sample)
{
    if ((buffer == NULL) ||
        (sample == NULL))
    {
        return 0U;
    }

    /* Check if the buffer is empty */

    if (buffer->head == buffer->tail)
    {
        return 0U;
    }

    *sample = buffer->data[buffer->tail];

    buffer->tail++;

    if (buffer->tail >= ECG_BUFFER_SIZE)
    {
        buffer->tail = 0U;
    }

    return 1U;
}


uint8_t CircularBuffer_IsEmpty(
    const CircularBuffer *buffer)
{
    if (buffer == NULL)
    {
        return 1U;
    }

    return (buffer->head == buffer->tail) ? 1U : 0U;
}


uint8_t CircularBuffer_HasOverflowed(
    const CircularBuffer *buffer)
{
    if (buffer == NULL)
    {
        return 0U;
    }

    return buffer->overflow;
}


void CircularBuffer_ClearOverflow(
    CircularBuffer *buffer)
{
    if (buffer == NULL)
    {
        return;
    }

    buffer->overflow = 0U;
}
