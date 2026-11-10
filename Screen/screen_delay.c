#include "screen_delay.h"
#include "stm32f4xx_hal.h"

/* 启用内核 DWT 周期计数器，供屏幕驱动进行微秒级延时。 */
void Screen_DelayInit(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* 按当前内核时钟换算等待周期，供软件通信使用。 */
void Screen_DelayUs(uint32_t microseconds)
{
    const uint32_t ticks = (SystemCoreClock / 1000000U) * microseconds;
    const uint32_t start = DWT->CYCCNT;

    while ((uint32_t)(DWT->CYCCNT - start) < ticks)
    {
    }
}
