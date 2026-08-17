/**
 * @file launcher.cpp
 * @author Forairaaaaa
 * @brief 
 * @version 0.1
 * @date 2023-07-25
 * 
 * @copyright Copyright (c) 2023
 * 
 */
#include "launcher.h"
#include "../common_define.h"
#include "../utilities/idle_screen/idle_screen.h"
#include "../utilities/weather_client/weather_client_config.h"
#include "../utilities/ntp_sync/ntp_sync.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <ctime>
#include "esp_system.h"


using namespace MOONCAKE::USER_APP;

/* Holding the physical encoder button on the home carousel this long
   reboots the device. Reads the button GPIO directly, not touch - the
   touch panel has a known intermittent I2C failure (hal_tp.hpp) that
   only clears on reboot, and when it's out, an on-screen touch button
   would be exactly as unreachable as everything else. */
static const uint32_t REBOOT_HOLD_MS = 3000;
static const int DEFAULT_SELECTED_APP = 6;  // Reachy


void Launcher::_menu_init()
{
    /* Create a menu to handle selector */
    _data.menu = new SMOOTH_MENU::Simple_Menu;
    _data.menu_render_cb = new LauncherRender_CB_t;
    _data.menu_render_cb->setCanvas(_data.hal->canvas);

    _data.menu->init(240, 240);
    _data.menu->setRenderCallback(_data.menu_render_cb);

    /* Set selector anim */
    auto cfg_selector = _data.menu->getSelector()->config();
    cfg_selector.animPath_x = LVGL::overshoot;
    cfg_selector.animPath_y = LVGL::overshoot;
    cfg_selector.animTime_x = 300;
    cfg_selector.animTime_y = 300;
    _data.menu->getSelector()->config(cfg_selector);

    /* Set menu looply */
    _data.menu->setMenuLoopMode(true);
    _data.menu->getMenu()->x = 0;
    _data.menu->getMenu()->y = 0;

    /* Push selector points into menu */
    int a = 120;
    int b = 120;
    int r = 60;
    int n = 10;
    int x;
    int y;
    for (int i = 0; i < n; i++)
    {
        x = a + r * std::cos(2 * 3.14 * i / n);
        y = b + r * std::sin(2 * 3.14 * i / n);
        _data.menu->getMenu()->addItem("", x, y, ICON_RADIUS, ICON_RADIUS);
    }

    _data.menu->goToItem(DEFAULT_SELECTED_APP);
    _data.menu->update(0, false);
}


void Launcher::_icon_list_init()
{
    for (int i = 0; i < icon_list.size(); i++)
    {
        /* Set colors */
        icon_list[i].color = icon_color_list[i];

        /* Set tags */
        icon_list[i].tag_up = icon_tag_list[i * 2];
        icon_list[i].tag_down = icon_tag_list[i * 2 + 1];

        /* Push Icon pic into sprite */
        icon_sprite_list[i].createSprite(42, 42);
        icon_sprite_list[i].pushImage(0, 0, 42, 42, icon_pic_list[i]);
    }

    /* Icon position */
    int a = 120;
    int b = 120;
    int r = 190 / 2;
    int n = 10;
    int x;
    int y;
    for (int i = 0; i < icon_list.size(); i++)
    {
        x = a + r * std::cos(2 * 3.14 * i / n);
        y = b + r * std::sin(2 * 3.14 * i / n);

        icon_list[i].x = x;
        icon_list[i].y = y;
    }
}


void Launcher::_launcher_init()
{
    _menu_init();
    _icon_list_init();
}


