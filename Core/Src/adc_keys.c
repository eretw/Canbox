#include "adc_keys.h"
#include "stm32f1xx.h"

#define ADC_KEYS_ADC_TIMEOUT       100000UL
#define ADC_KEYS_LOW_MAX           600U
#define ADC_KEYS_MIDDLE_MIN        1500U
#define ADC_KEYS_MIDDLE_MAX        2250U
#define ADC_KEYS_HIGH_MIN          2400U
#define ADC_KEYS_HIGH_MAX          3200U
#define ADC_KEYS_RELEASE_MIN       3500U
#define ADC_KEYS_AVERAGE_SAMPLES   4U

static uint16_t adc_values[ADC_KEYS_GROUP_COUNT];
static KeyRaw current_raw_key = KEY_RAW_NONE;

static uint16_t ADC_Keys_ReadChannel(uint8_t channel)
{
  uint32_t timeout = ADC_KEYS_ADC_TIMEOUT;

  ADC1->SQR1 &= ~ADC_SQR1_L;
  ADC1->SQR3 = channel;
  ADC1->SR = 0U;
  ADC1->CR2 |= ADC_CR2_ADON;

  while (((ADC1->SR & ADC_SR_EOC) == 0U) && (timeout > 0U))
  {
    timeout--;
  }

  if (timeout == 0U)
  {
    return 0xFFFFU;
  }
  return (uint16_t)ADC1->DR;
}

static uint16_t ADC_Keys_ReadAverage(uint8_t channel)
{
  uint32_t sum = 0U;
  uint8_t index;

  for (index = 0U; index < ADC_KEYS_AVERAGE_SAMPLES; index++)
  {
    uint16_t sample = ADC_Keys_ReadChannel(channel);
    if (sample == 0xFFFFU)
    {
      return sample;
    }
    sum += sample;
  }
  return (uint16_t)(sum / ADC_KEYS_AVERAGE_SAMPLES);
}

static uint8_t ADC_Keys_GetLevel(uint16_t adc)
{
  if (adc == 0xFFFFU)
  {
    return 0xFFU;
  }
  if (adc <= ADC_KEYS_LOW_MAX)
  {
    return 1U; /* 100 ohm key */
  }
  if ((adc >= ADC_KEYS_MIDDLE_MIN) && (adc <= ADC_KEYS_MIDDLE_MAX))
  {
    return 2U; /* 4.7 kohm key */
  }
  if ((adc >= ADC_KEYS_HIGH_MIN) && (adc <= ADC_KEYS_HIGH_MAX))
  {
    return 3U; /* 10 kohm key */
  }
  if (adc >= ADC_KEYS_RELEASE_MIN)
  {
    return 0U; /* no key */
  }
  return 0xFFU; /* transition, multiple press or wiring fault */
}

static KeyRaw ADC_Keys_Map(uint8_t group, uint8_t level)
{
  static const KeyRaw key_map[ADC_KEYS_GROUP_COUNT][3] =
  {
    { KEY_RAW_CONFIRM, KEY_RAW_RIGHT, KEY_RAW_LEFT },
    { KEY_RAW_CANCEL, KEY_RAW_DOWN, KEY_RAW_UP },
    { KEY_RAW_REVERSE, KEY_RAW_PREVIOUS_PAGE, KEY_RAW_NEXT_PAGE }
  };

  if ((group >= ADC_KEYS_GROUP_COUNT) || (level < 1U) || (level > 3U))
  {
    return KEY_RAW_INVALID;
  }
  return key_map[group][level - 1U];
}

