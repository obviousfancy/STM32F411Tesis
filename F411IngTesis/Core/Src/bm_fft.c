/**
 * @file    bm_fft.c
 * @brief   Categoría 2 — Análisis espectral FFT (BM05–BM07), f32 vs q15
 *
 * Las variantes q15 (b) además calculan un espectro f32 de referencia
 * (sin medir sus ciclos, solo como base de comparación) para reportar
 * precision_error_pct.
 *
 * NOTA METODOLÓGICA IMPORTANTE SOBRE LA COMPARACIÓN DE PRECISIÓN:
 * Se intentó inicialmente comparar la magnitud absoluta del bin
 * dominante entre f32 y q15, usando el factor de escala documentado
 * de arm_cmplx_mag_q15 (formato Q2.14, dividir entre 16384). Esto NO
 * dio un resultado coherente: la magnitud f32 de referencia resultó
 * ser ~26 (mucho mayor que 1.0), porque arm_cfft_f32 NO normaliza su
 * salida -- para una señal senoidal de amplitud 1, el bin dominante
 * crece proporcionalmente a N/2. Del lado q15, arm_cfft_q15 aplica
 * un escalado automático interno por etapa (tipo "block floating
 * point") para evitar overflow, dejando el resultado en una escala
 * interna distinta que NO se relaciona con la de f32 mediante un
 * factor fijo simple.
 *
 * Por esta razón, la comparación válida no es de magnitud absoluta,
 * sino de PROPORCIÓN: qué fracción de la energía espectral total
 * está concentrada en el bin dominante, calculada de forma
 * independiente en cada dominio (f32 con sus propios valores, q15
 * con los suyos). Esta proporción es adimensional y no depende de la
 * escala interna arbitraria de cada implementación, por lo que sí es
 * honestamente comparable entre ambas variantes.
 */

#include "bm_fft.h"
#include <string.h>
#include <math.h>

#define FFT64_LEN   64U
#define FFT128_LEN  128U
#define FFT256_LEN  256U

/**
 * @brief BM05a — FFT compleja de 64 puntos, f32.
 *
 * Señal de entrada: senoidal de 1 kHz, generada ANTES del trigger
 * (no se mide la generación, solo la FFT en sí).
 * Buffer intercalado {real, imag} → 128 floats para 64 puntos complejos.
 *
 * Validación: bin de magnitud máxima debe ser bin 1 (±1 bin), según
 * resolución espectral Fs/N = 44100/64 ≈ 689 Hz/bin.
 */
BM_Result_t BM05a_FFT64_F32_Run(void)
{
    BM_Result_t result = {0};
    uint32_t    cycle_samples[BM_ITERATIONS];
    float32_t   fft_buf[FFT64_LEN * 2U]; /* intercalado real/imag */

    strncpy(result.name,    "BM05a FFT 64pt", sizeof(result.name));
    strncpy(result.variant, "f32",             sizeof(result.variant));

    /* ─── Generar señal de prueba: senoidal 1 kHz, ANTES del trigger ─── */
    for (uint32_t i = 0; i < FFT64_LEN; i++) {
        fft_buf[2U * i]     = sinf(2.0f * PI * 1000.0f * (float)i / BM_SAMPLE_RATE); /* real */
        fft_buf[2U * i + 1U] = 0.0f;                                                 /* imag */
    }

    /* ─── Inicializar instancia CFFT usando tabla precalculada CMSIS ─── */
    arm_cfft_instance_f32 fft_instance;
    arm_status status = arm_cfft_init_f32(&fft_instance, FFT64_LEN);
    if (status != ARM_MATH_SUCCESS) {
        result.valid = 0U;
        return result;
    }

    for (uint32_t iter = 0; iter < BM_ITERATIONS; iter++) {

        float32_t work_buf[FFT64_LEN * 2U];
        memcpy(work_buf, fft_buf, sizeof(work_buf)); /* CFFT opera in-place */

        BM_TRIGGER_HIGH();
        BM_CYCLE_START();

        arm_cfft_f32(&fft_instance, work_buf, 0U /* forward */, 1U /* bit reversal */);

        cycle_samples[iter] = BM_CYCLE_STOP();
        BM_TRIGGER_LOW();

        if (iter == BM_ITERATIONS - 1U) {
            /* ─── Validación numérica solo en la última iteración ─── */
            float32_t mag_buf[FFT64_LEN];
            arm_cmplx_mag_f32(work_buf, mag_buf, FFT64_LEN);

            float32_t max_val;
            uint32_t  max_idx;
            arm_max_f32(mag_buf, FFT64_LEN / 2U, &max_val, &max_idx); /* solo mitad útil (espejo) */

            result.numeric_result = (float)max_idx;
            /* Esperado: bin 1 ± 1 */
            result.valid = ((max_idx >= 0U) && (max_idx <= 2U)) ? 1U : 0U;
        }
    }

    BM_Finalize(&result, cycle_samples, BM_ITERATIONS);

    return result;
}

