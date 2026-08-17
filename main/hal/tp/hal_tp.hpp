/**
 * @file hal_tp.hpp
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-05-20
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#pragma once
#include <driver/i2c.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_err.h>
#include <esp_system.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <cstring>


/** @brief FT5x06 register map and function codes */
#define FT5x06_ADDR                    (0x38)

#define FT5x06_DEVICE_MODE             (0x00)
#define FT5x06_GESTURE_ID              (0x01)
#define FT5x06_TOUCH_POINTS            (0x02)

#define FT5x06_TOUCH1_EV_FLAG          (0x03)
#define FT5x06_TOUCH1_XH               (0x03)
#define FT5x06_TOUCH1_XL               (0x04)
#define FT5x06_TOUCH1_YH               (0x05)
#define FT5x06_TOUCH1_YL               (0x06)

#define FT5x06_TOUCH2_EV_FLAG          (0x09)
#define FT5x06_TOUCH2_XH               (0x09)
#define FT5x06_TOUCH2_XL               (0x0A)
#define FT5x06_TOUCH2_YH               (0x0B)
#define FT5x06_TOUCH2_YL               (0x0C)

#define FT5x06_TOUCH3_EV_FLAG          (0x0F)
#define FT5x06_TOUCH3_XH               (0x0F)
#define FT5x06_TOUCH3_XL               (0x10)
#define FT5x06_TOUCH3_YH               (0x11)
#define FT5x06_TOUCH3_YL               (0x12)

#define FT5x06_TOUCH4_EV_FLAG          (0x15)
#define FT5x06_TOUCH4_XH               (0x15)
#define FT5x06_TOUCH4_XL               (0x16)
#define FT5x06_TOUCH4_YH               (0x17)
#define FT5x06_TOUCH4_YL               (0x18)

#define FT5x06_TOUCH5_EV_FLAG          (0x1B)
#define FT5x06_TOUCH5_XH               (0x1B)
#define FT5x06_TOUCH5_XL               (0x1C)
#define FT5x06_TOUCH5_YH               (0x1D)
#define FT5x06_TOUCH5_YL               (0x1E)

#define FT5x06_ID_G_THGROUP            (0x80)
#define FT5x06_ID_G_THPEAK             (0x81)
#define FT5x06_ID_G_THCAL              (0x82)
#define FT5x06_ID_G_THWATER            (0x83)
#define FT5x06_ID_G_THTEMP             (0x84)
#define FT5x06_ID_G_THDIFF             (0x85)
#define FT5x06_ID_G_CTRL               (0x86)
#define FT5x06_ID_G_TIME_ENTER_MONITOR (0x87)
#define FT5x06_ID_G_PERIODACTIVE       (0x88)
#define FT5x06_ID_G_PERIODMONITOR      (0x89)
#define FT5x06_ID_G_AUTO_CLB_MODE      (0xA0)
#define FT5x06_ID_G_LIB_VERSION_H      (0xA1)
#define FT5x06_ID_G_LIB_VERSION_L      (0xA2)
#define FT5x06_ID_G_CIPHER             (0xA3)
#define FT5x06_ID_G_MODE               (0xA4)
#define FT5x06_ID_G_PMODE              (0xA5)
#define FT5x06_ID_G_FIRMID             (0xA6)
#define FT5x06_ID_G_STATE              (0xA7)
#define FT5x06_ID_G_FT5201ID           (0xA8)
#define FT5x06_ID_G_ERR                (0xA9)


namespace FT3267
{
    static const char* TAG = "ft3267";


    enum ft3267_gesture_t
    {
        ft3267_gesture_none         = 0x00,
        ft3267_gesture_move_up      = 0x10,
        ft3267_gesture_move_left    = 0x14,
        ft3267_gesture_move_down    = 0x18,
        ft3267_gesture_move_right   = 0x1c,
        ft3267_gesture_zoom_in      = 0x48,
        ft3267_gesture_zoom_out     = 0x49,
    };


    struct TouchPoint_t
    {
        uint8_t touch_num = 0;
        int x = -1;
        int y = -1;
    };


    struct Config_t
    {
        i2c_port_t i2c_port = I2C_NUM_0;
        uint8_t dev_addr = FT5x06_ADDR;
    };


    class TP_FT3267
    {
        private:
            Config_t _cfg;
            uint8_t _data_buffer[7];
            TouchPoint_t _touch_point_buffer;


