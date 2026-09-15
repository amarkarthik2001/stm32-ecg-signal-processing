#ifndef CIRCULAR_BUFFER_H
#define CIRCULAR_BUFFER_H

#include "main.h"

#define ECG_BUFFER_SIZE    256U

typedef struct
{
    uint16_t data[ECG_BUFFER_SIZE];

    volatile uint16_t head;
    volatile uint16_t tail;

    volatile uint8_t overflow;

} CircularBuffer;


void CircularBuffer_Init(CircularBuffer *buffer);

uint8_t CircularBuffer_Push(
    CircularBuffer *buffer,
    uint16_t sample);

uint8_t CircularBuffer_Pop(
    CircularBuffer *buffer,
    uint16_t *sample);

uint8_t CircularBuffer_IsEmpty(
    const CircularBuffer *buffer);

uint8_t CircularBuffer_HasOverflowed(
    const CircularBuffer *buffer);

void CircularBuffer_ClearOverflow(
    CircularBuffer *buffer);

#endif