/**
 * @brief BM05b — FFT compleja de 64 puntos, q15.
 *
 * Misma señal de entrada que BM05a (senoidal 1 kHz), convertida a q15
 * ANTES del trigger. Permite comparación directa f32 vs q15 en el
 * mismo hardware, mismo compilador, misma metodología.
 *
 * Validación: bin de magnitud máxima debe ser bin 1 (±1 bin), igual
 * criterio que BM05a.
 *
 * Además calcula precision_error_pct: diferencia relativa porcentual
 * entre la PROPORCIÓN de energía concentrada en el bin dominante,
 * calculada independientemente en f32 y en q15 (ver nota metodológica
 * al inicio del archivo sobre por qué no se comparan magnitudes
 * absolutas).
 */
BM_Result_t BM05b_FFT64_Q15_Run(void)
{
    BM_Result_t result = {0};
    uint32_t    cycle_samples[BM_ITERATIONS];
    float32_t   fft_buf_f32[FFT64_LEN * 2U];
    q15_t       fft_buf_q15[FFT64_LEN * 2U];
    q15_t       mag_buf[FFT64_LEN]; /* se conserva el resultado de la última iteración */

    strncpy(result.name,    "BM05b FFT 64pt", sizeof(result.name));
    strncpy(result.variant, "q15",            sizeof(result.variant));

    /* ─── Generar señal de prueba: senoidal 1 kHz, ANTES del trigger ─── */
    for (uint32_t i = 0; i < FFT64_LEN; i++) {
        fft_buf_f32[2U * i]      = sinf(2.0f * PI * 1000.0f * (float)i / BM_SAMPLE_RATE); /* real */
        fft_buf_f32[2U * i + 1U] = 0.0f;                                                  /* imag */
    }

    /* ─── Convertir a q15, ANTES del trigger (no se mide la conversión) ─── */
    BM_F32toQ15(fft_buf_f32, fft_buf_q15, FFT64_LEN * 2U);

    /* ─── Inicializar instancia CFFT q15 ─── */
    arm_cfft_instance_q15 fft_instance;
    arm_status status = arm_cfft_init_q15(&fft_instance, FFT64_LEN);
    if (status != ARM_MATH_SUCCESS) {
        result.valid = 0U;
        return result;
    }

    for (uint32_t iter = 0; iter < BM_ITERATIONS; iter++) {

        q15_t work_buf[FFT64_LEN * 2U];
        memcpy(work_buf, fft_buf_q15, sizeof(work_buf)); /* CFFT opera in-place */

        BM_TRIGGER_HIGH();
        BM_CYCLE_START();

        arm_cfft_q15(&fft_instance, work_buf, 0U /* forward */, 1U /* bit reversal */);

        cycle_samples[iter] = BM_CYCLE_STOP();
        BM_TRIGGER_LOW();

        if (iter == BM_ITERATIONS - 1U) {
            arm_cmplx_mag_q15(work_buf, mag_buf, FFT64_LEN);
        }
    }

    BM_Finalize(&result, cycle_samples, BM_ITERATIONS);

    /* ─── Validación de bin dominante ─── */
    q15_t    max_val_q15;
    uint32_t max_idx;
    arm_max_q15(mag_buf, FFT64_LEN / 2U, &max_val_q15, &max_idx);

    result.numeric_result = (float)max_idx;
    /* Esperado: bin 1 ± 1 */
    uint8_t bin_ok = ((max_idx >= 0U) && (max_idx <= 2U)) ? 1U : 0U;

    /* ─── Espectro f32 de referencia (NO se miden sus ciclos, solo precisión) ─── */
    float32_t work_buf_ref[FFT64_LEN * 2U];
    memcpy(work_buf_ref, fft_buf_f32, sizeof(work_buf_ref));

    arm_cfft_instance_f32 fft_instance_ref;
    arm_cfft_init_f32(&fft_instance_ref, FFT64_LEN);
    arm_cfft_f32(&fft_instance_ref, work_buf_ref, 0U, 1U);

    float32_t mag_buf_ref[FFT64_LEN];
    arm_cmplx_mag_f32(work_buf_ref, mag_buf_ref, FFT64_LEN);

    /* ─── Proporción de energía en el bin dominante, calculada
           independientemente en cada dominio (f32 y q15) ─── */
    float32_t total_energy_f32 = 0.0f;
    uint32_t  total_energy_q15 = 0U;
    for (uint32_t k = 0; k < (FFT64_LEN / 2U); k++) {
        total_energy_f32 += mag_buf_ref[k];
        total_energy_q15 += (uint32_t)mag_buf[k];
    }

    float32_t proportion_f32 = (total_energy_f32 > 0.0f)
                                ? (mag_buf_ref[max_idx] / total_energy_f32)
                                : 0.0f;
    float32_t proportion_q15 = (total_energy_q15 > 0U)
                                ? ((float32_t)mag_buf[max_idx] / (float32_t)total_energy_q15)
                                : 0.0f;

    float32_t error_abs = fabsf(proportion_f32 - proportion_q15);
    result.precision_error_pct = (proportion_f32 > 0.0f)
                                  ? (error_abs / proportion_f32) * 100.0f
                                  : 0.0f;

    result.valid = bin_ok;

    return result;
}