void Launcher::_launcher_loop()
{
    _data.menu->update(millis());
    _canvas_update();
    // delay(5);

    /* If scrolled */
    if (_data.hal->encoder.wasMoved(true))
    {
        // printf("%d\n", _data.hal->encoder.getPosition());
        if (_data.hal->encoder.getDirection() < 1)
            _data.menu->goNext();
        else 
            _data.menu->goLast();
    }

    /* If button pressed */
    if (!_data.hal->encoder.btn.read())
    {
        _data.menu->getSelector()->pressed();

        uint32_t press_start_ms = millis();

        /* Hold until button release (or the long-press reboot threshold) */
        while (!_data.hal->encoder.btn.read())
        {
            if (millis() - press_start_ms > REBOOT_HOLD_MS)
            {
                _data.hal->canvas->fillScreen(TFT_BLACK);
                _data.hal->canvas->setFont(&fonts::Font0);
                _data.hal->canvas->setTextColor(TFT_WHITE);
                _data.hal->canvas->setTextSize(2);
                _data.hal->canvas->drawCenterString("REBOOTING...", 120, 120);
                _data.hal->canvas->pushSprite(0, 0);
                delay(400);
                esp_restart();
            }

            _data.menu->update(millis());
            _canvas_update();
        }

        _data.menu->getSelector()->released();

        // /* Hold until anim finish */
        // while (!_data.menu->getSelector()->isAnimFinished())
        // {
        //     _data.menu->update(millis());
        //     _canvas_update();
        // }

        /* App open callback */
        _app_open_callback(_data.menu->getSelector()->getTargetItem());
    }

    /* If touched */
    if (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();

        // printf("%d %d\n", _data.hal->tp.getTouchPointBuffer().x, _data.hal->tp.getTouchPointBuffer().y);
        // return;

        /* Check if in the center circle (r = 50) */
        int x = _data.hal->tp.getTouchPointBuffer().x - 120;
        int y = _data.hal->tp.getTouchPointBuffer().y - 120;
        if ((x * x + y * y) > (50 * 50))
        {
            // printf("no\n");
            return;
        }
        // printf("yes\n");

        /* Call button pressed callback */
        HAL::HAL::_encoder_button_pressed_callback(nullptr, _data.hal);


        if (_data.menu->getSelector()->isAnimFinished())
        {
            /* App open callback */
            _app_open_callback(_data.menu->getSelector()->getTargetItem());

            /* Wait until released */
            while (_data.hal->tp.isTouched())
            {
                _data.menu->update(millis());
                _canvas_update();
            }
        }
    }
}


