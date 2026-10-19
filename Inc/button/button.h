#ifndef BUTTON_H
#define BUTTON_H

#include "stm32f4xx_hal.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


typedef enum
{
  BUTTON_STATE_IDLE = 0,
  BUTTON_STATE_DEBOUNCE_PRESS,
  BUTTON_STATE_PRESSED,
  BUTTON_STATE_DEBOUNCE_RELEASE
} ButtonState_t;

typedef void (*ButtonCallback_t)(void *arg);

typedef struct
{
  GPIO_TypeDef *port;
  uint16_t pin;
  ButtonState_t state;
  uint32_t debounce_ms;
  uint32_t last_change_ms;
  ButtonCallback_t pressed_cb;
  ButtonCallback_t released_cb;
  void *arg;
} Button_t;

HAL_StatusTypeDef Button_Init(Button_t *button,
                              GPIO_TypeDef *port,
                              uint16_t pin,
                              uint32_t debounce_ms,
                              ButtonCallback_t pressed_cb,
                              ButtonCallback_t released_cb,
                              void *arg);
HAL_StatusTypeDef Button_Update(Button_t *button);
HAL_StatusTypeDef Button_IsPressed(const Button_t *button, bool *pressed);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_H */
