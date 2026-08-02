/**
 * @file hal_common_define.h
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-08-14
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#pragma once


#define HAL_PIN_PWR_HOLDING     46
#define HAL_PIN_PWR_WAKE_UP     42

#define HAL_PIN_ENCODER_A       41
#define HAL_PIN_ENCODER_B       40

#define HAL_PIN_TP_I2C_SCL      12
#define HAL_PIN_TP_I2C_SDA      11
#define HAL_PIN_TP_INT          14

#define HAL_PIN_GROVE_I2C_SCL   15
#define HAL_PIN_GROVE_I2C_SDA   13

#define HAL_PIN_BUZZER          3

/* Shared with the GC9A01 panel (display driver uses it as RST). Exposed here
   so the touch self-heal can pull it low to do a hardware reset of both the
   panel and the FT3267 controller when the scan engine hangs. */
#define HAL_PIN_LCD_RST         8