            inline esp_err_t _writr_reg(uint8_t reg, uint8_t data)
            {
                _data_buffer[0] = reg;
                _data_buffer[1] = data; 
                return i2c_master_write_to_device(_cfg.i2c_port, _cfg.dev_addr, _data_buffer, 2, pdMS_TO_TICKS(200));
            }


            inline esp_err_t _read_reg(uint8_t reg, uint8_t readSize)
            {
                /* Store data into buffer */
                return i2c_master_write_read_device(_cfg.i2c_port, _cfg.dev_addr, &reg, 1, _data_buffer, readSize, pdMS_TO_TICKS(200));
            }


            /* Surface only the transitions, not a 1Hz heartbeat. The
               touch-poll loop calls this at ~10Hz, so a steady-state
               heartbeat would flood the console; a healthy controller
               is by definition boring. The transitions are the signal:
                 - err changes (esp. OK -> FAIL): i2c/wedged-controller entry
                 - touch_num changes: real finger landing or leaving
               Both are how we notice a hang kicking in. */
            inline void _diag(esp_err_t err, uint8_t touch_num)
            {
                static esp_err_t s_last_err = ESP_OK;
                static uint8_t s_last_num = 0xFF;  /* impossible - forces first-sample log */

                if (err != s_last_err)
                {
                    ESP_LOGW(TAG, "[TP-DIAG] err changed: %s (0x%x), touch_num=%u",
                             esp_err_to_name(err), err, touch_num);
                    s_last_err = err;
                    s_last_num = touch_num;
                    return;
                }

                if (touch_num != s_last_num)
                {
                    ESP_LOGW(TAG, "[TP-DIAG] touch_num changed: %u (err=%s)",
                             touch_num, esp_err_to_name(err));
                    s_last_num = touch_num;
                }
            }


            inline void _tp_init()
            {
                // Valid touching detect threshold
                _writr_reg(FT5x06_ID_G_THGROUP, 70);

                // valid touching peak detect threshold
                _writr_reg(FT5x06_ID_G_THPEAK, 60);

                // Touch focus threshold
                _writr_reg(FT5x06_ID_G_THCAL, 16);

                // threshold when there is surface water
                _writr_reg(FT5x06_ID_G_THWATER, 60);

                // threshold of temperature compensation
                _writr_reg(FT5x06_ID_G_THTEMP, 10);

                // Touch difference threshold
                _writr_reg(FT5x06_ID_G_THDIFF, 20);

                // Delay to enter 'Monitor' status (s)
                _writr_reg(FT5x06_ID_G_TIME_ENTER_MONITOR, 2);

                // Period of 'Active' status (ms)
                _writr_reg(FT5x06_ID_G_PERIODACTIVE, 12);

                // Timer to enter 'idle' when in 'Monitor' (ms)
                _writr_reg(FT5x06_ID_G_PERIODMONITOR, 40);

                // _read_reg(0x90, 1);
                // printf("0x%X\n", _data_buffer[0]);
                // _read_reg(FT5x06_ID_G_FIRMID, 1);
                // printf("0x%X\n", _data_buffer[0]);
                // _read_reg(FT5x06_ID_G_FT5201ID, 1);
                // printf("0x%X\n", _data_buffer[0]);
            }


            /* Log the controller's own state registers right before each
               heal tier fires. After the run, you can see in the log
               whether L1/L2/L3 actually saw the known hang signature
               (G_MODE=0x00, FIRMID=0x00) or some other odd state - which
               is what determines whether to extend the heal ladder next
               time the field reports a new failure mode. Clobbers
               _data_buffer (call after the read you care about). */
            inline void _dump_status(const char* when)
            {
                const uint8_t regs[4] = { FT5x06_ID_G_MODE, FT5x06_ID_G_PMODE,
                                          FT5x06_ID_G_STATE, FT5x06_ID_G_FIRMID };
                uint8_t v[4];
                for (int i = 0; i < 4; i++)
                    v[i] = (_read_reg(regs[i], 1) == ESP_OK) ? _data_buffer[0] : 0xEE;

                ESP_LOGW(TAG, "[TP-DIAG] status(%s) G_MODE=0x%02x G_PMODE=0x%02x G_STATE=0x%02x FIRMID=0x%02x",
                         when, v[0], v[1], v[2], v[3]);
            }


