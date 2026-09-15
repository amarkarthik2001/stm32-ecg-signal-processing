# Real-Time ECG Signal Processing on STM32

A real-time ECG signal-processing system implemented on the STM32F103C8T6 using Embedded C.

The firmware acquires ECG samples at 500 Hz, stores them in a circular buffer, performs digital signal processing, detects R-peaks, calculates heart rate, evaluates signal quality, and reports the results through UART.

## System Architecture

```text
ECG Input
    ↓
STM32 ADC1
    ↓
500 Hz Sampling
    ↓
Circular Buffer
    ↓
ECG Processor
    ├── DC / Baseline Removal
    ├── IIR Filtering
    ├── R-Peak Detection
    ├── RR Interval Calculation
    ├── BPM Calculation
    └── Signal Quality
    ↓
USART1
    ↓
UART Output
```

The project uses STM32CubeMX for peripheral configuration and STM32 HAL for peripheral initialization and hardware access.

## Hardware

- STM32F103C8T6 Blue Pill
- 12-bit ADC
- 3.3 V ADC reference
- USART1
- 500 Hz sampling rate

## Firmware

The real-time ECG processing is implemented in Embedded C on the STM32.

### Acquisition

ADC1 acquires ECG samples at 500 Hz.

Each sample is placed into a circular buffer. The ADC conversion callback only captures the ADC result and stores it in the buffer. The main loop performs the ECG processing.

The complete ECG recording is not stored in STM32 RAM.

### Signal Processing

Each sample is processed sequentially:

1. Baseline/DC component estimation and removal
2. Digital IIR filtering
3. Signal amplitude calculation
4. R-peak detection
5. RR interval validation
6. BPM calculation
7. Signal-quality evaluation

## Digital Filter

A two-section fixed-point IIR band-pass filter is used at 500 Hz.

Configured response:

```text
Approximately 5 Hz – 20 Hz
```

The filter reduces slow baseline variation and higher-frequency noise while retaining the main QRS-related signal content.

The filter uses integer and fixed-point calculations and maintains its state between samples.

## R-Peak Detection

R-peaks are detected using:

- Local-maximum detection
- Adaptive thresholding
- Signal and noise level estimation
- Refractory-period control

A refractory period of 125 samples is used.

At 500 Hz:

```text
125 / 500 = 0.25 seconds
```

This corresponds to 250 ms and helps prevent multiple detections from occurring too close together.

RR intervals are checked against configured limits and recent RR values to reject some incorrect detections.

A separate search-back recovery stage is not implemented.

## BPM Calculation

The RR interval is the number of samples between two accepted R-peaks.

At 500 Hz:

```text
BPM = 30000 / RR
```

A short history of accepted RR intervals is maintained and averaged to make the BPM output more stable.

## Signal Quality

The firmware reports three signal-quality states:

```text
GOOD
FAIR
POOR
```

The quality decision uses:

- Estimated signal level
- Estimated noise level
- Recent RR consistency
- Time since the last valid R-peak

The quality value is an engineering indication of signal condition and is not a medical diagnosis.

## UART Output

The firmware reports:

```text
ECG=<processed>,R=<peak>,BPM=<bpm>,Q=<quality>
```

Example:

```text
ECG=123,R=0,BPM=72,Q=GOOD
```

USART1 is configured for 115200 baud.

Reports are sent periodically rather than for every sample to reduce communication overhead.

## Offline Validation

Python is used only for offline validation and plot generation.

The validation recording contains 15,000 ECG samples at 500 Hz.

Validation results:

```text
Samples        : 15,000
Duration       : 29.998 seconds
Sampling Rate  : 500 Hz
Detected Peaks : 35
Mean BPM       : 72.02
Final BPM      : 72
Quality        : GOOD
```

Validation outputs include:

- Raw ECG plot
- Processed ECG plot
- R-peak detection plot
- Processed ECG CSV
- R-peak location CSV

### Validation Flow

```text
ECG Dataset
    ↓
Python Offline Validation
    ↓
Processing
    ↓
Filtered ECG
    ↓
R-Peak Detection
    ↓
BPM / Signal Quality
    ↓
Plots + CSV Results
```

Python is used only for offline validation and plotting. The runtime ECG processing is implemented in Embedded C on the STM32.

## Repository Structure

```text
stm32-ecg-signal-processing/
│
├── ECG_STM32/
│   ├── Core/
│   │   ├── Inc/
│   │   └── Src/
│   ├── Drivers/
│   ├── .settings/
│   ├── .cproject
│   ├── .project
│   ├── .mxproject
│   ├── STM32_ECG_Signal_Processing.ioc
│   └── STM32F103C8TX_FLASH.ld
│
├── ECG_Validation/
│   ├── 01_raw_ecg.png
│   ├── 02_processed_ecg.png
│   ├── 03_rpeak_detection.png
│   ├── ECG_Validation_Input_30s.csv
│   ├── offline_validation.py
│   ├── processed_ecg_output.csv
│   └── r_peak_locations.csv
│
├── .gitignore
├── README.md
└── LICENSE
```

## Main Application Files

### `main.c`

Handles:

- STM32 initialization
- ADC / timer / UART startup
- Circular buffer handling
- ECG processing calls
- Periodic UART reporting

### `circular_buffer.c / circular_buffer.h`

Implements the circular buffer used to decouple ADC sample acquisition from ECG processing.

The allocated buffer contains 256 `uint16_t` entries:

```text
256 × 2 bytes = 512 bytes
```

One slot is reserved to distinguish the full and empty states, so the usable capacity is 255 entries.

### `ecg_processor.c / ecg_processor.h`

Implements:

- Baseline estimation
- Digital filtering
- R-peak detection
- RR interval validation
- BPM calculation
- Signal-quality evaluation

## RAM / Resource Considerations

Four RR interval values are retained:

```text
4 × 2 bytes = 8 bytes
```

The firmware does not store the complete ECG recording in RAM. Only the state required for buffering, filtering, peak detection, RR calculation and UART reporting is maintained.

Latest STM32CubeIDE build:

```text
Text     : 15,168 bytes
Data     : 92 bytes
BSS      : 2,788 bytes
Errors   : 0
Warnings : 0
```

The processing is performed sample by sample, but exact execution time per sample was not measured.

CPU utilization was not measured.

## Hardware Test

The firmware was built and tested on an STM32F103C8T6 Blue Pill.

The hardware test verified firmware execution and UART reporting.

A complete ECG analog front end was not connected during the hardware test, so the test does not represent a complete physiological ECG measurement.

A complete ECG device would require a suitable analog front end, signal conditioning and appropriate electrical safety protection.

## Limitations

- A separate search-back R-peak recovery stage is not implemented.
- Exact execution time per sample was not measured.
- CPU utilization was not measured.
- A complete ECG analog front end was not used during the hardware test.
- The signal-quality value is an engineering indicator and not a medical result.

## Development Environment

- STM32CubeIDE
- STM32CubeMX
- STM32 HAL
- Embedded C
- Python for offline validation

## License

MIT License