void Launcher::_screensaver_render()
{
    struct tm time_now;
    _data.hal->rtc.getTime(time_now);

    /* Snapshot the worker-owned data under the lock, then render from
       the copy - no std::string access races with _data_worker_task. */
    WEATHER_CLIENT::WeatherInfo weather;
    int mail_unread = 0;
    {
        if (xSemaphoreTake((SemaphoreHandle_t)_data_lock, pdMS_TO_TICKS(50)) == pdTRUE)
        {
            weather = _data.weather;
            mail_unread = mail_unread;
            xSemaphoreGive((SemaphoreHandle_t)_data_lock);
        }
        else
        {
            /* Lock busy (worker writing) - reuse nothing, render a
               neutral frame rather than blocking the UI tick. */
            weather.ok = false;
            mail_unread = 0;
        }
    }

    char time_buf[8];
    snprintf(time_buf, sizeof(time_buf), "%02d:%02d", time_now.tm_hour, time_now.tm_min);

    static const char* WEEKDAY_NAMES[7] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
    char date_buf[32];
    snprintf(date_buf, sizeof(date_buf), "%04d-%02d-%02d %s",
             time_now.tm_year, time_now.tm_mon + 1, time_now.tm_mday,
             WEEKDAY_NAMES[time_now.tm_wday % 7]);

    _data.hal->canvas->fillScreen(TFT_BLACK);

    /* Whichever app was last open (Sonos, Timer, etc.) may have left a
       large CJK font set on this shared canvas and never reset it (only
       textSize gets reset in each app's onDestroy(), not font) - explicitly
       set the default font here instead of relying on inherited state, or
       the very first screensaver render after closing such an app comes
       out oversized until this function's own later setFont() calls
       incidentally correct it on the next second's refresh. */
    _data.hal->canvas->setFont(&fonts::Font0);
    _data.hal->canvas->setTextColor(TFT_WHITE);
    _data.hal->canvas->setTextSize(4);
    int time_h = _data.hal->canvas->fontHeight();
    _data.hal->canvas->drawCenterString(time_buf, 120, 68 - time_h / 2);

    /* Date */
    _data.hal->canvas->setTextSize(2);
    _data.hal->canvas->drawCenterString(date_buf, 120, 90);

    /* Unread-mail indicator above the clock - hand-drawn white line
       envelope (no icon bitmap: the stock icon_email has a solid blue
       background that clashes with the black screensaver). 2px strokes:
       rounded feel, still crisp on the round panel. Blinks by toggling
       every render frame - NOT via tm_sec parity, because the render
       cadence can phase-lock to wall-clock seconds (every render
       landing on the same parity = icon stuck on). */
    static uint32_t s_render_frame = 0;
    const bool blink_on = (s_render_frame++ % 2) == 0;
    if (mail_unread > 0 && blink_on)
    {
        auto* cv = _data.hal->canvas;
        const int x0 = 110, y0 = 18, w = 20, h = 16; /* center (120,26) */
        cv->fillRect(x0, y0, w, 2, TFT_WHITE);          /* top */
        cv->fillRect(x0, y0 + h - 2, w, 2, TFT_WHITE);  /* bottom */
        cv->fillRect(x0, y0, 2, h, TFT_WHITE);          /* left */
        cv->fillRect(x0 + w - 2, y0, 2, h, TFT_WHITE);  /* right */
        /* flap: two diagonals meeting at the low point, drawn twice with
           a 1px offset for a matching 2px weight */
        cv->drawLine(x0 + 2, y0 + 2, x0 + w / 2, y0 + h / 2 + 1, TFT_WHITE);
        cv->drawLine(x0 + 2, y0 + 3, x0 + w / 2, y0 + h / 2 + 2, TFT_WHITE);
        cv->drawLine(x0 + w - 3, y0 + 2, x0 + w / 2, y0 + h / 2 + 1, TFT_WHITE);
        cv->drawLine(x0 + w - 3, y0 + 3, x0 + w / 2, y0 + h / 2 + 2, TFT_WHITE);
    }

    /* Weather block (CJK-capable font - city/condition are Chinese) */
    if (weather.ok)
    {
        _data.hal->canvas->setFont(GUI_FONT_CN_SMALL);
        _data.hal->canvas->setTextSize(1);

        char line1[48];
        snprintf(line1, sizeof(line1), "%s %s°C", weather.condition.c_str(), weather.temp_c.c_str());
        _data.hal->canvas->drawCenterString(line1, 120, 116);

        if (!weather.city.empty() || !weather.humidity.empty())
        {
            char line2[48];
            snprintf(line2, sizeof(line2), "%s 湿度 %s%%", weather.city.c_str(), weather.humidity.c_str());
            _data.hal->canvas->drawCenterString(line2, 120, 140);
        }

        if (!weather.feels_like_c.empty())
        {
            char line3[48];
            snprintf(line3, sizeof(line3), "体感 %s°C", weather.feels_like_c.c_str());
            _data.hal->canvas->drawCenterString(line3, 120, 164);
        }
    }
    else
    {
        _data.hal->canvas->setTextSize(2);
        _data.hal->canvas->drawCenterString("--", 120, 130);
    }

    /* Mail-unread ring: solid blue full circle when there is unread
       mail, nothing otherwise (no proportional meaning - purely a
       status flag at the screen edge, same 5-layer AA as before). */
    if (mail_unread > 0)
    {
        const uint16_t fill_color = TFT_BLUE;
        uint16_t c1 = (uint16_t)(fill_color >> 3) & 0x18E3;  /* 12.5% */
        uint16_t c2 = (uint16_t)(fill_color >> 2) & 0x39E7;  /* 25% */
        uint16_t c3 = (uint16_t)(fill_color >> 1) & 0x7BEF;  /* 50% */
        _data.hal->canvas->fillArc(120, 120, 118, 119, -90, 270, c1);
        _data.hal->canvas->fillArc(120, 120, 117, 118, -90, 270, c2);
        _data.hal->canvas->fillArc(120, 120, 116, 117, -90, 270, c3);
        _data.hal->canvas->fillArc(120, 120, 115, 116, -90, 270, (uint16_t)(c3 | c2));
        _data.hal->canvas->fillArc(120, 120, 114, 115, -90, 270, fill_color);
    }

    _data.hal->canvas->pushSprite(0, 0);
}


