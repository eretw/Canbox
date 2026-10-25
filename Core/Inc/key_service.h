#ifndef KEY_SERVICE_H
#define KEY_SERVICE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "adc_keys.h"
#include <stdint.h>

typedef enum
{
  KEY_EVENT_NONE = 0,
  KEY_EVENT_SHORT,
  KEY_EVENT_LONG
} KeyEventType;

typedef struct
{
  KeyRaw key;
  KeyEventType type;
  uint32_t duration_ms;
} KeyEvent;

void KeyService_Init(uint32_t now_ms);
void KeyService_Task(uint32_t now_ms);
uint8_t KeyService_GetEvent(KeyEvent *event);
KeyRaw KeyService_GetStableKey(void);
uint32_t KeyService_GetPressedDuration(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* KEY_SERVICE_H */
