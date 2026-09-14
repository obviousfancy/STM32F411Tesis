/**
 * @file    bm_fft.h
 * @brief   Categoría 2 — Análisis espectral FFT (BM05–BM07), f32 vs q15
 */

#ifndef BM_FFT_H
#define BM_FFT_H

#include "benchmark_config.h"

/* ─── BM05a: FFT 64 puntos, f32 ─── */
BM_Result_t BM05a_FFT64_F32_Run(void);

/* ─── BM05b: FFT 64 puntos, q15 ─── */
BM_Result_t BM05b_FFT64_Q15_Run(void);

/* ─── BM06a: FFT 128 puntos, f32 ─── */
BM_Result_t BM06a_FFT128_F32_Run(void);

/* ─── BM06b: FFT 128 puntos, q15 ─── */
BM_Result_t BM06b_FFT128_Q15_Run(void);

/* ─── BM07a: FFT 256 puntos, f32 ─── */
BM_Result_t BM07a_FFT256_F32_Run(void);

/* ─── BM07b: FFT 256 puntos, q15 ─── */
BM_Result_t BM07b_FFT256_Q15_Run(void);

#endif /* BM_FFT_H */
