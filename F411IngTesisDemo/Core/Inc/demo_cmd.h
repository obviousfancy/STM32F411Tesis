/**
 * @file    demo_cmd.h
 * @brief   Parser de comandos por UART (USART2) para seleccionar modo:
 *          "demo bm04" | "demo bm08" | "benchmark".
 *
 * No usa interrupciones de USART2 (CubeMX no las genero para este
 * proyecto) -- DemoCmd_Poll() hace polling no bloqueante del flag RXNE.
 * Debe llamarse repetidamente desde el loop principal en modo idle
 * (nunca durante la corrida de benchmarks con DWT).
 */

#ifndef DEMO_CMD_H
#define DEMO_CMD_H

/* Pre-genera los buffers de demo (llama a Demo_I2S_Init()) e imprime
 * el mensaje de ayuda por UART. Llamar una sola vez, despues de que
 * termine la corrida de benchmarks BM01-BM10. */
void DemoCmd_Init(void);

/* Revisa si llego un caracter por USART2 y, al completarse una linea
 * (CR o LF), ejecuta el comando. No bloquea si no hay datos. Llamar en
 * cada iteracion del loop principal. */
void DemoCmd_Poll(void);

#endif /* DEMO_CMD_H */