void Launcher::_screensaver_tick()
{
    if (!_data.screensaver_initialized)
    {
        _data.screensaver_last_activity_ms = millis();
        _data.screensaver_last_encoder_count = _data.hal->encoder.getCount();
        _data.screensaver_initialized = true;
    }

    bool touched = _data.hal->tp.isTouched();
    if (touched)
    {
        /* TP_FT3267::isTouched() is a bare I2C register read with no
           error checking (hal_tp.hpp) - an occasional bus glitch can
           report a phantom touch for a single poll, and this function
           gets polled at very high frequency while idle. A real finger
           touch holds for far longer than one poll, so require it to
           still read true after a short delay before trusting it,
           instead of waking/dismissing the screensaver on one noisy
           read (this was observed live: screensaver dismissed after
           only ~31s idle with nobody touching the device). */
        delay(20);
        touched = _data.hal->tp.isTouched();
    }

    /* Read the raw count directly (no side effects) instead of
       wasMoved(), which the carousel's own scroll logic already
       consumes in _launcher_loop() - calling it here too would eat
       rotation before the carousel sees it. */
    int64_t current_count = _data.hal->encoder.getCount();
    bool encoder_moved = (current_count != _data.screensaver_last_encoder_count);
    _data.screensaver_last_encoder_count = current_count;

    bool button_pressed = !_data.hal->encoder.btn.read();

    bool activity = touched || encoder_moved || button_pressed;

    if (_data.screensaver_on)
    {
        if (activity)
        {
            if (touched)
            {
                while (_data.hal->tp.isTouched())
                {
                    _data.hal->tp.update();
                    delay(5);
                }
            }
            if (button_pressed)
            {
                while (!_data.hal->encoder.btn.read())
                    delay(5);
            }

            _data.screensaver_on = false;
            _data.screensaver_last_activity_ms = millis();
            return;
        }

        /* Refresh the displayed clock once a second while idle */
        if (millis() - _data.screensaver_last_render_ms >= 1000)
        {
            _screensaver_render();
            _data.screensaver_last_render_ms = millis();
        }
        return;
    }

    if (activity)
    {
        _data.screensaver_last_activity_ms = millis();
        return;
    }

    if (millis() - _data.screensaver_last_activity_ms > 30000)
    {
        _data.screensaver_on = true;
        _data.screensaver_started_ms = millis();
        /* Ask the data worker for an immediate poll (non-blocking -
           see Data_t::weather). */
        _data.force_poll = true;
        _screensaver_render();
        _data.screensaver_last_render_ms = millis();
    }
}


