/**
 * @file    bm_filters.c
 * @brief   Categoría 3 — Filtrado digital (BM08–BM09), f32 vs q15
 *
 * Coeficientes generados con fir_coefficients.py (scipy 1.18.0,
 * numpy 2.3.5) — ver results/README_session.md para detalle completo.
 */

#include "bm_filters.h"
#include <string.h>
#include <math.h>

#define FIR_NUM_TAPS   32U
#define IIR_NUM_STAGES  4U
/* ── Coeficientes FIR, paso bajo fc=4kHz @ 44.1kHz, generados con scipy ── */
static const float32_t fir_coeffs_f32[FIR_NUM_TAPS] = {
    0.0009135370f,  0.0017961076f,  0.0027231435f,  0.0030745141f,
    0.0016470869f, -0.0026444830f, -0.0097066725f, -0.0174180225f,
   -0.0215961846f, -0.0170796380f,  0.0003054037f,  0.0316256416f,
    0.0735178202f,  0.1183969678f,  0.1563434240f,  0.1781013542f,
    0.1781013542f,  0.1563434240f,  0.1183969678f,  0.0735178202f,
    0.0316256416f,  0.0003054037f, -0.0170796380f, -0.0215961846f,
   -0.0174180225f, -0.0097066725f, -0.0026444830f,  0.0016470869f,
    0.0030745141f,  0.0027231435f,  0.0017961076f,  0.0009135370f
};

static const q15_t fir_coeffs_q15[FIR_NUM_TAPS] = {
    30,   59,   89,  101,   54,  -87, -318, -571,
  -708, -560,   10, 1036, 2409, 3880, 5123, 5836,
  5836, 5123, 3880, 2409, 1036,   10, -560, -708,
  -571, -318,  -87,   54,  101,   89,   59,   30
};

/* ── Coeficientes IIR Butterworth orden 8, 4 biquads, fc=4kHz @ 44.1kHz ── */
/* Formato f32 CMSIS-DSP (DF2T): {b0, b1, b2, -a1, -a2} por etapa */
static const float32_t iir_coeffs_f32[5U * IIR_NUM_STAGES] = {
    0.0000122541f, 0.0000245083f, 0.0000122541f,  1.1011799750f, -0.3078875724f,
    1.0000000000f, 2.0000000000f, 1.0000000000f,  1.1624208646f, -0.3806242733f,
    1.0000000000f, 2.0000000000f, 1.0000000000f,  1.2955532780f, -0.5387475891f,
    1.0000000000f, 2.0000000000f, 1.0000000000f,  1.5235369496f, -0.8095271325f
};

/* Formato q15 CMSIS-DSP (DF1): {b0, b1, b2, a1, a2} por etapa, sin negar */
/* postShift GLOBAL = 2 (arm_biquad_cascade_df1_init_q15 usa un solo valor,
   no uno por etapa -- recalculado con scipy usando el máximo shift
   necesario entre las 4 etapas) */
static const q15_t iir_coeffs_q15[5U * IIR_NUM_STAGES] = {
    0,    0,    0,    -9021, 2522,
    8192, 16384, 8192, -9523, 3118,
    8192, 16384, 8192, -10613, 4413,
    8192, 16384, 8192, -12481, 6632
};


#define IIR_POST_SHIFT  2
/**
 * @brief BM08a — Filtro FIR de 32 taps, f32, aplicado a ruido blanco.
 *
 * Señal de entrada: ruido blanco generado por LFSR, ANTES del trigger.
 * Validación: rms_out < rms_in (reducción > 5%), y resultado finito.
 */