/**
 * @brief BM06a — FFT compleja de 128 puntos, f32.
 *
 * Validación: bin de magnitud máxima debe ser bin 3 (±1 bin), según
 * resolución espectral Fs/N = 44100/128 ≈ 345 Hz/bin.
 */
BM_Result_t BM06a_FFT128_F32_Run(void)
{
    BM_Result_t result = {0};
    uint32_t    cycle_samples[BM_ITERATIONS];
    float32_t   fft_buf[FFT128_LEN * 2U];

    strncpy(result.name,    "BM06a FFT 128pt", sizeof(result.name));
    strncpy(result.variant, "f32",              sizeof(result.variant));

    for (uint32_t i = 0; i < FFT128_LEN; i++) {
        fft_buf[2U * i]      = sinf(2.0f * PI * 1000.0f * (float)i / BM_SAMPLE_RATE);
        fft_buf[2U * i + 1U] = 0.0f;
    }

    arm_cfft_instance_f32 fft_instance;
    arm_status status = arm_cfft_init_f32(&fft_instance, FFT128_LEN);
    if (status != ARM_MATH_SUCCESS) {
        result.valid = 0U;
        return result;
    }

    for (uint32_t iter = 0; iter < BM_ITERATIONS; iter++) {

        float32_t work_buf[FFT128_LEN * 2U];
        memcpy(work_buf, fft_buf, sizeof(work_buf));

        BM_TRIGGER_HIGH();
        BM_CYCLE_START();

        arm_cfft_f32(&fft_instance, work_buf, 0U, 1U);

        cycle_samples[iter] = BM_CYCLE_STOP();
        BM_TRIGGER_LOW();

        if (iter == BM_ITERATIONS - 1U) {
            float32_t mag_buf[FFT128_LEN];
            arm_cmplx_mag_f32(work_buf, mag_buf, FFT128_LEN);

            float32_t max_val;
            uint32_t  max_idx;
            arm_max_f32(mag_buf, FFT128_LEN / 2U, &max_val, &max_idx);

            result.numeric_result = (float)max_idx;
            /* Esperado: bin 3 ± 1 */
            result.valid = ((max_idx >= 2U) && (max_idx <= 4U)) ? 1U : 0U;
        }
    }

    BM_Finalize(&result, cycle_samples, BM_ITERATIONS);

    return result;
}

