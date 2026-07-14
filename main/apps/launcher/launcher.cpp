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
#include "../utilities/weather_client/weather_client.h"
#include <ctime>


using namespace MOONCAKE::USER_APP;

/* Backlight off after the screensaver has been showing this long with no
   activity; restored to this brightness on wake (same value IDLE_SCREEN
   uses for its own screen-off/on cycle inside apps). */
static const uint32_t SCREENSAVER_SCREEN_OFF_MS = 3 * 60 * 1000;
static const int SCREENSAVER_ON_BRIGHTNESS = 128;


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

        /* Hold until button release */
        while (!_data.hal->encoder.btn.read())
        {
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


void Launcher::_fetch_weather()
{
    WEATHER_CLIENT::WeatherInfo info = WEATHER_CLIENT::get_weather(WEATHER_API_BASE_URL);
    _data.weather_ok = info.ok;
    _data.weather_temp_c = info.temp_c;
    _data.weather_condition = info.condition;
}


void Launcher::_screensaver_render()
{
    struct tm time_now;
    _data.hal->rtc.getTime(time_now);

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
    _data.hal->canvas->drawCenterString(time_buf, 120, 72 - time_h / 2);

    _data.hal->canvas->setTextSize(2);
    _data.hal->canvas->drawCenterString(date_buf, 120, 120);

    /* Temp (ASCII) and condition (Chinese) are drawn as two separate
       strings sized to visually match, then centered as a pair -
       drawing them as one mixed string left the ASCII digits looking
       smaller than the CJK glyphs even at the same nominal font size,
       since this bitmap CJK font's Latin/digit glyphs don't fill the
       same visual weight as its full-width Chinese characters. Reset
       back to the default font afterward so it doesn't leak into the
       carousel's own tag rendering. */
    if (_data.weather_ok)
    {
        /* Font0 (this project's default bitmap font) is ASCII-only and
           has no glyph for the degree symbol (0xB0) - it rendered as a
           missing-glyph box. Draw the degree mark as a small circle
           instead of relying on any font's glyph coverage. */
        const std::string& temp_only = _data.weather_temp_c;
        const int degree_gap = 6;
        const int degree_r = 3;
        const int degree_slot_w = degree_gap + degree_r * 2 + 2; /* +2 gap before "C" */

        _data.hal->canvas->setFont(&fonts::Font0);
        _data.hal->canvas->setTextSize(2);
        int temp_only_w = _data.hal->canvas->textWidth(temp_only.c_str());
        int c_w = _data.hal->canvas->textWidth("C");
        int temp_total_w = temp_only_w + degree_slot_w + c_w;

        _data.hal->canvas->setFont(&fonts::efontCN_16_b);
        _data.hal->canvas->setTextSize(1);
        int gap = 8;
        int cond_w = _data.hal->canvas->textWidth(_data.weather_condition.c_str());

        int start_x = 120 - (temp_total_w + gap + cond_w) / 2;

        _data.hal->canvas->setFont(&fonts::Font0);
        _data.hal->canvas->setTextSize(2);
        _data.hal->canvas->setTextColor(TFT_WHITE);
        _data.hal->canvas->drawString(temp_only.c_str(), start_x, 152);

        int degree_cx = start_x + temp_only_w + degree_gap + degree_r;
        int degree_cy = 152 + degree_r + 1;
        _data.hal->canvas->drawCircle(degree_cx, degree_cy, degree_r, TFT_WHITE);

        _data.hal->canvas->drawString("C", start_x + temp_only_w + degree_slot_w, 152);

        _data.hal->canvas->setFont(&fonts::efontCN_16_b);
        _data.hal->canvas->setTextSize(1);
        _data.hal->canvas->setTextColor(TFT_WHITE);
        _data.hal->canvas->drawString(_data.weather_condition.c_str(), start_x + temp_total_w + gap, 153);

        _data.hal->canvas->setFont(&fonts::Font0);
    }
    else
    {
        _data.hal->canvas->setFont(&fonts::Font0);
        _data.hal->canvas->setTextSize(2);
        _data.hal->canvas->drawCenterString("--", 120, 152);
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
            /* If the backlight was off, turning it back on IS the wake
               gesture - don't also let it act as an icon tap/rotation on
               the home carousel underneath, same "wake, don't act"
               convention as IDLE_SCREEN. */
            if (_data.screen_off)
            {
                _data.hal->display.setBrightness(SCREENSAVER_ON_BRIGHTNESS);
                _data.screen_off = false;
            }

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

        /* Backlight off after SCREENSAVER_SCREEN_OFF_MS of no activity
           since the screensaver came up - nothing to render once it's
           off, so skip the once-a-second refresh below too. */
        if (!_data.screen_off &&
            millis() - _data.screensaver_started_ms > SCREENSAVER_SCREEN_OFF_MS)
        {
            _data.hal->display.setBrightness(0);
            _data.screen_off = true;
            return;
        }

        if (_data.screen_off)
        {
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
        _data.screen_off = false;
        _fetch_weather();
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
            app_ptr = new MOONCAKE::USER_APP::VideoShit;
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
        _simple_app_manager(app_ptr);
        
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


void Launcher::_simple_app_manager(MOONCAKE::APP_BASE* app)
{
    app->setUserData((void*)_data.hal);
    app->onSetup();
    app->onCreate();
    while (1)
    {
        if (!IDLE_SCREEN::tick(_data.hal))
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
}


void Launcher::onRunning()
{
    _screensaver_tick();
    if (!_data.screensaver_on)
    {
        _launcher_loop();
    }
}