            /* FT3267 scan engine has been observed to hang after hours
               of uptime: I2C still ACKs, but touch_num stays 0 even under
               a finger. The tell is that G_MODE drops from 0x01 to 0x00
               and FIRMID from 0x06 to 0x00, which a plain _tp_init() does
               NOT recover from (re-initialising the threshold registers
               doesn't restart the scan engine - validated against an
               offline repro that stayed at touch_num=0 through multiple
               re-inits).

               Escalate in 3 tiers, each attempt spaced one interval
               apart so a healthy controller is never disturbed:

                 L1 = rewrite threshold registers (_tp_init) - cheap, almost
                      never enough on its own but worth trying first
                 L2 = write DEVICE_MODE=0 to trigger the controller's
                      internal reset path, then re-init

               There used to be an L3 = pull LCD_RST low 20ms (the FT3267
               shares that pin with the GC9A01 panel) but that left the
               display permanently black: LovyanGFX doesn't know about
               the reset and never re-runs the panel init sequence.
               Rather than wire a display-reinit callback into the TP
               driver, we stop at L2 - if both tiers fail, log loudly
               and let the user hold the encoder button 3s to reboot.

               The ladder only advances when we actually see the known
               hang signature (G_MODE=0x00, FIRMID=0x00, i2c alive) -
               otherwise L3 would fire on every long idle period and
               blank the shared LCD panel. Any tier that brings
               touch_num > 0 resets back to L0. Only runs while idle
               (touch_num == 0) so an in-progress touch is never
               disrupted. */
            inline void _maintenance(uint8_t touch_num)
            {
                static const uint32_t REINIT_INTERVAL_MS = 20000;
                static uint32_t s_last_reinit_ms = 0;
                static int s_heal_lvl = 0;   /* 0=idle, 1=L1 done, 2=L2 done, 3=exhausted */

                uint32_t now_ms = (uint32_t)(esp_timer_get_time() / 1000);
                if (s_last_reinit_ms == 0) { s_last_reinit_ms = now_ms; return; }

                /* Touch recovered - drop back to idle so we don't escalate
                   a future hang past the level that actually fixed the last one. */
                if (touch_num > 0)
                {
                    if (s_heal_lvl != 0)
                    {
                        ESP_LOGI(TAG, "[TP-DIAG] touch recovered at heal-lvl=%d, reset to idle",
                                 s_heal_lvl);
                        s_heal_lvl = 0;
                    }
                    return;
                }

                if (now_ms - s_last_reinit_ms < REINIT_INTERVAL_MS) return;

                s_last_reinit_ms = now_ms;

                /* Distinguish three idle-but-touch_num=0 cases. Both healthy
                   idle and a wedged i2c bus look identical from the read
                   path alone, so peek at the controller's own state
                   registers:
                     - healthy idle: G_MODE=0x01, FIRMID=0x06
                     - known hang:   G_MODE=0x00, FIRMID=0x00 (i2c alive)
                     - i2c dead:     register reads return non-OK
                   Any other G_MODE/FIRMID combo is treated as a "weird
                   state" - logged but NOT healed, since the heal ladder
                   is only known to fix the specific (0x00, 0x00) case. */
                esp_err_t e_mode  = _read_reg(FT5x06_ID_G_MODE,   1);
                uint8_t g_mode   = (e_mode == ESP_OK) ? _data_buffer[0] : 0xEE;
                esp_err_t e_firm  = _read_reg(FT5x06_ID_G_FIRMID, 1);
                uint8_t g_firmid = (e_firm == ESP_OK) ? _data_buffer[0] : 0xEE;

                bool i2c_alive   = (e_mode == ESP_OK) && (e_firm == ESP_OK);
                bool looks_hung  = i2c_alive && (g_mode == 0x00) && (g_firmid == 0x00);
                bool looks_healthy = i2c_alive && (g_mode == 0x01);

                if (looks_healthy)
                {
                    ESP_LOGI(TAG, "[TP-DIAG] idle, controller healthy "
                                  "(G_MODE=0x%02x FIRMID=0x%02x) - no heal",
                             g_mode, g_firmid);
                    s_heal_lvl = 0;
                    return;
                }

                if (!i2c_alive)
                {
                    ESP_LOGW(TAG, "[TP-DIAG] idle but I2C read failed (mode=%s, firm=%s) "
                                  "- skip heal, retry next interval",
                             esp_err_to_name(e_mode), esp_err_to_name(e_firm));
                    return;
                }

                if (!looks_hung)
                {
                    /* Not the known hang signature, and not healthy, and
                       not an i2c failure. Could be a transient mode (e.g.
                       FT3267 in Monitor state with G_MODE=0x02). Logging
                       only - we don't escalate a heal we have no reason
                       to believe will help. */
                    ESP_LOGW(TAG, "[TP-DIAG] idle, weird state "
                                  "G_MODE=0x%02x FIRMID=0x%02x - skip heal, log only",
                             g_mode, g_firmid);
                    return;
                }

                /* Known hang signature. Walk up the heal ladder. */
                ESP_LOGW(TAG, "[TP-DIAG] HANG DETECTED G_MODE=0x%02x FIRMID=0x%02x",
                         g_mode, g_firmid);

                if (s_heal_lvl < 1)
                {
                    s_heal_lvl = 1;
                    _dump_status("pre-L1");
                    _tp_init();
                    ESP_LOGW(TAG, "[TP-DIAG] heal L1 fired (soft re-init)");
                }
                else if (s_heal_lvl < 2)
                {
                    s_heal_lvl = 2;
                    _dump_status("pre-L2");
                    _writr_reg(FT5x06_DEVICE_MODE, 0x00);
                    vTaskDelay(pdMS_TO_TICKS(50));
                    _tp_init();
                    ESP_LOGW(TAG, "[TP-DIAG] heal L2 fired (DEVICE_MODE soft reset)");
                }
                else
                {
                    /* L1 and L2 both failed. Stop trying - the earlier L3
                       (LCD_RST pulse) left the GC9A01 panel permanently
                       black because LovyanGFX doesn't re-init the panel
                       after a hardware reset. A full restart is the only
                       observed safe recovery path from this terminal state. */
                    s_heal_lvl = 3;
                    _dump_status("ladder-exhausted");
                    ESP_LOGE(TAG, "[TP-DIAG] heal ladder exhausted: L1 and L2 "
                                  "both failed. Restarting device.");
                    vTaskDelay(pdMS_TO_TICKS(500));
                    esp_restart();
                }
            }


