#include "button/button.h"

static void Button_ClockEnable(GPIO_TypeDef *port)
{
  if (port == GPIOA)
  {
    __HAL_RCC_GPIOA_CLK_ENABLE();
  }
  else if (port == GPIOB)
  {
    __HAL_RCC_GPIOB_CLK_ENABLE();
  }
  else if (port == GPIOC)
  {
    __HAL_RCC_GPIOC_CLK_ENABLE();
  }
  else if (port == GPIOD)
  {
    __HAL_RCC_GPIOD_CLK_ENABLE();
  }
  else if (port == GPIOE)
  {
    __HAL_RCC_GPIOE_CLK_ENABLE();
  }
  else if (port == GPIOH)
  {
    __HAL_RCC_GPIOH_CLK_ENABLE();
  }
}

HAL_StatusTypeDef Button_Init(Button_t *button,
                              GPIO_TypeDef *port,
                              uint16_t pin,
                              uint32_t debounce_ms,
                              ButtonCallback_t pressed_cb,
                              ButtonCallback_t released_cb,
                              void *arg)
{
  GPIO_InitTypeDef gpio_init = {0};

  if ((button == NULL) || (port == NULL) || (pin == 0))
  {
    return HAL_ERROR;
  }

  button->port = port;
  button->pin = pin;
  button->state = BUTTON_STATE_IDLE;
  button->debounce_ms = debounce_ms;
  button->last_change_ms = 0;
  button->pressed_cb = pressed_cb;
  button->released_cb = released_cb;
  button->arg = arg;

  Button_ClockEnable(port);

  gpio_init.Pin = pin;
  gpio_init.Mode = GPIO_MODE_INPUT;
  gpio_init.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(port, &gpio_init);

  return HAL_OK;
}

HAL_StatusTypeDef Button_Update(Button_t *button)
{
  bool is_low = false;
  uint32_t now = 0;

  if (button == NULL)
  {
    return HAL_ERROR;
  }

  is_low = (HAL_GPIO_ReadPin(button->port, button->pin) == GPIO_PIN_RESET);
  now = HAL_GetTick();

  switch (button->state)
  {
    case BUTTON_STATE_IDLE:
      if (is_low)
      {
        button->state = BUTTON_STATE_DEBOUNCE_PRESS;
        button->last_change_ms = now;
      }
      break;

    case BUTTON_STATE_DEBOUNCE_PRESS:
      if ((now - button->last_change_ms) >= button->debounce_ms)
      {
        if (is_low)
        {
          button->state = BUTTON_STATE_PRESSED;
          if (button->pressed_cb != NULL)
          {
            button->pressed_cb(button->arg);
          }
        }
        else
        {
          /* Short glitch, not a real press. */
          button->state = BUTTON_STATE_IDLE;
        }
      }
      break;

    case BUTTON_STATE_PRESSED:
      if (!is_low)
      {
        button->state = BUTTON_STATE_DEBOUNCE_RELEASE;
        button->last_change_ms = now;
      }
      break;

    case BUTTON_STATE_DEBOUNCE_RELEASE:
      if ((now - button->last_change_ms) >= button->debounce_ms)
      {
        if (!is_low)
        {
          button->state = BUTTON_STATE_IDLE;
          if (button->released_cb != NULL)
          {
            button->released_cb(button->arg);
          }
        }
        else
        {
          /* Bounce on release, the button is still held. */
          button->state = BUTTON_STATE_PRESSED;
        }
      }
      break;

    default:
      button->state = BUTTON_STATE_IDLE;
      break;
  }

  return HAL_OK;
}

HAL_StatusTypeDef Button_IsPressed(const Button_t *button, bool *pressed)
{
  if ((button == NULL) || (pressed == NULL))
  {
    return HAL_ERROR;
  }

  *pressed = (button->state == BUTTON_STATE_PRESSED) ||
             (button->state == BUTTON_STATE_DEBOUNCE_RELEASE);
  return HAL_OK;
}
