#ifndef __LCD12864_H
#define __LCD12864_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"

#define LCD12864_ROWS 4U
#define LCD12864_COLS 16U

/*
 * QC12864B/ST7920 three-wire serial interface:
 *   PA5 -> LCD_CLK (E)
 *   PA6 -> LCD_SID (RW)
 *   PA7 -> LCD_CS  (RS)
 * PSB is strapped low on the board to select serial mode.
 */
uint8_t LCD12864_Init(void);
void LCD12864_Clear(void);
void LCD12864_WriteLine(uint8_t row, const char *text);
void LCD12864_TestPattern(void);

/* Required project interface: row 0..3, col 0..15, clipped at line end. */
void lcd12864_display(uint8_t row, uint8_t col,
                      const uint8_t *buffer, uint8_t length);

#ifdef __cplusplus
}
#endif

#endif /* __LCD12864_H */
