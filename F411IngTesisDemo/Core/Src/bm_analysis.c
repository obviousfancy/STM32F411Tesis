/**
 * @file    bm_analysis.c
 * @brief   Categoría 4 — Análisis de nivel (BM10), f32
 */

#include "bm_analysis.h"
#include <math.h>
#include <string.h>

/**
 * @brief BM10 — RMS de 256 muestras, senoidal de EXACTAMENTE 1 ciclo.
 *
 * A diferencia de BM04/BM05-07 (440 Hz o 1 kHz, que no completan un
 * número entero de ciclos en 256 muestras), aquí la señal es
 * sin(2*pi*i/256) para i=0..255 -- exactamente 1 ciclo completo.
 * Esto elimina el efecto de ventana finita, permitiendo comparar
 * contra el valor teórico exacto sin ese margen de error adicional.
 *
 * Validación: RMS teórico = 1/√2 = 0.70710678..., tolerancia ±0.005
 * (la más estricta de todo el proyecto, precisamente porque aquí no
 * hay efecto de ventana que justifique un margen más amplio).
 */
BM_Result_t BM10_RMS_Run(void)
{
    BM_Result_t result = {0};
    uint32_t    cycle_samples[BM_ITERATIONS];
    float32_t   rms_buf[BM_BLOCK_SIZE];

    strncpy(result.name,    "BM10 RMS 256s", sizeof(result.name));
    strncpy(result.variant, "f32",           sizeof(result.variant));

    /* ─── Generar señal: 1 ciclo exacto en 256 muestras, ANTES del trigger ─── */
    for (uint32_t i = 0; i < BM_BLOCK_SIZE; i++) {
        rms_buf[i] = sinf(2.0f * PI * (float)i / (float)BM_BLOCK_SIZE);
    }

    for (uint32_t iter = 0; iter < BM_ITERATIONS; iter++) {

        float32_t rms_val;

        BM_TRIGGER_HIGH();
        BM_CYCLE_START();

        arm_rms_f32(rms_buf, BM_BLOCK_SIZE, &rms_val);

        cycle_samples[iter] = BM_CYCLE_STOP();
        BM_TRIGGER_LOW();

        if (iter == BM_ITERATIONS - 1U) {
            /* ─── Validación numérica: RMS teórico exacto = 1/√2 ─── */
            const float rms_theoretical = 0.70710678f;
            const float tolerance       = 0.005f;

            result.numeric_result = rms_val;
            result.valid = (fabsf(rms_val - rms_theoretical) <= tolerance) ? 1U : 0U;
        }
    }

    BM_Finalize(&result, cycle_samples, BM_ITERATIONS);

    return result;
}
