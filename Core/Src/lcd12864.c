#include "lcd12864.h"

#include <string.h>

#define LCD_SYNC_COMMAND 0xF8U
#define LCD_SYNC_DATA    0xFAU

static const uint8_t lcd_row_address[LCD12864_ROWS] =
{
  0x80U, 0x90U, 0x88U, 0x98U
};

static uint8_t lcd_shadow[LCD12864_ROWS][LCD12864_COLS];
static uint8_t lcd_initialized;

static void LCD12864_ShortDelay(void)
{
  volatile uint32_t count;

  /* At 72 MHz this is comfortably longer than the 200 ns clock half-period. */
  for (count = 0U; count < 24U; count++)
  {
    __NOP();
  }
}
// GPIO模拟SPI时序
static void LCD12864_SendSerialByte(uint8_t value)
{
  uint8_t bit;

  for (bit = 0U; bit < 8U; bit++)
  {
    HAL_GPIO_WritePin(LCD_SID_GPIO_Port, LCD_SID_Pin,
                      ((value & 0x80U) != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    LCD12864_ShortDelay();
    HAL_GPIO_WritePin(LCD_CLK_GPIO_Port, LCD_CLK_Pin, GPIO_PIN_SET);
    LCD12864_ShortDelay();
    HAL_GPIO_WritePin(LCD_CLK_GPIO_Port, LCD_CLK_Pin, GPIO_PIN_RESET);
    value <<= 1;
  }
}

static void LCD12864_WriteRaw(uint8_t is_data, uint8_t value)
{
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
  LCD12864_ShortDelay();

  LCD12864_SendSerialByte((is_data != 0U) ? LCD_SYNC_DATA : LCD_SYNC_COMMAND);
  LCD12864_SendSerialByte(value & 0xF0U);
  LCD12864_SendSerialByte((uint8_t)(value << 4));

  LCD12864_ShortDelay();
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);

  /* The data sheet gives 72 us for normal commands. 1 ms is conservative. */
  HAL_Delay(1U);
}

static void LCD12864_WriteCommand(uint8_t command)
{
  LCD12864_WriteRaw(0U, command);
}

static void LCD12864_WriteData(uint8_t data)
{
  LCD12864_WriteRaw(1U, data);
}

static void LCD12864_RefreshRow(uint8_t row)
{
  uint8_t col;

  if ((lcd_initialized == 0U) || (row >= LCD12864_ROWS))
  {
    return;
  }

  LCD12864_WriteCommand(lcd_row_address[row]);
  for (col = 0U; col < LCD12864_COLS; col++)
  {
    LCD12864_WriteData(lcd_shadow[row][col]);
  }
}

uint8_t LCD12864_Init(void)
{
  GPIO_InitTypeDef gpio = {0};

  __HAL_RCC_GPIOA_CLK_ENABLE();

  gpio.Pin = LCD_CLK_Pin | LCD_SID_Pin | LCD_CS_Pin;
  gpio.Mode = GPIO_MODE_OUTPUT_PP;
  gpio.Pull = GPIO_NOPULL;
  gpio.Speed = GPIO_SPEED_FREQ_HIGH;
  HAL_GPIO_Init(GPIOA, &gpio);

  // 通过GPIO模拟SPI时序
  HAL_GPIO_WritePin(LCD_CLK_GPIO_Port, LCD_CLK_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_SID_GPIO_Port, LCD_SID_Pin, GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);

  memset(lcd_shadow, ' ', sizeof(lcd_shadow));

  /* Power-on and reset settling time. LCD reset is pulled high on this board. */
  HAL_Delay(50U);
  LCD12864_WriteCommand(0x30U); /* Basic instruction set, 8-bit interface. */
  HAL_Delay(5U);
  LCD12864_WriteCommand(0x30U);
  HAL_Delay(5U);
  LCD12864_WriteCommand(0x0CU); /* Display on, cursor and blink off. */
  HAL_Delay(5U);
  LCD12864_WriteCommand(0x01U); /* Clear display. */
  HAL_Delay(10U);
  LCD12864_WriteCommand(0x06U); /* Cursor moves right after data write. */
  HAL_Delay(5U);

  lcd_initialized = 1U;
  return 1U;
}

void LCD12864_Clear(void)
{
  memset(lcd_shadow, ' ', sizeof(lcd_shadow));
  if (lcd_initialized != 0U)
  {
    LCD12864_WriteCommand(0x01U);
    HAL_Delay(10U);
  }
}

void lcd12864_display(uint8_t row, uint8_t col,
                      const uint8_t *buffer, uint8_t length)
{
  uint8_t index;

  if ((lcd_initialized == 0U) || (buffer == NULL) ||
      (row >= LCD12864_ROWS) || (col >= LCD12864_COLS) || (length == 0U))
  {
    return;
  }

  if (length > (LCD12864_COLS - col))
  {
    length = (uint8_t)(LCD12864_COLS - col);
  }

  for (index = 0U; index < length; index++)
  {
    uint8_t character = buffer[index];

    if ((character < 0x20U) || (character > 0x7EU))
    {
      character = '?';
    }
    lcd_shadow[row][col + index] = character;
  }

  LCD12864_RefreshRow(row);
}

void LCD12864_WriteLine(uint8_t row, const char *text)
{
  uint8_t length = 0U;

  if ((row >= LCD12864_ROWS) || (text == NULL))
  {
    return;
  }

  memset(lcd_shadow[row], ' ', LCD12864_COLS);
  while ((text[length] != '\0') && (length < LCD12864_COLS))
  {
    uint8_t character = (uint8_t)text[length];
    lcd_shadow[row][length] = ((character >= 0x20U) && (character <= 0x7EU))
                                ? character : (uint8_t)'?';
    length++;
  }
  LCD12864_RefreshRow(row);
}

void LCD12864_TestPattern(void)
{
  LCD12864_WriteLine(0U, "CANBox LCD Test");
  LCD12864_WriteLine(1U, "0123456789ABCDEF");
  LCD12864_WriteLine(2U, "Cviauto_6");
  LCD12864_WriteLine(3U, "LCD PASS?");
}
