import pandas as pd
import numpy as np
import matplotlib.pyplot as plt


# ---------------------------------------------------------
# Configuration
# ---------------------------------------------------------

SAMPLE_RATE = 500

STARTUP_SAMPLES = 250
REFRACTORY_SAMPLES = 125

RR_MIN = 150
RR_MAX = 1500

QUALITY_TIMEOUT = 1000

FILTER_Q = 8192


# Filter coefficients used by the STM32 firmware
B0_1 = 64
B1_1 = 128
B2_1 = 64
A1_1 = -14527
A2_1 = 6698

B0_2 = 8192
B1_2 = -16384
B2_2 = 8192
A1_2 = -15826
A2_2 = 7675


# ---------------------------------------------------------
# Utility functions
# ---------------------------------------------------------

def moving_value(current, input_value, shift):
    return current + ((input_value - current) >> shift)


def filter_section(input_value, z1, z2,
                   b0, b1, b2, a1, a2):

    temp = (b0 * input_value) + z1

    output = int(temp / FILTER_Q)

    z1_new = (
        (b1 * input_value)
        - (a1 * output)
        + z2
    )

    z2_new = (
        (b2 * input_value)
        - (a2 * output)
    )

    return output, z1_new, z2_new


# ---------------------------------------------------------
# ECG processor
# Same processing flow as the STM32 firmware
# ---------------------------------------------------------

def process_ecg(samples):

    processed = []
    r_peaks = []
    bpm_values = []
    quality_values = []

    sample_index = 0

    dc_estimate = 0

    z1_1 = 0
    z2_1 = 0

    z1_2 = 0
    z2_2 = 0

    prev2 = 0
    prev1 = 0

    signal_level = 100
    noise_level = 10

    refractory = 0

    last_peak = 0

    rr_history = []

    bpm = 0

    for sample in samples:

        sample = int(sample)

        # ---------------------------------------------
        # DC / baseline removal
        # ---------------------------------------------

        if sample_index == 0:
            dc_estimate = sample

        dc_estimate = moving_value(
            dc_estimate,
            sample,
            8
        )

        centered = sample - dc_estimate

        # ---------------------------------------------
        # First filter section
        # ---------------------------------------------

        filtered, z1_1, z2_1 = filter_section(
            centered,
            z1_1,
            z2_1,
            B0_1,
            B1_1,
            B2_1,
            A1_1,
            A2_1
        )

        # ---------------------------------------------
        # Second filter section
        # ---------------------------------------------

        filtered, z1_2, z2_2 = filter_section(
            filtered,
            z1_2,
            z2_2,
            B0_2,
            B1_2,
            B2_2,
            A1_2,
            A2_2
        )

        amplitude = abs(filtered)

        peak = 0

        # ---------------------------------------------
        # R-peak detection
        # ---------------------------------------------

        if sample_index >= STARTUP_SAMPLES:

            threshold = (
                noise_level
                + (
                    35 * (signal_level - noise_level)
                    // 100
                )
            )

            if threshold < 20:
                threshold = 20

            # Local maximum and threshold test
            if (
                prev1 > prev2
                and prev1 >= amplitude
                and prev1 > threshold
                and refractory == 0
            ):

                signal_level = moving_value(
                    signal_level,
                    prev1,
                    3
                )

                refractory = REFRACTORY_SAMPLES

                peak = 1

                peak_position = sample_index - 1

                # -------------------------------------
                # RR interval / BPM
                # -------------------------------------

                if last_peak == 0:

                    last_peak = peak_position

                else:

                    rr = peak_position - last_peak

                    if RR_MIN <= rr <= RR_MAX:

                        accept = True

                        if len(rr_history) >= 2:

                            average_rr = (
                                sum(rr_history)
                                // len(rr_history)
                            )

                            if (
                                rr < average_rr // 2
                                or rr > average_rr * 2
                            ):
                                accept = False

                        if accept:

                            last_peak = peak_position

                            rr_history.append(rr)

                            if len(rr_history) > 4:
                                rr_history.pop(0)

                            average_rr = (
                                sum(rr_history)
                                // len(rr_history)
                            )

                            if average_rr != 0:

                                bpm = (
                                    60 * SAMPLE_RATE
                                ) // average_rr

            # -----------------------------------------
            # Noise update
            # -----------------------------------------

            elif (
                prev1 > prev2
                and prev1 >= amplitude
            ):

                noise_level = moving_value(
                    noise_level,
                    prev1,
                    4
                )

        # ---------------------------------------------
        # Refractory countdown
        # ---------------------------------------------

        if refractory > 0:
            refractory -= 1

        # ---------------------------------------------
        # Save previous samples
        # ---------------------------------------------

        prev2 = prev1
        prev1 = amplitude

        # ---------------------------------------------
        # Signal quality
        # ---------------------------------------------

        score = 0

        if (
            signal_level > 0
            and signal_level >= noise_level * 3
        ):
            score += 1

        if len(rr_history) >= 3:

            rr_array = np.array(rr_history)

            minimum = rr_array.min()
            maximum = rr_array.max()
            average = rr_array.mean()

            if maximum - minimum <= average / 4:
                score += 1

        if last_peak != 0:

            age = sample_index - last_peak

            if age <= QUALITY_TIMEOUT:
                score += 1

        if score >= 3:
            quality = "GOOD"

        elif score == 2:
            quality = "FAIR"

        else:
            quality = "POOR"

        # ---------------------------------------------
        # Store result
        # ---------------------------------------------

        processed.append(filtered)
        r_peaks.append(peak)
        bpm_values.append(bpm)
        quality_values.append(quality)

        sample_index += 1

    return (
        np.array(processed),
        np.array(r_peaks),
        np.array(bpm_values),
        quality_values
    )


