/**
 * @file    benchmark_config.h
 * @brief   Infraestructura de medición de ciclos de CPU para benchmarks de audio DSP
 *
 * NOTA SOBRE DWT:
 *   El DWT (Data Watchpoint and Trace) es parte del núcleo ARM Cortex-M4,
 *   no de los periféricos STM32. Se accede a través de core_cm4.h,
 *   incluido automáticamente por HAL. No existe equivalente HAL con
 *   resolución de ciclos individuales. HAL_GetTick() tiene resolución
 *   de 1 ms, insuficiente para benchmarks de microsegundos.
 *
 * NOTA SOBRE printf/UART:
 *   BM_Print() usa printf(), que aún no está retargeteado a UART2
 *   (USART2 pendiente, pausado por decisión de diseño). Compila sin
 *   error, pero no imprimirá nada útil hasta que se configure el
 *   retarget de __io_putchar() en main.c.
 */

#ifndef BENCHMARK_CONFIG_H
#define BENCHMARK_CONFIG_H

#include "stm32f4xx_hal.h"
#include "arm_math.h"
#include <stdio.h>
#include <stdint.h>

#ifndef PI
#define PI 3.14159265358979f
#endif

/* ─── Parámetros del banco de pruebas ─── */
#define BM_BLOCK_SIZE       256U          /* muestras por bloque */
#define BM_ITERATIONS       100U          /* iteraciones por benchmark */
#define BM_SAMPLE_RATE      44100.0f      /* Hz — frecuencia de audio objetivo */
#define BM_CORE_HZ          100000000UL   /* Hz — frecuencia del núcleo */
#define BM_BUDGET_CYCLES    580352UL      /* ciclos/bloque = BM_CORE_HZ/Fs × N */

/* ─── Pin de trigger para osciloscopio ─── */
/* Configurado en CubeMX como GPIO Output en PA6 (PA5 reservado para I2S) */
#define BM_TRIGGER_PORT     GPIOA
#define BM_TRIGGER_PIN      GPIO_PIN_6

/* ─── Macros de trigger: HAL, sin acceso directo a registros ─── */
#define BM_TRIGGER_HIGH()   HAL_GPIO_WritePin(BM_TRIGGER_PORT, BM_TRIGGER_PIN, GPIO_PIN_SET)
#define BM_TRIGGER_LOW()    HAL_GPIO_WritePin(BM_TRIGGER_PORT, BM_TRIGGER_PIN, GPIO_PIN_RESET)

/* ─── Macros de medición de ciclos — DWT (ARM CoreSight) ─── */
#define BM_CYCLE_START()    uint32_t _bm_t0 = DWT->CYCCNT
#define BM_CYCLE_STOP()     (DWT->CYCCNT - _bm_t0)

/* ─── Estructura de resultado ─── */
typedef struct {
    char     name[32];         /* nombre del benchmark */
    char     variant[4];       /* "f32" o "q15" */
    uint32_t cycles_min;
    uint32_t cycles_max;
    uint32_t cycles_avg;
    float    time_us;          /* = cycles_avg / 100.0f */
    float    cpu_percent;      /* = 100.0f × cycles_avg / BM_BUDGET_CYCLES */
    uint8_t  viable;           /* 1 si cycles_avg < BM_BUDGET_CYCLES */
    uint8_t  valid;            /* 1 si la validación numérica pasa */
    float    numeric_result;   /* valor numérico de validación (RMS, pico FFT, etc.) */
    float    precision_error_pct;   /* NUEVO: error relativo % vs f32, solo variantes q15 */
} BM_Result_t;

/* ─── Inicialización del DWT ─── */
static inline void BM_DWT_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0U;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;
}

/* ─── Finalización estadística ─── */
static inline void BM_Finalize(BM_Result_t *r,
                                uint32_t *samples,
                                uint32_t  n)
{
    uint64_t sum = 0;
    r->cycles_min = UINT32_MAX;
    r->cycles_max = 0;
    for (uint32_t i = 0; i < n; i++) {
        if (samples[i] < r->cycles_min) r->cycles_min = samples[i];
        if (samples[i] > r->cycles_max) r->cycles_max = samples[i];
        sum += samples[i];
    }
    r->cycles_avg  = (uint32_t)(sum / n);
    r->time_us     = (float)r->cycles_avg / (BM_CORE_HZ / 1000000UL);
    r->cpu_percent = 100.0f * (float)r->cycles_avg / (float)BM_BUDGET_CYCLES;
    r->viable      = (r->cycles_avg < BM_BUDGET_CYCLES) ? 1U : 0U;
}

/* ─── Imprimir resultado por UART (printf retargetado — pendiente) ─── */
static inline void BM_Print(const BM_Result_t *r) {
    printf("%-20s %s | avg:%7lu cyc | %7.2f us | %5.2f%% CPU | %s | %s\r\n",
           r->name,
           r->variant,
           r->cycles_avg,
           r->time_us,
           r->cpu_percent,
           r->viable ? "VIABLE    " : "NO VIABLE ",
           r->valid  ? "VALID"      : "INVALID");
}

/* ─── Generador de ruido blanco LFSR 32-bit (señal de entrada para filtros) ─── */
static inline float BM_LFSR_Next(uint32_t *state) {
    *state = (*state >> 1U) ^ (-(*state & 1U) & 0xD0000001U);
    return (float)(int32_t)(*state) * (1.0f / 2147483648.0f);
}

/* ─── Generar bloque de ruido blanco ─── */
static inline void BM_GenWhiteNoise(float32_t *buf, uint32_t N) {
    static uint32_t lfsr_state = 0xACE1U;
    for (uint32_t i = 0; i < N; i++) {
        buf[i] = BM_LFSR_Next(&lfsr_state);
    }
}

/* ─── Convertir buffer f32 → q15 (para benchmarks q15) ─── */
static inline void BM_F32toQ15(const float32_t *src, q15_t *dst, uint32_t N) {
    arm_float_to_q15(src, dst, N);
}

#endif /* BENCHMARK_CONFIG_H */
