/**
 * @file    bm_synthesis.c
 * @brief   Categoría 1 — Síntesis de señal (BM01–BM04), solo f32
 */

#include "bm_synthesis.h"
#include <string.h>
#include <math.h>
/**
 * @brief BM01 — Genera 256 muestras de onda diente de sierra (sawtooth)
 *        usando acumulador de fase, mide ciclos DWT, valida rango [-1, 1].
 *
 * Frecuencia de prueba: 440 Hz (ver tabla de validación numérica del README)
 * Validación: arm_max_f32 > 0.9 Y arm_min_f32 < -0.9 (tolerancia ±0.05)
 */
BM_Result_t BM01_Sawtooth_Run(void)
{
    BM_Result_t result = {0};
    uint32_t    cycle_samples[BM_ITERATIONS];
    float32_t   out[BM_BLOCK_SIZE];

    const float freq_hz     = 440.0f;
    const float dt          = freq_hz / BM_SAMPLE_RATE;

    strncpy(result.name,    "BM01 Sawtooth",   sizeof(result.name) - 1);
    strncpy(result.variant, "f32",             sizeof(result.variant));

    for (uint32_t iter = 0; iter < BM_ITERATIONS; iter++) {

        float phase = 0.0f;

        BM_TRIGGER_HIGH();
        BM_CYCLE_START();

        for (uint32_t i = 0; i < BM_BLOCK_SIZE; i++) {
            out[i] = 2.0f * phase - 1.0f;
            phase += dt;
            if (phase >= 1.0f) {
                phase -= 1.0f;
            }
        }

        cycle_samples[iter] = BM_CYCLE_STOP();
        BM_TRIGGER_LOW();
    }

    BM_Finalize(&result, cycle_samples, BM_ITERATIONS);

    /* ─── Validación numérica: rango completo [-1, 1], tolerancia ±0.05 ─── */
    float32_t max_val, min_val;
    uint32_t  max_idx, min_idx;
    arm_max_f32(out, BM_BLOCK_SIZE, &max_val, &max_idx);
    arm_min_f32(out, BM_BLOCK_SIZE, &min_val, &min_idx);

    result.numeric_result = max_val; /* guardamos el pico máximo como referencia */
    result.valid = ((max_val > 0.9f) && (min_val < -0.9f)) ? 1U : 0U;

    return result;
}


/**
 * @brief BM02 — Genera 256 muestras de onda cuadrada (square) usando
 *        acumulador de fase, mide ciclos DWT, valida biestable exacto ±1.0f.
 *
 * Frecuencia de prueba: 440 Hz
 * Validación: TODOS los valores deben ser exactamente +1.0f o -1.0f (tolerancia 0)
 */
BM_Result_t BM02_Square_Run(void)
{
    BM_Result_t result = {0};
    uint32_t    cycle_samples[BM_ITERATIONS];
    float32_t   out[BM_BLOCK_SIZE];

    const float freq_hz = 440.0f;
    const float dt      = freq_hz / BM_SAMPLE_RATE;

    strncpy(result.name,    "BM02 Square", sizeof(result.name));
    strncpy(result.variant, "f32",         sizeof(result.variant));

    for (uint32_t iter = 0; iter < BM_ITERATIONS; iter++) {

        float phase = 0.0f;

        BM_TRIGGER_HIGH();
        BM_CYCLE_START();

        for (uint32_t i = 0; i < BM_BLOCK_SIZE; i++) {
            out[i] = (phase < 0.5f) ? 1.0f : -1.0f;
            phase += dt;
            if (phase >= 1.0f) {
                phase -= 1.0f;
            }
        }

        cycle_samples[iter] = BM_CYCLE_STOP();
        BM_TRIGGER_LOW();
    }

    BM_Finalize(&result, cycle_samples, BM_ITERATIONS);

    /* ─── Validación numérica: TODOS los valores deben ser ±1.0f exacto ─── */
    uint8_t all_bistable = 1U;
    for (uint32_t i = 0; i < BM_BLOCK_SIZE; i++) {
        if ((out[i] != 1.0f) && (out[i] != -1.0f)) {
            all_bistable = 0U;
            break;
        }
    }

    result.numeric_result = out[0]; /* primer valor como referencia */
    result.valid = all_bistable;

    return result;
}

