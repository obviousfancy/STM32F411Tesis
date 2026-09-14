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
    if (strcmp(line, "demo bm04") == 0) {
        Demo_I2S_Start(DEMO_SIGNAL_BM04_SINE);
        DemoCmd_PrintLine("-> demo bm04 (senoidal 441 Hz)\r\n");
    } else if (strcmp(line, "demo bm08") == 0) {
        Demo_I2S_Start(DEMO_SIGNAL_BM08_FIR);
        DemoCmd_PrintLine("-> demo bm08 (ruido filtrado FIR)\r\n");
    } else if (strcmp(line, "benchmark") == 0) {
        Demo_I2S_Stop();
        DemoCmd_PrintLine("-> benchmark (I2S detenido; BM01-BM10 solo corren al arrancar)\r\n");
    } else if (line[0] != '\0') {
        DemoCmd_PrintLine("comandos: demo bm04 | demo bm08 | benchmark\r\n");
    }
}

void DemoCmd_Init(void)
{
    Demo_I2S_Init();
    cmd_len = 0U;
    memset(cmd_buf, 0, sizeof(cmd_buf));
    DemoCmd_PrintLine("\r\nDemo listo. Comandos: demo bm04 | demo bm08 | benchmark\r\n");
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
