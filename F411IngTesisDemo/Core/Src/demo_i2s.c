/**
 * @file    demo_i2s.c
 * @brief   Modulo de demo cualitativa por I2S -> DAC PCM5102A.
 *
 * Formato I2S usado (ver MX_I2S1_Init en main.c, generado por CubeMX,
 * NO modificado por este modulo):
 *   hi2s1.Init.DataFormat = I2S_DATAFORMAT_16B_EXTENDED
 * En este modo el propio periferico SPI/I2S rellena con ceros los 16
 * bits restantes del slot de 32 bits del frame Philips. El buffer que
 * se pasa a HAL_I2S_Transmit_DMA() es simplemente uint16_t* con la
 * muestra de 16 bits sin ningun empaquetado adicional -- no hay que
 * alinear ni desplazar nada en software.
 *
 * Las senales de demo se generan aqui, NO se extraen de
 * bm_synthesis.c/bm_filters.c:
 *  - Esas funciones _Run() guardan sus buffers en arrays locales
 *    (stack), no los exponen.
 *  - bm_filters.c declara sus coeficientes FIR/IIR como 'static
 *    const', invisibles fuera de ese archivo.
 * Por eso este modulo REGENERA cada senal con el mismo algoritmo (y,
 * para BM08/BM09, los mismos coeficientes, copiados una vez) en vez de
 * "consumir" el buffer original. bm_synthesis.c/h y bm_filters.c/h
 * quedan intactos.
 *
 * BM01-BM04 (sintesis) se reproducen a 441 Hz, no 440 Hz: 44100/441 =
 * 100 exacto, lo que permite un buffer circular de 100 muestras con
 * loop perfecto (sin click de discontinuidad de fase). Con 440 Hz real
 * se necesitarian 2205 muestras para un loop exacto. La diferencia de
 * 0.23% es inaudible e irrelevante -- no afecta nada de la tesis.
 *
 * BM08/BM09 (ruido blanco crudo / FIR / IIR) SI tienen un click de
 * wraparound inevitable, porque el ruido no es periodico. Los tres
 * comparten el MISMO bloque de ruido de entrada (DEMO_NOISE_SAMPLES),
 * para que "demo noise" / "demo bm08" / "demo bm09" sean una
 * comparacion A/B/C real sobre la misma señal. Se usa un buffer de
 * 1024 muestras (~23 ms de loop @ 44.1 kHz) -- se redujo de 2048 a
 * 1024 al agregar la tercera variante (IIR) para no pasar de ~65 KB de
 * RAM en un chip de 128 KB solo en buffers de demo. Si el click
 * resulta molesto, la mitigacion (crossfade en los bordes del buffer)
 * queda fuera de este alcance.
 */

#include "demo_i2s.h"
#include "benchmark_config.h"   /* BM_GenWhiteNoise(), BM_SAMPLE_RATE, PI -- static inline, reuso seguro */
#include "arm_math.h"
#include <math.h>
#include <string.h>

extern I2S_HandleTypeDef hi2s1;

/* ─── Senales 1-4: sintesis continua, equivalentes a BM01-BM04 (ver nota de 441 Hz arriba) ─── */
#define DEMO_TONE_SAMPLES   100U
#define DEMO_TONE_FREQ_HZ   441.0f

/* ─── Senales 5-7: ruido blanco crudo / FIR / IIR, equivalentes a BM08/BM09 ─── */
#define DEMO_NOISE_SAMPLES  1024U
#define DEMO_FIR_NUM_TAPS   32U
#define DEMO_IIR_NUM_STAGES 4U

/* Coeficientes IDENTICOS a bm_filters.c (BM08a, FIR 32 taps, fc=4kHz
 * @44.1kHz). bm_filters.c los declara 'static', por lo que no son
 * visibles desde aqui -- se duplican a proposito para no tocar
 * bm_filters.c/.h. Si el benchmark cambia sus coeficientes, esta copia
 * debe actualizarse manualmente. */
static const float32_t demo_fir_coeffs_f32[DEMO_FIR_NUM_TAPS] = {
    0.0009135370f,  0.0017961076f,  0.0027231435f,  0.0030745141f,
    0.0016470869f, -0.0026444830f, -0.0097066725f, -0.0174180225f,
   -0.0215961846f, -0.0170796380f,  0.0003054037f,  0.0316256416f,
    0.0735178202f,  0.1183969678f,  0.1563434240f,  0.1781013542f,
    0.1781013542f,  0.1563434240f,  0.1183969678f,  0.0735178202f,
    0.0316256416f,  0.0003054037f, -0.0170796380f, -0.0215961846f,
   -0.0174180225f, -0.0097066725f, -0.0026444830f,  0.0016470869f,
    0.0030745141f,  0.0027231435f,  0.0017961076f,  0.0009135370f
};

/* Coeficientes IDENTICOS a bm_filters.c (BM09a, IIR Butterworth orden 8,
 * 4 biquads DF2T, fc=4kHz @44.1kHz), mismo motivo de duplicacion que
 * los FIR arriba. Formato CMSIS-DSP DF2T: {b0,b1,b2,-a1,-a2} por etapa. */
