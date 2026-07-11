/**
 * @file app_timer.cpp
 */
#include "app_timer.h"
#include "../common_define.h"


using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::TIMER;


void AppTimer::onSetup()
{
    setAppName("AppTimer");
    setAllowBgRunning(false);

    TIMER::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void AppTimer::onCreate()
{
    _log("onCreate");
    _render();
}


std::string AppTimer::_pill_label()
{
    switch (_data.state)
    {
        case State::EDIT:    return "START";
        case State::RUNNING: return "PAUSE";
        case State::PAUSED:  return "RESUME";
        case State::DONE:    return "RESET";
    }
    return "START";
}


void AppTimer::_handle_encoder()
{
    if (_data.state != State::EDIT && _data.state != State::PAUSED)
        return;

    if (!_data.hal->encoder.wasMoved(true))
        return;

    int step = (_data.hal->encoder.getDirection() < 1) ? 1 : -1;

    if (_data.selected_field == Field::MINUTES)
    {
        _data.set_minutes += step;
        if (_data.set_minutes < 0)  _data.set_minutes = 99;
        if (_data.set_minutes > 99) _data.set_minutes = 0;
    }
    else
    {
        _data.set_seconds += step;
        if (_data.set_seconds < 0)  _data.set_seconds = 59;
        if (_data.set_seconds > 59) _data.set_seconds = 0;
    }

    _render();
}


void AppTimer::_handle_touch()
{
    if (!_data.hal->tp.isTouched())
        return;

    _data.hal->tp.update();
    int x = _data.hal->tp.getTouchPointBuffer().x;
    int y = _data.hal->tp.getTouchPointBuffer().y;

    /* Digit row tap: select field (only while editable) */
    if ((_data.state == State::EDIT || _data.state == State::PAUSED) &&
        y >= 75 && y <= 115)
    {
        _data.selected_field = (x < 120) ? Field::MINUTES : Field::SECONDS;
        _render();
    }

    /* Pill button tap */
    if (x >= 70 && x <= 170 && y >= 175 && y <= 205)
    {
        switch (_data.state)
        {
            case State::EDIT:
                _data.remaining_seconds = _data.set_minutes * 60 + _data.set_seconds;
                if (_data.remaining_seconds > 0)
                {
                    _data.state = State::RUNNING;
                    _data.last_tick_ms = millis();
                }
                break;

            case State::RUNNING:
                _data.state = State::PAUSED;
                _data.set_minutes = _data.remaining_seconds / 60;
                _data.set_seconds = _data.remaining_seconds % 60;
                break;

            case State::PAUSED:
                _data.remaining_seconds = _data.set_minutes * 60 + _data.set_seconds;
                _data.state = State::RUNNING;
                _data.last_tick_ms = millis();
                break;

            case State::DONE:
                _data.state = State::EDIT;
                _data.blink_on = true;
                break;
        }
        _render();
    }

    /* Debounce: wait for release so one tap isn't read as many */
    while (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        delay(5);
    }
}


void AppTimer::_handle_tick()
{
    if (_data.state == State::RUNNING)
    {
        if (millis() - _data.last_tick_ms >= 1000)
        {
            _data.last_tick_ms += 1000;
            _data.remaining_seconds -= 1;

            if (_data.remaining_seconds <= 0)
            {
                _data.remaining_seconds = 0;
                _data.state = State::DONE;
                _data.blink_on = true;
                _data.last_blink_ms = millis();
                _data.hal->buzz.tone(4000, 200);
            }

            _render();
        }
    }
    else if (_data.state == State::DONE)
    {
        if (millis() - _data.last_blink_ms >= 500)
        {
            _data.last_blink_ms = millis();
            _data.blink_on = !_data.blink_on;
            _render();
        }
    }
}


void AppTimer::_render()
{
    int display_minutes;
    int display_seconds;

    if (_data.state == State::RUNNING || _data.state == State::DONE)
    {
        display_minutes = _data.remaining_seconds / 60;
        display_seconds = _data.remaining_seconds % 60;
    }
    else
    {
        display_minutes = _data.set_minutes;
        display_seconds = _data.set_seconds;
    }

    int selected_field = -1;
    if (_data.state == State::EDIT || _data.state == State::PAUSED)
    {
        selected_field = (_data.selected_field == Field::MINUTES) ? 0 : 1;
    }

    bool show_digits = (_data.state != State::DONE) || _data.blink_on;

    _gui.renderPage(display_minutes, display_seconds, selected_field, _pill_label(), show_digits);
}


void AppTimer::onRunning()
{
    _handle_encoder();
    _handle_touch();
    _handle_tick();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}


void AppTimer::onDestroy()
{
    _log("onDestroy");
}