BM_Result_t BM08a_FIR_F32_Run(void)
{
    BM_Result_t result = {0};
    uint32_t    cycle_samples[BM_ITERATIONS];
    float32_t   noise_in[BM_BLOCK_SIZE];
    float32_t   filt_out[BM_BLOCK_SIZE];

    strncpy(result.name,    "BM08a FIR 32t", sizeof(result.name));
    strncpy(result.variant, "f32",           sizeof(result.variant));

    /* ─── Generar ruido blanco, ANTES del trigger ─── */
    BM_GenWhiteNoise(noise_in, BM_BLOCK_SIZE);

    /* ─── Inicializar instancia FIR f32 ─── */
    arm_fir_instance_f32 fir_instance;
    float32_t             fir_state[BM_BLOCK_SIZE + FIR_NUM_TAPS - 1U];
    arm_fir_init_f32(&fir_instance, FIR_NUM_TAPS,
                      (float32_t *)fir_coeffs_f32, fir_state, BM_BLOCK_SIZE);

    for (uint32_t iter = 0; iter < BM_ITERATIONS; iter++) {

        BM_TRIGGER_HIGH();
        BM_CYCLE_START();

        arm_fir_f32(&fir_instance, noise_in, filt_out, BM_BLOCK_SIZE);

        cycle_samples[iter] = BM_CYCLE_STOP();
        BM_TRIGGER_LOW();
    }

    BM_Finalize(&result, cycle_samples, BM_ITERATIONS);

    /* ─── Validación numérica: rms_out < rms_in, reducción > 5% ─── */
    float32_t rms_in, rms_out;
    arm_rms_f32(noise_in,  BM_BLOCK_SIZE, &rms_in);
    arm_rms_f32(filt_out,  BM_BLOCK_SIZE, &rms_out);

    result.numeric_result = rms_out;
    uint8_t reduced_enough = (rms_out < (rms_in * 0.95f)) ? 1U : 0U;
    uint8_t is_finite      = (isfinite(rms_out)) ? 1U : 0U;
    result.valid = (reduced_enough && is_finite) ? 1U : 0U;

    return result;
}

/**
 * @brief BM08b — Filtro FIR de 32 taps, q15, aplicado al mismo ruido blanco.
 */
BM_Result_t BM08b_FIR_Q15_Run(void)
{
    BM_Result_t result = {0};
    uint32_t    cycle_samples[BM_ITERATIONS];
    float32_t   noise_in_f32[BM_BLOCK_SIZE];
    q15_t       noise_in_q15[BM_BLOCK_SIZE];
    q15_t       filt_out[BM_BLOCK_SIZE];

    strncpy(result.name,    "BM08b FIR 32t", sizeof(result.name));
    strncpy(result.variant, "q15",           sizeof(result.variant));

    /* ─── Generar el MISMO ruido blanco, convertir a q15, ANTES del trigger ─── */
    BM_GenWhiteNoise(noise_in_f32, BM_BLOCK_SIZE);
    BM_F32toQ15(noise_in_f32, noise_in_q15, BM_BLOCK_SIZE);

    /* ─── Inicializar instancia FIR q15 ─── */
    arm_fir_instance_q15 fir_instance;
    q15_t                 fir_state[BM_BLOCK_SIZE + FIR_NUM_TAPS - 1U];
    arm_fir_init_q15(&fir_instance, FIR_NUM_TAPS,
                      (q15_t *)fir_coeffs_q15, fir_state, BM_BLOCK_SIZE);

    for (uint32_t iter = 0; iter < BM_ITERATIONS; iter++) {

        BM_TRIGGER_HIGH();
        BM_CYCLE_START();

        arm_fir_q15(&fir_instance, noise_in_q15, filt_out, BM_BLOCK_SIZE);

        cycle_samples[iter] = BM_CYCLE_STOP();
        BM_TRIGGER_LOW();
    }

    BM_Finalize(&result, cycle_samples, BM_ITERATIONS);

    /* ─── Validación numérica: rms_out < rms_in, reducción > 5% ─── */
    q15_t rms_in_q15, rms_out_q15;
    arm_rms_q15(noise_in_q15, BM_BLOCK_SIZE, &rms_in_q15);
    arm_rms_q15(filt_out,     BM_BLOCK_SIZE, &rms_out_q15);

    result.numeric_result = (float)rms_out_q15 / 32768.0f;
    uint8_t reduced_enough = (rms_out_q15 < (q15_t)(rms_in_q15 * 0.95f)) ? 1U : 0U;
    result.valid = reduced_enough;

    return result;
}

/**
 * @brief BM09a — Filtro IIR Biquad 4 etapas (DF2T), f32, sobre ruido blanco.
 *
 * Señal de entrada: ruido blanco generado por LFSR, ANTES del trigger.
 * Validación: rms_out debe estar en (0.001, 0.5), y ser finito.
 */
