/**
 * @file    demo_cmd.c
 * @brief   Parser de comandos por UART (USART2) para seleccionar modo demo.
 */

#include "demo_cmd.h"
#include "demo_i2s.h"
#include "stm32f4xx_hal.h"
#include <string.h>

extern UART_HandleTypeDef huart2;

#define CMD_BUF_LEN  32U

static char     cmd_buf[CMD_BUF_LEN];
static uint32_t cmd_len = 0U;

static void DemoCmd_PrintLine(const char *s)
{
    HAL_UART_Transmit(&huart2, (uint8_t *)s, (uint16_t)strlen(s), 100U);
}

static void DemoCmd_Execute(const char *line)
{
    if (strcmp(line, "demo bm01") == 0) {
        Demo_I2S_Start(DEMO_SIGNAL_BM01_SAWTOOTH);
        DemoCmd_PrintLine("-> demo bm01 (diente de sierra 441 Hz)\r\n");
    } else if (strcmp(line, "demo bm02") == 0) {
        Demo_I2S_Start(DEMO_SIGNAL_BM02_SQUARE);
        DemoCmd_PrintLine("-> demo bm02 (onda cuadrada 441 Hz)\r\n");
    } else if (strcmp(line, "demo bm03") == 0) {
        Demo_I2S_Start(DEMO_SIGNAL_BM03_TRIANGLE);
        DemoCmd_PrintLine("-> demo bm03 (onda triangular 441 Hz)\r\n");
    } else if (strcmp(line, "demo bm04") == 0) {
        Demo_I2S_Start(DEMO_SIGNAL_BM04_SINE);
        DemoCmd_PrintLine("-> demo bm04 (senoidal 441 Hz)\r\n");
    } else if (strcmp(line, "demo bm08") == 0) {
        Demo_I2S_Start(DEMO_SIGNAL_BM08_FIR);
        DemoCmd_PrintLine("-> demo bm08 (ruido filtrado FIR, fc=4kHz)\r\n");
    } else if (strcmp(line, "demo bm09") == 0) {
        Demo_I2S_Start(DEMO_SIGNAL_BM09_IIR);
        DemoCmd_PrintLine("-> demo bm09 (ruido filtrado IIR, fc=4kHz)\r\n");
    } else if (strcmp(line, "demo noise") == 0) {
        Demo_I2S_Start(DEMO_SIGNAL_BM08_RAW_NOISE);
        DemoCmd_PrintLine("-> demo noise (mismo ruido de bm08/bm09, SIN filtrar -- referencia A/B/C)\r\n");
    } else if (strcmp(line, "benchmark") == 0) {
        Demo_I2S_Stop();
        DemoCmd_PrintLine("-> benchmark (I2S detenido; BM01-BM10 solo corren al arrancar)\r\n");
    } else if (line[0] != '\0') {
        DemoCmd_PrintLine("comandos: demo bm01 | demo bm02 | demo bm03 | demo bm04 | demo bm08 | demo bm09 | demo noise | benchmark\r\n");
    }
}

void DemoCmd_Init(void)
{
    Demo_I2S_Init();
    cmd_len = 0U;
    memset(cmd_buf, 0, sizeof(cmd_buf));
    DemoCmd_PrintLine("\r\nDemo listo. Comandos: demo bm01 | demo bm02 | demo bm03 | demo bm04 | demo bm08 | demo bm09 | demo noise | benchmark\r\n");
}

void DemoCmd_Poll(void)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE) == RESET) {
        return;
    }

    uint8_t c = (uint8_t)(huart2.Instance->DR & 0xFFU);

    if ((c == '\r') || (c == '\n')) {
        cmd_buf[cmd_len] = '\0';
        if (cmd_len > 0U) {
            DemoCmd_Execute(cmd_buf);
        }
        cmd_len = 0U;
        return;
    }

    if (cmd_len < (CMD_BUF_LEN - 1U)) {
        cmd_buf[cmd_len++] = (char)c;
    } else {
        /* linea demasiado larga: descartar y reiniciar */
        cmd_len = 0U;
    }
}