static const float32_t demo_iir_coeffs_f32[5U * DEMO_IIR_NUM_STAGES] = {
    0.0000122541f, 0.0000245083f, 0.0000122541f,  1.1011799750f, -0.3078875724f,
    1.0000000000f, 2.0000000000f, 1.0000000000f,  1.1624208646f, -0.3806242733f,
    1.0000000000f, 2.0000000000f, 1.0000000000f,  1.2955532780f, -0.5387475891f,
    1.0000000000f, 2.0000000000f, 1.0000000000f,  1.5235369496f, -0.8095271325f
};

/* ─── Ajuste esperable en banco con el osciloscopio -- cambiar aqui,
 * no requiere tocar la logica. NOTA: no hay flag de "swap L/R" porque
 * L y R llevan el mismo dato (mono duplicado); un intercambio de
 * canales no tendria ningun efecto observable con esta señal. Si mas
 * adelante se quiere una señal real por canal, ahi si aplicaria. ─── */
#define DEMO_INVERT_POLARITY   0   /* 1 = invierte el signo de la muestra */

/* Buffers mono float32, intermedios antes de convertir a PCM16 */
static float32_t demo_sawtooth_f32[DEMO_TONE_SAMPLES];
static float32_t demo_square_f32[DEMO_TONE_SAMPLES];
static float32_t demo_triangle_f32[DEMO_TONE_SAMPLES];
static float32_t demo_sine_f32[DEMO_TONE_SAMPLES];
static float32_t demo_fir_f32[DEMO_NOISE_SAMPLES];
static float32_t demo_iir_f32[DEMO_NOISE_SAMPLES];

/* Buffers finales entregados al DMA: PCM16 intercalado L,R,L,R,... */
static uint16_t demo_sawtooth_i16_stereo[DEMO_TONE_SAMPLES * 2U];
static uint16_t demo_square_i16_stereo[DEMO_TONE_SAMPLES * 2U];
static uint16_t demo_triangle_i16_stereo[DEMO_TONE_SAMPLES * 2U];
static uint16_t demo_sine_i16_stereo[DEMO_TONE_SAMPLES * 2U];
static uint16_t demo_fir_i16_stereo[DEMO_NOISE_SAMPLES * 2U];
static uint16_t demo_iir_i16_stereo[DEMO_NOISE_SAMPLES * 2U];
/* Mismo ruido de entrada de demo_fir_i16_stereo/demo_iir_i16_stereo,
 * SIN pasar por ningun filtro -- referencia para comparar A/B/C (oido
 * u osciloscopio) contra las versiones filtradas y verificar que los
 * filtros realmente estan haciendo algo. */
static uint16_t demo_noise_raw_i16_stereo[DEMO_NOISE_SAMPLES * 2U];

static Demo_Signal_t demo_active_signal = DEMO_SIGNAL_NONE;

/**
 * @brief Convierte un buffer mono float32 en [-1,1] a PCM16 estereo
 *        (mismo dato en L y R). Clamping defensivo en el limite hacia
 *        el hardware -- las fuentes internas ya estan en [-1,1], pero
 *        esta es la frontera real hacia el DAC.
 */
static void Demo_ConvertMonoF32ToStereoI16(const float32_t *src, uint16_t *dst, uint32_t n)
{
    for (uint32_t i = 0; i < n; i++) {
        float32_t s = src[i];
        if (s >  1.0f) { s =  1.0f; }
        if (s < -1.0f) { s = -1.0f; }
#if DEMO_INVERT_POLARITY
        s = -s;
#endif

        int16_t  pcm   = (int16_t)(s * 32767.0f);
        uint16_t pcm_u = (uint16_t)pcm;

        dst[2U * i]      = pcm_u; /* L */
        dst[2U * i + 1U] = pcm_u; /* R */
    }
}

static void Demo_GenerateTones(void)
{
    const float32_t dt = DEMO_TONE_FREQ_HZ / BM_SAMPLE_RATE; /* = 0.01 exacto */
    float32_t phase = 0.0f;

    for (uint32_t i = 0; i < DEMO_TONE_SAMPLES; i++) {
        demo_sawtooth_f32[i] = 2.0f * phase - 1.0f;
        demo_square_f32[i]   = (phase < 0.5f) ? 1.0f : -1.0f;
        demo_triangle_f32[i] = 1.0f - 4.0f * fabsf(phase - 0.5f);

        phase += dt;
        if (phase >= 1.0f) {
            phase -= 1.0f;
        }
    }

    for (uint32_t i = 0; i < DEMO_TONE_SAMPLES; i++) {
        float32_t theta = 2.0f * PI * DEMO_TONE_FREQ_HZ * (float32_t)i / BM_SAMPLE_RATE;
        demo_sine_f32[i] = arm_sin_f32(theta);
    }

    Demo_ConvertMonoF32ToStereoI16(demo_sawtooth_f32, demo_sawtooth_i16_stereo, DEMO_TONE_SAMPLES);
    Demo_ConvertMonoF32ToStereoI16(demo_square_f32,   demo_square_i16_stereo,   DEMO_TONE_SAMPLES);
    Demo_ConvertMonoF32ToStereoI16(demo_triangle_f32, demo_triangle_i16_stereo, DEMO_TONE_SAMPLES);
    Demo_ConvertMonoF32ToStereoI16(demo_sine_f32,     demo_sine_i16_stereo,     DEMO_TONE_SAMPLES);
}