/**
 * @brief BM03 — Genera 256 muestras de onda triangular usando
 *        acumulador de fase, mide ciclos DWT, valida RMS ≈ 1/√3.
 *
 * Frecuencia de prueba: 440 Hz
 * Validación: arm_rms_f32 ≈ 0.5774 (1/√3), tolerancia ±0.03
 */
BM_Result_t BM03_Triangle_Run(void)
{
    BM_Result_t result = {0};
    uint32_t    cycle_samples[BM_ITERATIONS];
    float32_t   out[BM_BLOCK_SIZE];

    const float freq_hz = 440.0f;
    const float dt      = freq_hz / BM_SAMPLE_RATE;

    strncpy(result.name,    "BM03 Triangle", sizeof(result.name));
    strncpy(result.variant, "f32",           sizeof(result.variant));

    for (uint32_t iter = 0; iter < BM_ITERATIONS; iter++) {

        float phase = 0.0f;

        BM_TRIGGER_HIGH();
        BM_CYCLE_START();

        for (uint32_t i = 0; i < BM_BLOCK_SIZE; i++) {
            out[i] = 1.0f - 4.0f * fabsf(phase - 0.5f);
            phase += dt;
            if (phase >= 1.0f) {
                phase -= 1.0f;
            }
        }

        cycle_samples[iter] = BM_CYCLE_STOP();
        BM_TRIGGER_LOW();
    }

    BM_Finalize(&result, cycle_samples, BM_ITERATIONS);

    /* ─── Validación numérica: RMS ≈ 1/√3 = 0.5774, tolerancia ±0.03 ─── */
    float32_t rms_val;
    arm_rms_f32(out, BM_BLOCK_SIZE, &rms_val);

    const float rms_theoretical = 0.57735027f; /* 1/sqrt(3) */
    const float tolerance       = 0.03f;

    result.numeric_result = rms_val;
    result.valid = (fabsf(rms_val - rms_theoretical) <= tolerance) ? 1U : 0U;

    return result;
}

/**
 * @brief BM04 — Genera 256 muestras de onda senoidal usando sinf()
 *        de libm (no CMSIS-DSP), mide ciclos DWT, valida RMS ≈ 1/√2.
 *
 * Este es el CONTRASTE CLAVE de la Categoría 1: sinf() es una función
 * trascendental (típicamente implementada por reducción de rango +
 * aproximación polinomial en libm), frente a la aritmética pura de
 * BM01-03. Se espera un costo de ~10-15x en ciclos.
 *
 * Frecuencia de prueba: 440 Hz
 * Validación: arm_rms_f32 ≈ 0.7071 (1/√2), tolerancia ±0.01
 */
BM_Result_t BM04_Sine_Run(void)
{
    BM_Result_t result = {0};
    uint32_t    cycle_samples[BM_ITERATIONS];
    float32_t   out[BM_BLOCK_SIZE];

    const float freq_hz = 440.0f;
    const float two_pi  = 6.28318530f;

    strncpy(result.name,    "BM04 Sine sinf()", sizeof(result.name));
    strncpy(result.variant, "f32",              sizeof(result.variant));

    for (uint32_t iter = 0; iter < BM_ITERATIONS; iter++) {

        BM_TRIGGER_HIGH();
        BM_CYCLE_START();

        for (uint32_t i = 0; i < BM_BLOCK_SIZE; i++) {
            out[i] = sinf(two_pi * freq_hz * (float)i / BM_SAMPLE_RATE);
        }

        cycle_samples[iter] = BM_CYCLE_STOP();
        BM_TRIGGER_LOW();
    }

    BM_Finalize(&result, cycle_samples, BM_ITERATIONS);

    /* ─── Validación numérica: RMS ≈ 1/√2 = 0.7071, tolerancia ±0.01 ─── */
    float32_t rms_val;
    arm_rms_f32(out, BM_BLOCK_SIZE, &rms_val);

    const float rms_theoretical = 0.70710678f; /* 1/sqrt(2) */
    const float tolerance       = 0.01f;

    result.numeric_result = rms_val;
    result.valid = (fabsf(rms_val - rms_theoretical) <= tolerance) ? 1U : 0U;

    return result;
}
