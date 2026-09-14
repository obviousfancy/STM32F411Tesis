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
 * Las dos senales de demo se generan aqui, NO se extraen de
 * bm_synthesis.c/bm_filters.c:
 *  - Esas funciones _Run() guardan sus buffers en arrays locales
 *    (stack), no los exponen.
 *  - bm_filters.c declara sus coeficientes FIR/IIR como 'static
 *    const', invisibles fuera de ese archivo.
 * Por eso este modulo REGENERA una senal equivalente (mismo algoritmo,
 * mismos coeficientes FIR copiados una vez) en vez de "consumir" el
 * buffer original. bm_synthesis.c/h y bm_filters.c/h quedan intactos.
 *
 * BM04 (senoidal) se reproduce a 441 Hz, no 440 Hz: 44100/441 = 100
 * exacto, lo que permite un buffer circular de 100 muestras con loop
 * perfecto (sin click de discontinuidad de fase). Con 440 Hz real se
 * necesitarian 2205 muestras para un loop exacto. La diferencia de
 * 0.23% es inaudible e irrelevante -- no afecta nada de la tesis.
 *
 * BM08 (ruido blanco + FIR paso-bajo) SI tiene un click de wraparound
 * inevitable, porque el ruido no es periodico. Se usa un buffer de
 * 2048 muestras (~46 ms de loop @ 44.1 kHz) para que el click sea poco
 * frecuente. Si en el osciloscopio/oido resulta molesto, la mitigacion
 * (crossfade en los bordes del buffer) queda fuera de este alcance.
 */

#include "demo_i2s.h"
#include "benchmark_config.h"   /* BM_GenWhiteNoise(), BM_SAMPLE_RATE, PI -- static inline, reuso seguro */
#include "arm_math.h"
#include <string.h>

extern I2S_HandleTypeDef hi2s1;

/* ─── Senal 1: senoidal continua, equivalente a BM04 (ver nota de 441 Hz arriba) ─── */
#define DEMO_SINE_SAMPLES   100U
#define DEMO_SINE_FREQ_HZ   441.0f

/* ─── Senal 2: ruido blanco filtrado FIR paso-bajo, equivalente a BM08a ─── */
#define DEMO_FIR_SAMPLES    2048U
#define DEMO_FIR_NUM_TAPS   32U

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

/* ─── Ajuste esperable en banco con el osciloscopio -- cambiar aqui,
 * no requiere tocar la logica. NOTA: no hay flag de "swap L/R" porque
 * L y R llevan el mismo dato (mono duplicado); un intercambio de
 * canales no tendria ningun efecto observable con esta señal. Si mas
 * adelante se quiere una señal real por canal, ahi si aplicaria. ─── */
#define DEMO_INVERT_POLARITY   0   /* 1 = invierte el signo de la muestra */

static float32_t demo_sine_f32[DEMO_SINE_SAMPLES];
static float32_t demo_fir_f32[DEMO_FIR_SAMPLES];

/* Buffers finales entregados al DMA: PCM16 intercalado L,R,L,R,... */
static uint16_t demo_sine_i16_stereo[DEMO_SINE_SAMPLES * 2U];
static uint16_t demo_fir_i16_stereo[DEMO_FIR_SAMPLES * 2U];

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

static void Demo_GenerateSine(void)
{
    for (uint32_t i = 0; i < DEMO_SINE_SAMPLES; i++) {
        float32_t theta = 2.0f * PI * DEMO_SINE_FREQ_HZ * (float32_t)i / BM_SAMPLE_RATE;
        demo_sine_f32[i] = arm_sin_f32(theta);
    }
    Demo_ConvertMonoF32ToStereoI16(demo_sine_f32, demo_sine_i16_stereo, DEMO_SINE_SAMPLES);
}

static void Demo_GenerateFirNoise(void)
{
    /* 'static' para no comprometer el stack con ~16 KB de arrays
     * transitorios -- este RAM permanece reservado en .bss, pero en
     * un STM32F411 de 128 KB de RAM el costo es aceptable y elimina
     * cualquier riesgo de overflow de stack en esta inicializacion. */
    static float32_t noise_in[DEMO_FIR_SAMPLES];
    static float32_t fir_state[DEMO_FIR_SAMPLES + DEMO_FIR_NUM_TAPS - 1U];
    arm_fir_instance_f32 fir_instance;

    BM_GenWhiteNoise(noise_in, DEMO_FIR_SAMPLES);

    arm_fir_init_f32(&fir_instance, DEMO_FIR_NUM_TAPS,
                      (float32_t *)demo_fir_coeffs_f32, fir_state, DEMO_FIR_SAMPLES);
    arm_fir_f32(&fir_instance, noise_in, demo_fir_f32, DEMO_FIR_SAMPLES);

    Demo_ConvertMonoF32ToStereoI16(demo_fir_f32, demo_fir_i16_stereo, DEMO_FIR_SAMPLES);
}

void Demo_I2S_Init(void)
{
    Demo_GenerateSine();
    Demo_GenerateFirNoise();
    demo_active_signal = DEMO_SIGNAL_NONE;
}

void Demo_I2S_Start(Demo_Signal_t signal)
{
    if (demo_active_signal != DEMO_SIGNAL_NONE) {
        HAL_I2S_DMAStop(&hi2s1);
    }

    switch (signal) {
        case DEMO_SIGNAL_BM04_SINE:
            HAL_I2S_Transmit_DMA(&hi2s1, demo_sine_i16_stereo, DEMO_SINE_SAMPLES * 2U);
            break;

        case DEMO_SIGNAL_BM08_FIR:
            HAL_I2S_Transmit_DMA(&hi2s1, demo_fir_i16_stereo, DEMO_FIR_SAMPLES * 2U);
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
