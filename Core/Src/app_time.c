/*提供了一个基于毫秒计数的非阻塞软件时基，即程序等待某个时间到达时，不停在原地等待
 * 而是继续处理UART、CAN、按键和LCD等其他任务
 * */
#include "app_time.h"

static volatile uint32_t app_time_ms;

void AppTime_Increment1ms(void)
{
  app_time_ms++;
}

uint32_t AppTime_GetMs(void)
{
  return app_time_ms;
}

uint8_t AppTime_IsDue(uint32_t now_ms, uint32_t *last_ms, uint32_t period_ms)
{
  if ((last_ms == 0) || (period_ms == 0U))
  {
    return 0U;
  }

  if ((uint32_t)(now_ms - *last_ms) < period_ms)
  {
    return 0U;
  }

  /* Advance by whole periods so a delayed task does not accumulate drift. */
  *last_ms += (((uint32_t)(now_ms - *last_ms) / period_ms) * period_ms);
  return 1U;
}