void Launcher::_app_open_callback(uint8_t selectedNum)
{
    _log("selected %d", selectedNum);

    /* If not in icon list */
    if (selectedNum >= icon_list.size())
    {
        return;
    }


    /* Special color for app more menu */
    uint32_t theme_color = 0;
    if (selectedNum != 7)
        theme_color = icon_list[selectedNum].color;
    else
        theme_color = 0;


    /* Play app open anim */
    for (int i = 0; i < 10; i++)
    {
        _data.hal->canvas->fillSmoothCircle(icon_list[selectedNum].x, icon_list[selectedNum].y, i * 24, theme_color);
        _canvas_update();
    }

    

    // /* ----------------------- Simple test ----------------------- */
    // std::array<GUI_Base*, ICON_NUM> gui_list;
    // gui_list[0] = new GUI_Base;
    // gui_list[1] = new GUI_Base;
    // gui_list[2] = new GUI_Base;
    // gui_list[3] = new GUI_Base;
    // gui_list[4] = new GUI_Base;
    // gui_list[5] = new GUI_Base;
    // gui_list[6] = new GUI_Base;
    // gui_list[7] = new GUI_Base;

    // /* Open app */
    // gui_list[selectedNum]->setThemeColor(icon_list[selectedNum].color);
    // gui_list[selectedNum]->init(_data.hal->canvas, &icon_sprite_list[selectedNum]);
    // while (1)
    // {
    //     if (_data.hal->encoder.btn.pressed())
    //     {
    //         /* Hold until button release */
    //         while (!_data.hal->encoder.btn.read());
    //         break;
    //     }
    // }
    // /* ----------------------- Simple test ----------------------- */






    // /* ----------------------- Simple app test ----------------------- */
    // std::array<MOONCAKE::APP_BASE*, ICON_NUM> app_list;
    // app_list[0] = new MOONCAKE::USER_APP::LCD_Test;
    // app_list[1] = new MOONCAKE::USER_APP::RTC_Test;
    // app_list[2] = new MOONCAKE::USER_APP::RFID_Test;
    // app_list[3] = new MOONCAKE::USER_APP::Set_Brightness;
    // app_list[4] = new MOONCAKE::USER_APP::WiFi_Scan;
    // app_list[5] = new MOONCAKE::APP_BASE;
    // app_list[6] = new MOONCAKE::APP_BASE;
    // app_list[7] = new MOONCAKE::USER_APP::VideoShit;

    // if (app_list[selectedNum]->getGui() != nullptr)
    // {
    //     app_list[selectedNum]->getGui()->setThemeColor(icon_list[selectedNum].color);
    //     app_list[selectedNum]->getGui()->init(_data.hal->canvas, &icon_sprite_list[selectedNum]);

    //     _simple_app_manager(app_list[selectedNum]);
    // }

    // /* Free */
    // for (auto& i : app_list)
    // {
    //     delete i;
    // }
    // /* ----------------------- Simple app test ----------------------- */




    /* Memery leak check */
    size_t mem_before_open = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    _log("free mem: %d", mem_before_open);



    MOONCAKE::APP_BASE* app_ptr = nullptr;

    /* Create app */
    switch (selectedNum)
    {
        case 0:
            app_ptr = new MOONCAKE::USER_APP::LCD_Test;
            break;
        case 1:
            app_ptr = new MOONCAKE::USER_APP::RTC_Test;
            break;
        case 2:
            app_ptr = new MOONCAKE::USER_APP::RFID_Test;
            break;
        case 3:
            app_ptr = new MOONCAKE::USER_APP::Set_Brightness;
            break;
        case 4:
            app_ptr = new MOONCAKE::USER_APP::WiFi_Scan;
            break;
        case 5:
            app_ptr = new MOONCAKE::USER_APP::BLE_Server;
            break;
        case 6:
            app_ptr = new MOONCAKE::USER_APP::AppReachy;
            break;
        case 7:
            app_ptr = new MOONCAKE::USER_APP::MoreMenu;
            break;
        case 8:
            app_ptr = new MOONCAKE::USER_APP::AppTimer;
            break;
        case 9:
            app_ptr = new MOONCAKE::USER_APP::AppSonos;
            break;
        default:
            break;
    };

    /* If app created */
    if (app_ptr != nullptr)
    {
        /* Init if gui module exsit */
        if (app_ptr->getGui() != nullptr)
        {
            app_ptr->getGui()->setThemeColor(icon_list[selectedNum].color);
            app_ptr->getGui()->init(_data.hal->canvas, &icon_sprite_list[selectedNum]);
        }

        /* Run app */
        _simple_app_manager(app_ptr, selectedNum == 6);
        
        /* Free app */
        delete app_ptr;
    }



    /* Memery leak check */
    _log_mem();
    if (heap_caps_get_free_size(MALLOC_CAP_INTERNAL) < mem_before_open)
    {
        _log_e("memory leak: %d", mem_before_open - heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    }
    
    

    /* Play app close anim */
    for (int i = 10; i > 1; i--)
    {
        _data.menu->update(millis());
        _data.hal->canvas->fillSmoothCircle(icon_list[selectedNum].x, icon_list[selectedNum].y, i * 24, theme_color);
        _canvas_update();
    }
}


void Launcher::_simple_app_manager(MOONCAKE::APP_BASE* app, bool suppress_idle_screen)
{
    app->setUserData((void*)_data.hal);
    app->onSetup();
    app->onCreate();
    while (1)
    {
        if (suppress_idle_screen || !IDLE_SCREEN::tick(_data.hal))
        {
            app->onRunning();
        }
        if (app->isGoingDestroy())
        {
            app->resetGoingDestroyFlag();
            app->onDestroy();
            break;
        }

        // if (_data.hal->encoder.btn.pressed())
        // {
        //     /* Hold until button release */
        //     while (!_data.hal->encoder.btn.read());
        //     break;
        // }
    }
}


Launcher::~Launcher()
{
    delete _data.menu;
    delete _data.menu_render_cb;
}


void Launcher::onSetup()
{
    setAppName("Launcher");
    setAllowBgRunning(false);

    /* Init with default value */
    LAUNCHER::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


/* Life cycle */
void Launcher::onCreate()
{
    _log("onCreate");
    
    _launcher_init();
    _data.menu->getSelector()->reset(millis());

    /* Start the background data worker (weather + unread mail). See
       Data_t::weather for why this must NOT run on the launcher task. */
    _data_lock = xSemaphoreCreateMutex();
    if (_data_lock == nullptr)
    {
        ESP_LOGE(_tag, "failed to create data lock");
        return;
    }
    if (xTaskCreate(_data_worker_task, "data_worker", 8192, this, 5, nullptr) != pdPASS)
        ESP_LOGE(_tag, "failed to create data worker");
}

void Launcher::_data_worker_task(void* arg)
{
    auto* self = (Launcher*)arg;
    auto& data = self->_data;

    while (true)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));

        /* Weather: 10 minutes when healthy, 30s retry while failing.
           Mail: 2 minutes. force_poll pulls both immediately
           (screensaver entry). */
        bool force;
        {
            /* Also take the lock for the flag clear - tiny window but
               keep it clean vs the launcher setting it meanwhile. */
            if (xSemaphoreTake((SemaphoreHandle_t)self->_data_lock, pdMS_TO_TICKS(100)) != pdTRUE)
                continue;
            force = data.force_poll;
            data.force_poll = false;
            xSemaphoreGive((SemaphoreHandle_t)self->_data_lock);
        }

        uint32_t now = millis();
        uint32_t weather_period = 600000;
        bool weather_ok;
        {
            if (xSemaphoreTake((SemaphoreHandle_t)self->_data_lock, pdMS_TO_TICKS(100)) != pdTRUE)
                continue;
            weather_ok = data.weather.ok;
            xSemaphoreGive((SemaphoreHandle_t)self->_data_lock);
        }
        if (!weather_ok)
            weather_period = 30000;

        if (force || now - data.weather_last_poll_ms > weather_period)
        {
            auto w = WEATHER_CLIENT::get_weather(WEATHER_SERVER_URL);
            if (xSemaphoreTake((SemaphoreHandle_t)self->_data_lock, portMAX_DELAY) == pdTRUE)
            {
                data.weather = w;
                data.weather_last_poll_ms = millis();
                xSemaphoreGive((SemaphoreHandle_t)self->_data_lock);
            }
        }

        if (force || now - data.mail_last_poll_ms > 120000)
        {
            auto folders = EMAIL_CLIENT::get_unread(EMAIL_API_BASE_URL);
            int unread = 0;
            for (auto& f : folders)
                unread += f.unread;
            if (xSemaphoreTake((SemaphoreHandle_t)self->_data_lock, portMAX_DELAY) == pdTRUE)
            {
                data.mail_unread = unread;
                data.mail_last_poll_ms = millis();
                xSemaphoreGive((SemaphoreHandle_t)self->_data_lock);
            }
        }
    }
}


void Launcher::onRunning()
{
    /* Retry NTP if RTC holds garbage (e.g. after flash with dead battery) */
    static uint32_t ntp_retry_ms = 0;
    if (millis() - ntp_retry_ms > 30000)
    {
        struct tm now;
        _data.hal->rtc.getTime(now);
        if (now.tm_hour > 23 || now.tm_min > 59 || now.tm_mday > 31 || now.tm_mon > 11)
        {
            ESP_LOGW("Launcher", "RTC garbage detected, retrying NTP...");
            NTP_SYNC::sync_rtc_time(_data.hal, 15000);
        }
        ntp_retry_ms = millis();
    }

    /* Weather is slow-changing: refresh every 10 minutes - but on
       failure retry in 30s so a flaky WiFi link self-heals quickly
       instead of blanking the screensaver for a full cycle. Unread mail
       can change any minute: refresh every 2 minutes. Both polls run
       on _data_worker_task - nothing network touches this task. */

    _screensaver_tick();
    if (!_data.screensaver_on)
    {
        _launcher_loop();
    }
}