        public:
            TP_FT3267()
            {
                memset(_data_buffer, 0, sizeof(_data_buffer));
            }
            ~TP_FT3267() = default;

            
            /* Config */
            inline Config_t getConfig() { return _cfg; }
            inline void setConfig(const Config_t& cfg) { _cfg = cfg; }


            inline bool init()
            {
                ESP_LOGI(TAG, "init tp ft3267");
                
                /* Interrupt pin */
                gpio_reset_pin(GPIO_NUM_14);
                gpio_set_direction(GPIO_NUM_14, GPIO_MODE_INPUT);
                gpio_set_pull_mode(GPIO_NUM_14, GPIO_PULLUP_ONLY);

                _tp_init();

                return true;
            }


            inline uint8_t getTouchPointsNum()
            {
                _data_buffer[0] = 0;
                esp_err_t err = _read_reg(FT5x06_TOUCH_POINTS, 1);
                uint8_t raw = _data_buffer[0];   // save before _maintenance clobbers _data_buffer
                uint8_t touch_num = (err == ESP_OK) ? (raw & 0x0F) : 0;
                if (touch_num > 5)
                    touch_num = 0;  /* FT5x06 max 5 points - garbage means the read lied */
                _diag(err, touch_num);
                _maintenance(touch_num);
                return touch_num;
            }


            inline const TouchPoint_t& readPos()
            {
                _touch_point_buffer.touch_num = 0;
                _touch_point_buffer.x = -1;
                _touch_point_buffer.y = -1;

                /* Get touch num */
                esp_err_t err = _read_reg(FT5x06_TOUCH_POINTS, 1);
                _data_buffer[0] = _data_buffer[0] & 0x0F;
                _touch_point_buffer.touch_num = _data_buffer[0];
                _diag(err, _data_buffer[0]);

                /* Get postion */
                if (_data_buffer[0] != 0)
                {
                    _read_reg(FT5x06_TOUCH1_XH, 4);
                    _touch_point_buffer.x = ((_data_buffer[0] & 0x0f) << 8) + _data_buffer[1];
                    _touch_point_buffer.y = ((_data_buffer[2] & 0x0f) << 8) + _data_buffer[3];
                }

                return _touch_point_buffer;
            }


            inline bool isTouched()
            {
                return (getTouchPointsNum() > 0);
            }


            /**
             * @brief Update internal touch point buffer
             * 
             */
            inline void update()
            {
                readPos();
            }


            /**
             * @brief Get internal touch point buffer
             * 
             * @return TouchPoint_t 
             */
            inline TouchPoint_t getTouchPointBuffer()
            {
                return _touch_point_buffer;
            }
    };


}