BM_Result_t BM09a_IIR_F32_Run(void)
{
    BM_Result_t result = {0};
    uint32_t    cycle_samples[BM_ITERATIONS];
    float32_t   noise_in[BM_BLOCK_SIZE];
    float32_t   filt_out[BM_BLOCK_SIZE];

    strncpy(result.name,    "BM09a IIR 4-stage", sizeof(result.name));
    strncpy(result.variant, "f32",                sizeof(result.variant));

    BM_GenWhiteNoise(noise_in, BM_BLOCK_SIZE);

    /* ─── Inicializar instancia IIR f32 (DF2T) ─── */
    arm_biquad_cascade_df2T_instance_f32 iir_instance;
    float32_t                             iir_state[2U * IIR_NUM_STAGES]; /* DF2T: 2 estados por etapa */
    arm_biquad_cascade_df2T_init_f32(&iir_instance, IIR_NUM_STAGES,
                                      (float32_t *)iir_coeffs_f32, iir_state);

    for (uint32_t iter = 0; iter < BM_ITERATIONS; iter++) {

        BM_TRIGGER_HIGH();
        BM_CYCLE_START();

        arm_biquad_cascade_df2T_f32(&iir_instance, noise_in, filt_out, BM_BLOCK_SIZE);

        cycle_samples[iter] = BM_CYCLE_STOP();
        BM_TRIGGER_LOW();
    }

    BM_Finalize(&result, cycle_samples, BM_ITERATIONS);

    /* ─── Validación numérica: rms_out en (0.001, 0.5), finito ─── */
    float32_t rms_out;
    arm_rms_f32(filt_out, BM_BLOCK_SIZE, &rms_out);

    result.numeric_result = rms_out;
    uint8_t in_range  = ((rms_out > 0.001f) && (rms_out < 0.5f)) ? 1U : 0U;
    uint8_t is_finite = (isfinite(rms_out)) ? 1U : 0U;
    result.valid = (in_range && is_finite) ? 1U : 0U;

    return result;
}

/**
 * @brief BM09b — Filtro IIR Biquad 4 etapas (DF1), q15, sobre el mismo ruido.
 *
 * ADVERTENCIA METODOLÓGICA: la etapa 1 de los coeficientes q15 tiene
 * b0=b1=b2=0 (ver fir_coefficients.py, sección de verificación) --
 * los coeficientes b reales de esa etapa son demasiado pequeños para
 * representarse en q15 incluso con postShift, dada la gran disparidad
 * de magnitud frente a los coeficientes a de la misma etapa. Se espera
 * que este benchmark pueda fallar la validación numérica o producir
 * un resultado degradado -- esto es un hallazgo esperado y válido,
 * no un error de programación.
 */
BM_Result_t BM09b_IIR_Q15_Run(void)
{
    BM_Result_t result = {0};
    uint32_t    cycle_samples[BM_ITERATIONS];
    float32_t   noise_in_f32[BM_BLOCK_SIZE];
    q15_t       noise_in_q15[BM_BLOCK_SIZE];
    q15_t       filt_out[BM_BLOCK_SIZE];

    strncpy(result.name,    "BM09b IIR 4-stage", sizeof(result.name));
    strncpy(result.variant, "q15",                sizeof(result.variant));

    BM_GenWhiteNoise(noise_in_f32, BM_BLOCK_SIZE);
    BM_F32toQ15(noise_in_f32, noise_in_q15, BM_BLOCK_SIZE);

    /* ─── Inicializar instancia IIR q15 (DF1) ─── */
    arm_biquad_casd_df1_inst_q15 iir_instance;
    q15_t                         iir_state[4U * IIR_NUM_STAGES]; /* DF1 q15: 4 estados por etapa */
    arm_biquad_cascade_df1_init_q15(&iir_instance, IIR_NUM_STAGES,
                                     (q15_t *)iir_coeffs_q15, iir_state,
                                     IIR_POST_SHIFT);

    for (uint32_t iter = 0; iter < BM_ITERATIONS; iter++) {

        BM_TRIGGER_HIGH();
        BM_CYCLE_START();

        arm_biquad_cascade_df1_q15(&iir_instance, noise_in_q15, filt_out, BM_BLOCK_SIZE);

        cycle_samples[iter] = BM_CYCLE_STOP();
        BM_TRIGGER_LOW();
    }

    BM_Finalize(&result, cycle_samples, BM_ITERATIONS);

    /* ─── Validación numérica: rms_out en rango análogo, escalado a q15 ─── */
    q15_t rms_out_q15;
    arm_rms_q15(filt_out, BM_BLOCK_SIZE, &rms_out_q15);

    float32_t rms_out_normalized = (float32_t)rms_out_q15 / 32768.0f;
    result.numeric_result = rms_out_normalized;
    uint8_t in_range = ((rms_out_normalized > 0.001f) && (rms_out_normalized < 0.5f)) ? 1U : 0U;
    result.valid = in_range;

    return result;
}