/**
 * @brief BM06b — FFT compleja de 128 puntos, q15.
 *
 * Además calcula precision_error_pct por proporción de energía
 * (ver documentación de BM05b y la nota metodológica del archivo).
 */
BM_Result_t BM06b_FFT128_Q15_Run(void)
{
    BM_Result_t result = {0};
    uint32_t    cycle_samples[BM_ITERATIONS];
    float32_t   fft_buf_f32[FFT128_LEN * 2U];
    q15_t       fft_buf_q15[FFT128_LEN * 2U];
    q15_t       mag_buf[FFT128_LEN];

    strncpy(result.name,    "BM06b FFT 128pt", sizeof(result.name));
    strncpy(result.variant, "q15",              sizeof(result.variant));

    for (uint32_t i = 0; i < FFT128_LEN; i++) {
        fft_buf_f32[2U * i]      = sinf(2.0f * PI * 1000.0f * (float)i / BM_SAMPLE_RATE);
        fft_buf_f32[2U * i + 1U] = 0.0f;
    }

    BM_F32toQ15(fft_buf_f32, fft_buf_q15, FFT128_LEN * 2U);

    arm_cfft_instance_q15 fft_instance;
    arm_status status = arm_cfft_init_q15(&fft_instance, FFT128_LEN);
    if (status != ARM_MATH_SUCCESS) {
        result.valid = 0U;
        return result;
    }

    for (uint32_t iter = 0; iter < BM_ITERATIONS; iter++) {

        q15_t work_buf[FFT128_LEN * 2U];
        memcpy(work_buf, fft_buf_q15, sizeof(work_buf));

        BM_TRIGGER_HIGH();
        BM_CYCLE_START();

        arm_cfft_q15(&fft_instance, work_buf, 0U, 1U);

        cycle_samples[iter] = BM_CYCLE_STOP();
        BM_TRIGGER_LOW();

        if (iter == BM_ITERATIONS - 1U) {
            arm_cmplx_mag_q15(work_buf, mag_buf, FFT128_LEN);
        }
    }

    BM_Finalize(&result, cycle_samples, BM_ITERATIONS);

    /* ─── Validación de bin dominante ─── */
    q15_t    max_val_q15;
    uint32_t max_idx;
    arm_max_q15(mag_buf, FFT128_LEN / 2U, &max_val_q15, &max_idx);

    result.numeric_result = (float)max_idx;
    /* Esperado: bin 3 ± 1 */
    uint8_t bin_ok = ((max_idx >= 2U) && (max_idx <= 4U)) ? 1U : 0U;

    /* ─── Espectro f32 de referencia (NO se miden sus ciclos, solo precisión) ─── */
    float32_t work_buf_ref[FFT128_LEN * 2U];
    memcpy(work_buf_ref, fft_buf_f32, sizeof(work_buf_ref));

    arm_cfft_instance_f32 fft_instance_ref;
    arm_cfft_init_f32(&fft_instance_ref, FFT128_LEN);
    arm_cfft_f32(&fft_instance_ref, work_buf_ref, 0U, 1U);

    float32_t mag_buf_ref[FFT128_LEN];
    arm_cmplx_mag_f32(work_buf_ref, mag_buf_ref, FFT128_LEN);

    /* ─── Proporción de energía en el bin dominante, por dominio ─── */
    float32_t total_energy_f32 = 0.0f;
    uint32_t  total_energy_q15 = 0U;
    for (uint32_t k = 0; k < (FFT128_LEN / 2U); k++) {
        total_energy_f32 += mag_buf_ref[k];
        total_energy_q15 += (uint32_t)mag_buf[k];
    }

    float32_t proportion_f32 = (total_energy_f32 > 0.0f)
                                ? (mag_buf_ref[max_idx] / total_energy_f32)
                                : 0.0f;
    float32_t proportion_q15 = (total_energy_q15 > 0U)
                                ? ((float32_t)mag_buf[max_idx] / (float32_t)total_energy_q15)
                                : 0.0f;

    float32_t error_abs = fabsf(proportion_f32 - proportion_q15);
    result.precision_error_pct = (proportion_f32 > 0.0f)
                                  ? (error_abs / proportion_f32) * 100.0f
                                  : 0.0f;

    result.valid = bin_ok;

    return result;
}