# ---------------------------------------------------------
# Load ECG dataset
# ---------------------------------------------------------

filename = "ECG_Validation_Input_30s.csv"

data = pd.read_csv(filename)

print()
print("ECG DATASET")
print("------------------------------")

print("Number of samples:", len(data))
print("First columns:", list(data.columns))

time = data["time_s"].to_numpy()
adc = data["adc_12bit"].to_numpy(dtype=np.int32)


# ---------------------------------------------------------
# Process dataset
# ---------------------------------------------------------

print()
print("Processing ECG samples...")

processed, r_peaks, bpm_values, quality_values = process_ecg(adc)

print("Processing complete.")


# ---------------------------------------------------------
# Find R-peaks
# ---------------------------------------------------------

peak_indices = np.where(r_peaks == 1)[0]

peak_times = time[peak_indices]


# ---------------------------------------------------------
# RR interval calculation
# ---------------------------------------------------------

rr_samples = np.diff(peak_indices)

rr_seconds = rr_samples / SAMPLE_RATE

if len(rr_seconds) > 0:

    bpm_from_rr = 60.0 / rr_seconds

    mean_rr = rr_seconds.mean()
    median_rr = np.median(rr_seconds)
    mean_bpm = bpm_from_rr.mean()

else:

    mean_rr = np.nan
    median_rr = np.nan
    mean_bpm = np.nan


# ---------------------------------------------------------
# Final results
# ---------------------------------------------------------

final_bpm = int(bpm_values[-1])

final_quality = quality_values[-1]


print()
print("VALIDATION RESULT")
print("------------------------------")

print(f"Samples           : {len(adc)}")
print(f"Duration           : {time[-1]:.3f} s")
print(f"Sampling rate      : {SAMPLE_RATE} Hz")
print(f"ADC minimum        : {adc.min()}")
print(f"ADC maximum        : {adc.max()}")
print(f"R-peaks detected   : {len(peak_indices)}")
print(f"Mean RR            : {mean_rr:.4f} s")
print(f"Median RR          : {median_rr:.4f} s")
print(f"Mean BPM           : {mean_bpm:.2f}")
print(f"Final BPM          : {final_bpm}")
print(f"Signal quality     : {final_quality}")


# ---------------------------------------------------------
# Save processed samples
# ---------------------------------------------------------

output = pd.DataFrame({

    "time_s": time,

    "adc_12bit": adc,

    "processed_ecg": processed,

    "r_peak": r_peaks,

    "bpm": bpm_values,

    "quality": quality_values

})

output.to_csv(
    "processed_ecg_output.csv",
    index=False
)


# ---------------------------------------------------------
# Save R-peak locations
# ---------------------------------------------------------

peak_output = pd.DataFrame({

    "sample_index": peak_indices,

    "time_s": peak_times

})

peak_output.to_csv(
    "r_peak_locations.csv",
    index=False
)


# ---------------------------------------------------------
# Plot 1: Raw ECG
# ---------------------------------------------------------

plt.figure(figsize=(12, 4))

plt.plot(
    time,
    adc
)

plt.xlabel("Time (s)")
plt.ylabel("ADC value")

plt.title(
    "ECG Dataset - Raw ECG Signal"
)

plt.grid(True)

plt.tight_layout()

plt.savefig(
    "01_raw_ecg.png",
    dpi=200
)

plt.show()


# ---------------------------------------------------------
# Plot 2: Processed ECG
# ---------------------------------------------------------

plt.figure(figsize=(12, 4))

plt.plot(
    time,
    processed
)

plt.xlabel("Time (s)")
plt.ylabel("Processed ECG")

plt.title(
    "ECG Dataset - Processed ECG"
)

plt.grid(True)

plt.tight_layout()

plt.savefig(
    "02_processed_ecg.png",
    dpi=200
)

plt.show()


# ---------------------------------------------------------
# Plot 3: R-peak detection
# ---------------------------------------------------------

plt.figure(figsize=(12, 4))

plt.plot(
    time,
    processed
)

plt.plot(
    peak_times,
    processed[peak_indices],
    "o",
    label="R-peak"
)

plt.xlabel("Time (s)")
plt.ylabel("Processed ECG")

plt.title(
    "ECG Dataset - R-Peak Detection"
)

plt.legend()

plt.grid(True)

plt.tight_layout()

plt.savefig(
    "03_rpeak_detection.png",
    dpi=200
)

plt.show()


# ---------------------------------------------------------
# Finished
# ---------------------------------------------------------

print()
print("Validation files created:")
print("01_raw_ecg.png")
print("02_processed_ecg.png")
print("03_rpeak_detection.png")
print("processed_ecg_output.csv")
print("r_peak_locations.csv")
