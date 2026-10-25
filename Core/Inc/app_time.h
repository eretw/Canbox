#ifndef APP_TIME_H
#define APP_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void AppTime_Increment1ms(void);
uint32_t AppTime_GetMs(void);
uint8_t AppTime_IsDue(uint32_t now_ms, uint32_t *last_ms, uint32_t period_ms);

#ifdef __cplusplus
}
#endif

#endif /* APP_TIME_H */
