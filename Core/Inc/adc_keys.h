#ifndef ADC_KEYS_H
#define ADC_KEYS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define ADC_KEYS_GROUP_COUNT 3U

typedef enum
{
  KEY_RAW_NONE = 0,
  KEY_RAW_CONFIRM,
  KEY_RAW_LEFT,
  KEY_RAW_RIGHT,
  KEY_RAW_CANCEL,
  KEY_RAW_UP,
  KEY_RAW_DOWN,
  KEY_RAW_REVERSE,
  KEY_RAW_NEXT_PAGE,
  KEY_RAW_PREVIOUS_PAGE,
  KEY_RAW_INVALID
} KeyRaw;

uint8_t ADC_Keys_Init(void);
void ADC_Keys_SampleTask(void);
KeyRaw ADC_Keys_GetRaw(void);
uint16_t ADC_Keys_GetAdc(uint8_t group);
const char *ADC_Keys_GetName(KeyRaw key);

#ifdef __cplusplus
}
#endif

#endif /* ADC_KEYS_H */