/**
 * @brief BM07a — FFT compleja de 256 puntos, f32.
 *
 * Validación: bin de magnitud máxima debe ser bin 6 (±1 bin), según
 * resolución espectral Fs/N = 44100/256 ≈ 172 Hz/bin.
 */
BM_Result_t BM07a_FFT256_F32_Run(void)
{
    BM_Result_t result = {0};
    uint32_t    cycle_samples[BM_ITERATIONS];
    float32_t   fft_buf[FFT256_LEN * 2U];

    strncpy(result.name,    "BM07a FFT 256pt", sizeof(result.name));
    strncpy(result.variant, "f32",              sizeof(result.variant));

    for (uint32_t i = 0; i < FFT256_LEN; i++) {
        fft_buf[2U * i]      = sinf(2.0f * PI * 1000.0f * (float)i / BM_SAMPLE_RATE);
        fft_buf[2U * i + 1U] = 0.0f;
    }

    arm_cfft_instance_f32 fft_instance;
    arm_status status = arm_cfft_init_f32(&fft_instance, FFT256_LEN);
    if (status != ARM_MATH_SUCCESS) {
        result.valid = 0U;
        return result;
    }

    for (uint32_t iter = 0; iter < BM_ITERATIONS; iter++) {

        float32_t work_buf[FFT256_LEN * 2U];
        memcpy(work_buf, fft_buf, sizeof(work_buf));

        BM_TRIGGER_HIGH();
        BM_CYCLE_START();

        arm_cfft_f32(&fft_instance, work_buf, 0U, 1U);

        cycle_samples[iter] = BM_CYCLE_STOP();
        BM_TRIGGER_LOW();

        if (iter == BM_ITERATIONS - 1U) {
            float32_t mag_buf[FFT256_LEN];
            arm_cmplx_mag_f32(work_buf, mag_buf, FFT256_LEN);

            float32_t max_val;
            uint32_t  max_idx;
            arm_max_f32(mag_buf, FFT256_LEN / 2U, &max_val, &max_idx);

            result.numeric_result = (float)max_idx;
            /* Esperado: bin 6 ± 1 */
            result.valid = ((max_idx >= 5U) && (max_idx <= 7U)) ? 1U : 0U;
        }
    }

    BM_Finalize(&result, cycle_samples, BM_ITERATIONS);

    return result;
}

/**
 * @brief BM07b — FFT compleja de 256 puntos, q15.
 *
 * Además calcula precision_error_pct por proporción de energía
 * (ver documentación de BM05b y la nota metodológica del archivo).
 */
