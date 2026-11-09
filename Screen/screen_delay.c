#include "screen_delay.h"
#include "stm32f4xx_hal.h"

void Screen_DelayInit(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

void Screen_DelayUs(uint32_t microseconds)
{
    const uint32_t ticks = (SystemCoreClock / 1000000U) * microseconds;
    const uint32_t start = DWT->CYCCNT;

    while ((uint32_t)(DWT->CYCCNT - start) < ticks)
    {
    }
}
