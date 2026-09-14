/**
 * @file    bm_filters.h
 * @brief   Categoría 3 — Filtrado digital (BM08–BM09), f32 vs q15
 */

#ifndef BM_FILTERS_H
#define BM_FILTERS_H

#include "benchmark_config.h"

/* ─── BM08a/b: FIR 32 taps, f32 y q15 ─── */
BM_Result_t BM08a_FIR_F32_Run(void);
BM_Result_t BM08b_FIR_Q15_Run(void);

/* ─── BM09a/b: IIR Biquad 4 etapas, f32 y q15 ─── */
BM_Result_t BM09a_IIR_F32_Run(void);
BM_Result_t BM09b_IIR_Q15_Run(void);
#endif /* BM_FILTERS_H */
