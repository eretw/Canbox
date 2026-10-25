#include "key_service.h"

#define KEY_DEBOUNCE_TIME_MS  40U
#define KEY_LONG_TIME_MS      2000U
#define KEY_EVENT_QUEUE_SIZE  4U

static KeyRaw candidate_key;
static KeyRaw stable_key;
static uint32_t candidate_since_ms;
static uint32_t pressed_since_ms;
static KeyEvent event_queue[KEY_EVENT_QUEUE_SIZE];
static uint8_t event_head;
static uint8_t event_tail;

static void KeyService_PushEvent(KeyRaw key, uint32_t duration_ms)
{
  uint8_t next_head = (uint8_t)((event_head + 1U) % KEY_EVENT_QUEUE_SIZE);

  if (next_head == event_tail)
  {
    /* Preserve the newest user action if the application is temporarily busy. */
    event_tail = (uint8_t)((event_tail + 1U) % KEY_EVENT_QUEUE_SIZE);
  }

  event_queue[event_head].key = key;
  event_queue[event_head].duration_ms = duration_ms;
  event_queue[event_head].type = (duration_ms >= KEY_LONG_TIME_MS) ?
                                 KEY_EVENT_LONG : KEY_EVENT_SHORT;
  event_head = next_head;
}

void KeyService_Init(uint32_t now_ms)
{
  candidate_key = KEY_RAW_NONE;
  stable_key = KEY_RAW_NONE;
  candidate_since_ms = now_ms;
  pressed_since_ms = now_ms;
  event_head = 0U;
  event_tail = 0U;
}

void KeyService_Task(uint32_t now_ms)
{
  KeyRaw raw_key = ADC_Keys_GetRaw();

  if (raw_key == KEY_RAW_INVALID)
  {
    candidate_key = stable_key;
    candidate_since_ms = now_ms;
    return;
  }

  if (raw_key != candidate_key)
  {
    candidate_key = raw_key;
    candidate_since_ms = now_ms;
    return;
  }

  if ((candidate_key == stable_key) ||
      ((uint32_t)(now_ms - candidate_since_ms) < KEY_DEBOUNCE_TIME_MS))
  {
    return;
  }
  // 要求按键状态稳定40ms
  if ((stable_key != KEY_RAW_NONE) && (candidate_key == KEY_RAW_NONE))
  {
    KeyService_PushEvent(stable_key, (uint32_t)(now_ms - pressed_since_ms));
  }

  stable_key = candidate_key;
  if (stable_key != KEY_RAW_NONE)
  {
    pressed_since_ms = now_ms;
  }
}

uint8_t KeyService_GetEvent(KeyEvent *event)
{
  if ((event == 0) || (event_head == event_tail))
  {
    return 0U;
  }

  *event = event_queue[event_tail];
  event_tail = (uint8_t)((event_tail + 1U) % KEY_EVENT_QUEUE_SIZE);
  return 1U;
}

KeyRaw KeyService_GetStableKey(void)
{
  return stable_key;
}

uint32_t KeyService_GetPressedDuration(uint32_t now_ms)
{
  if (stable_key == KEY_RAW_NONE)
  {
    return 0U;
  }
  return (uint32_t)(now_ms - pressed_since_ms);
}
