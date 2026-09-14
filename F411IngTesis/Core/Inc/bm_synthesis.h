/**
 * @file    bm_synthesis.h
 * @brief   Categoría 1 — Síntesis de señal (BM01–BM04), solo f32
 *
 * Todas las señales se generan internamente en el MCU mediante
 * acumulador de fase. No hay ADC ni entrada externa — ver
 * benchmark_config.h y README para justificación metodológica.
 */

#ifndef BM_SYNTHESIS_H
#define BM_SYNTHESIS_H

#include "benchmark_config.h"

/* ─── BM01: Diente de sierra (sawtooth), f32 ─── */
BM_Result_t BM01_Sawtooth_Run(void);

/* ─── BM02: Onda cuadrada (square), f32 ─── */
BM_Result_t BM02_Square_Run(void);

/* ─── BM03: Onda triangular (triangle), f32 ─── */
BM_Result_t BM03_Triangle_Run(void);

/* ─── BM04: Senoidal con sinf(), f32 — CONTRASTE CLAVE ─── */
BM_Result_t BM04_Sine_Run(void);

#endif /* BM_SYNTHESIS_H */