BM_Result_t BM07b_FFT256_Q15_Run(void)
{
    BM_Result_t result = {0};
    uint32_t    cycle_samples[BM_ITERATIONS];
    float32_t   fft_buf_f32[FFT256_LEN * 2U];
    q15_t       fft_buf_q15[FFT256_LEN * 2U];
    q15_t       mag_buf[FFT256_LEN];

    strncpy(result.name,    "BM07b FFT 256pt", sizeof(result.name));
    strncpy(result.variant, "q15",              sizeof(result.variant));

    for (uint32_t i = 0; i < FFT256_LEN; i++) {
        fft_buf_f32[2U * i]      = sinf(2.0f * PI * 1000.0f * (float)i / BM_SAMPLE_RATE);
        fft_buf_f32[2U * i + 1U] = 0.0f;
    }

    BM_F32toQ15(fft_buf_f32, fft_buf_q15, FFT256_LEN * 2U);

    arm_cfft_instance_q15 fft_instance;
    arm_status status = arm_cfft_init_q15(&fft_instance, FFT256_LEN);
    if (status != ARM_MATH_SUCCESS) {
        result.valid = 0U;
        return result;
    }

    for (uint32_t iter = 0; iter < BM_ITERATIONS; iter++) {

        q15_t work_buf[FFT256_LEN * 2U];
        memcpy(work_buf, fft_buf_q15, sizeof(work_buf));

        BM_TRIGGER_HIGH();
        BM_CYCLE_START();

        arm_cfft_q15(&fft_instance, work_buf, 0U, 1U);

        cycle_samples[iter] = BM_CYCLE_STOP();
        BM_TRIGGER_LOW();

        if (iter == BM_ITERATIONS - 1U) {
            arm_cmplx_mag_q15(work_buf, mag_buf, FFT256_LEN);
        }
    }

    BM_Finalize(&result, cycle_samples, BM_ITERATIONS);

    /* ─── Validación de bin dominante ─── */
    q15_t    max_val_q15;
    uint32_t max_idx;
    arm_max_q15(mag_buf, FFT256_LEN / 2U, &max_val_q15, &max_idx);

    result.numeric_result = (float)max_idx;
    /* Esperado: bin 6 ± 1 */
    uint8_t bin_ok = ((max_idx >= 5U) && (max_idx <= 7U)) ? 1U : 0U;

    /* ─── Espectro f32 de referencia (NO se miden sus ciclos, solo precisión) ─── */
    float32_t work_buf_ref[FFT256_LEN * 2U];
    memcpy(work_buf_ref, fft_buf_f32, sizeof(work_buf_ref));

    arm_cfft_instance_f32 fft_instance_ref;
    arm_cfft_init_f32(&fft_instance_ref, FFT256_LEN);
    arm_cfft_f32(&fft_instance_ref, work_buf_ref, 0U, 1U);

    float32_t mag_buf_ref[FFT256_LEN];
    arm_cmplx_mag_f32(work_buf_ref, mag_buf_ref, FFT256_LEN);

    /* ─── Proporción de energía en el bin dominante, por dominio ─── */
    float32_t total_energy_f32 = 0.0f;
    uint32_t  total_energy_q15 = 0U;
    for (uint32_t k = 0; k < (FFT256_LEN / 2U); k++) {
        total_energy_f32 += mag_buf_ref[k];
        total_energy_q15 += (uint32_t)mag_buf[k];
    }

    float32_t proportion_f32 = (total_energy_f32 > 0.0f)
                                ? (mag_buf_ref[max_idx] / total_energy_f32)
                                : 0.0f;
    float32_t proportion_q15 = (total_energy_q15 > 0U)
                                ? ((float32_t)mag_buf[max_idx] / (float32_t)total_energy_q15)
                                : 0.0f;

    float32_t error_abs = fabsf(proportion_f32 - proportion_q15);
    result.precision_error_pct = (proportion_f32 > 0.0f)
                                  ? (error_abs / proportion_f32) * 100.0f
                                  : 0.0f;

    result.valid = bin_ok;

    return result;
}