static void Demo_GenerateFilteredNoise(void)
{
    /* 'static' para no comprometer el stack con arrays de varios KB --
     * este RAM permanece reservado en .bss, pero en un STM32F411 de
     * 128 KB de RAM el costo es aceptable y elimina cualquier riesgo
     * de overflow de stack en esta inicializacion. */
    static float32_t noise_in[DEMO_NOISE_SAMPLES];
    static float32_t fir_state[DEMO_NOISE_SAMPLES + DEMO_FIR_NUM_TAPS - 1U];
    float32_t        iir_state[2U * DEMO_IIR_NUM_STAGES]; /* DF2T: 2 estados/etapa, cabe en stack */

    arm_fir_instance_f32 fir_instance;
    arm_biquad_cascade_df2T_instance_f32 iir_instance;

    /* Un solo bloque de ruido, compartido por las 3 variantes -- para
     * que "demo noise" / "demo bm08" / "demo bm09" comparen la MISMA
     * señal de entrada, no ruidos independientes. */
    BM_GenWhiteNoise(noise_in, DEMO_NOISE_SAMPLES);
    Demo_ConvertMonoF32ToStereoI16(noise_in, demo_noise_raw_i16_stereo, DEMO_NOISE_SAMPLES);

    arm_fir_init_f32(&fir_instance, DEMO_FIR_NUM_TAPS,
                      (float32_t *)demo_fir_coeffs_f32, fir_state, DEMO_NOISE_SAMPLES);
    arm_fir_f32(&fir_instance, noise_in, demo_fir_f32, DEMO_NOISE_SAMPLES);
    Demo_ConvertMonoF32ToStereoI16(demo_fir_f32, demo_fir_i16_stereo, DEMO_NOISE_SAMPLES);

    arm_biquad_cascade_df2T_init_f32(&iir_instance, DEMO_IIR_NUM_STAGES,
                                      (float32_t *)demo_iir_coeffs_f32, iir_state);
    arm_biquad_cascade_df2T_f32(&iir_instance, noise_in, demo_iir_f32, DEMO_NOISE_SAMPLES);
    Demo_ConvertMonoF32ToStereoI16(demo_iir_f32, demo_iir_i16_stereo, DEMO_NOISE_SAMPLES);
}

void Demo_I2S_Init(void)
{
    Demo_GenerateTones();
    Demo_GenerateFilteredNoise();
    demo_active_signal = DEMO_SIGNAL_NONE;
}

void Demo_I2S_Start(Demo_Signal_t signal)
{
    if (demo_active_signal != DEMO_SIGNAL_NONE) {
        HAL_I2S_DMAStop(&hi2s1);
    }

    switch (signal) {
        case DEMO_SIGNAL_BM01_SAWTOOTH:
            HAL_I2S_Transmit_DMA(&hi2s1, demo_sawtooth_i16_stereo, DEMO_TONE_SAMPLES * 2U);
            break;

        case DEMO_SIGNAL_BM02_SQUARE:
            HAL_I2S_Transmit_DMA(&hi2s1, demo_square_i16_stereo, DEMO_TONE_SAMPLES * 2U);
            break;

        case DEMO_SIGNAL_BM03_TRIANGLE:
            HAL_I2S_Transmit_DMA(&hi2s1, demo_triangle_i16_stereo, DEMO_TONE_SAMPLES * 2U);
            break;

        case DEMO_SIGNAL_BM04_SINE:
            HAL_I2S_Transmit_DMA(&hi2s1, demo_sine_i16_stereo, DEMO_TONE_SAMPLES * 2U);
            break;

        case DEMO_SIGNAL_BM08_FIR:
            HAL_I2S_Transmit_DMA(&hi2s1, demo_fir_i16_stereo, DEMO_NOISE_SAMPLES * 2U);
            break;

        case DEMO_SIGNAL_BM08_RAW_NOISE:
            HAL_I2S_Transmit_DMA(&hi2s1, demo_noise_raw_i16_stereo, DEMO_NOISE_SAMPLES * 2U);
            break;

        case DEMO_SIGNAL_BM09_IIR:
            HAL_I2S_Transmit_DMA(&hi2s1, demo_iir_i16_stereo, DEMO_NOISE_SAMPLES * 2U);
            break;

        default:
            demo_active_signal = DEMO_SIGNAL_NONE;
            return;
    }

    demo_active_signal = signal;
}

void Demo_I2S_Stop(void)
{
    if (demo_active_signal != DEMO_SIGNAL_NONE) {
        HAL_I2S_DMAStop(&hi2s1);
        demo_active_signal = DEMO_SIGNAL_NONE;
    }
}

Demo_Signal_t Demo_I2S_GetActiveSignal(void)
{
    return demo_active_signal;
}
