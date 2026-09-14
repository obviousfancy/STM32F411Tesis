/**
 * @file    demo_i2s.h
 * @brief   Modulo de demo cualitativa por I2S -> DAC PCM5102A.
 *
 * NO forma parte de la fase de benchmarks (no usa DWT, no toca
 * benchmark_config.h). Reproduce en loop continuo, via DMA circular
 * sobre hi2s1/SPI1, senales equivalentes a BM01-BM04 (sintesis) y a
 * BM08/BM09 (ruido blanco crudo vs filtrado FIR/IIR), regeneradas
 * localmente en este modulo porque bm_synthesis.c/bm_filters.c no
 * exponen sus buffers ni sus coeficientes fuera de esos archivos.
 */

#ifndef DEMO_I2S_H
#define DEMO_I2S_H

#include "stm32f4xx_hal.h"

typedef enum {
    DEMO_SIGNAL_NONE = 0,
    DEMO_SIGNAL_BM01_SAWTOOTH,
    DEMO_SIGNAL_BM02_SQUARE,
    DEMO_SIGNAL_BM03_TRIANGLE,
    DEMO_SIGNAL_BM04_SINE,
    DEMO_SIGNAL_BM08_FIR,
    DEMO_SIGNAL_BM08_RAW_NOISE,  /* mismo ruido de BM08/BM09, SIN filtrar -- referencia A/B */
    DEMO_SIGNAL_BM09_IIR,
} Demo_Signal_t;

/* Pre-genera todos los buffers de audio. No toca el I2S.
 * Llamar una sola vez, despues de MX_I2S1_Init(). */
void Demo_I2S_Init(void);

/* Detiene cualquier transmision activa y arranca HAL_I2S_Transmit_DMA()
 * en modo circular sobre el buffer pre-generado correspondiente. */
void Demo_I2S_Start(Demo_Signal_t signal);

/* Detiene la transmision I2S (silencio). Seguro llamarla si ya estaba
 * detenida. */
void Demo_I2S_Stop(void);

Demo_Signal_t Demo_I2S_GetActiveSignal(void);

#endif /* DEMO_I2S_H */