uint8_t ADC_Keys_Init(void)
{
  GPIO_InitTypeDef gpio_init = {0};
  uint32_t timeout;
  // 配置ADC时钟 ADC时钟来自PCLK2 72/6 = 12MHz,没有超过STM32F1 ADC允许的14MHz上限
  RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_ADCPRE) | RCC_CFGR_ADCPRE_DIV6;
  //使能GPIOB、GPIOC、ADC1时钟
  RCC->APB2ENR |= RCC_APB2ENR_IOPBEN | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_ADC1EN;

  //这些引脚连接三组电阻按键网络，不同网络按键按下之后产生不同电压，ADC根据电压范围识别具体按钮
  gpio_init.Pin = GPIO_PIN_4 | GPIO_PIN_5;
  gpio_init.Mode = GPIO_MODE_ANALOG;//配置为模拟输入
  HAL_GPIO_Init(GPIOC, &gpio_init);
  gpio_init.Pin = GPIO_PIN_0;
  HAL_GPIO_Init(GPIOB, &gpio_init);

  /* 55.5个ADC周期的采样时间，采用较长采样时间是因为按键经过电阻网络，信号源阻抗相对较大，较长
   * 采样时间能够让ADC内部采样电容充分充电，提高读数稳定性*/
  ADC1->SMPR1 &= ~((7UL << 12U) | (7UL << 15U));
  ADC1->SMPR1 |= (5UL << 12U) | (5UL << 15U); /* CH14, CH15 */
  ADC1->SMPR2 &= ~(7UL << 24U);
  ADC1->SMPR2 |= (5UL << 24U);                 /* CH8 */
  ADC1->CR1 = 0U;
  // 启动并校准ADC
  ADC1->CR2 = ADC_CR2_ADON;

  for (timeout = 0U; timeout < 800U; timeout++)
  {
    __NOP();
  }

  ADC1->CR2 |= ADC_CR2_RSTCAL;
  timeout = ADC_KEYS_ADC_TIMEOUT;
  while (((ADC1->CR2 & ADC_CR2_RSTCAL) != 0U) && (timeout > 0U))
  {
    timeout--;
  }
  if (timeout == 0U)
  {
    return 0U;
  }

  ADC1->CR2 |= ADC_CR2_CAL;
  timeout = ADC_KEYS_ADC_TIMEOUT;
  while (((ADC1->CR2 & ADC_CR2_CAL) != 0U) && (timeout > 0U))
  {
    timeout--;
  }
  if (timeout == 0U)
  {
    return 0U;
  }
  // 立即执行一次按键采样
  ADC_Keys_SampleTask();
  return 1U;
}
// 程序会依次读取CH14, CH15和CH8，每个通道连续读取4次并计算平均值
void ADC_Keys_SampleTask(void)
{
  // Group0 CH14 包含按键：CONFIRM、RIGHT、LEFT
  // Group1 CH15 包含按键：CANCEL、DOWN、UP
  // Group2 CH8 包含按键：REVERSE、PREVIOUS、NEXT
  static const uint8_t adc_channels[ADC_KEYS_GROUP_COUNT] = {14U, 15U, 8U};
  KeyRaw detected = KEY_RAW_NONE;
  uint8_t active_count = 0U;
  uint8_t group;

  for (group = 0U; group < ADC_KEYS_GROUP_COUNT; group++)
  {
    uint8_t level;
    adc_values[group] = ADC_Keys_ReadAverage(adc_channels[group]);
    // 根据ADC范围判断电阻档位
    level = ADC_Keys_GetLevel(adc_values[group]);

    if (level == 0xFFU)
    {
      current_raw_key = KEY_RAW_INVALID;
      return;
    }
    if (level != 0U)
    {
      active_count++;
      detected = ADC_Keys_Map(group, level);
    }
  }

  current_raw_key = (active_count == 0U) ? KEY_RAW_NONE :
                    (active_count == 1U) ? detected : KEY_RAW_INVALID;
}

KeyRaw ADC_Keys_GetRaw(void)
{
  return current_raw_key;
}

uint16_t ADC_Keys_GetAdc(uint8_t group)
{
  if (group >= ADC_KEYS_GROUP_COUNT)
  {
    return 0xFFFFU;
  }
  return adc_values[group];
}

const char *ADC_Keys_GetName(KeyRaw key)
{
  static const char *const names[] =
  {
    "NONE", "CONFIRM", "LEFT", "RIGHT", "CANCEL", "UP", "DOWN",
    "REVERSE", "NEXT_PAGE", "PREVIOUS_PAGE", "INVALID"
  };

  if ((uint32_t)key >= (sizeof(names) / sizeof(names[0])))
  {
    return "INVALID";
  }
  return names[key];
}
