/**
 * @file app_more_menu.cpp
 */
#include "app_more_menu.h"
#include "../common_define.h"
#include "../utilities/stock_client/stock_client.h"


using namespace MOONCAKE::USER_APP;
using namespace MOONCAKE::USER_APP::MORE_MENU;


void MoreMenu::onSetup()
{
    setAppName("MoreMenu");
    setAllowBgRunning(false);

    MORE_MENU::Data_t default_data;
    _data = default_data;

    _data.hal = (HAL::HAL*)getUserData();
}


void MoreMenu::_fetch()
{
    _data.stocks = STOCK_CLIENT::get_portfolio(STOCK_API_BASE_URL);

    if (_data.stocks.empty())
    {
        _data.state = State::ERROR;
        _data.error_message = "No data";
        return;
    }

    if (_data.current_index >= (int)_data.stocks.size())
    {
        _data.current_index = 0;
    }
    _data.state = State::CONTROLLING;
}


void MoreMenu::onCreate()
{
    _log("onCreate");

    _data.state = State::CONNECTING;
    _render();

    _fetch();
    _render();
}


void MoreMenu::_handle_encoder()
{
    if (_data.state != State::CONTROLLING || _data.showing_analysis)
        return;

    if (!_data.hal->encoder.wasMoved(true))
        return;

    int direction = (_data.hal->encoder.getDirection() < 1) ? 1 : -1;
    int count = (int)_data.stocks.size();

    _data.current_index = (_data.current_index + direction + count) % count;

    _render();
}


void MoreMenu::_handle_touch()
{
    if (!_data.hal->tp.isTouched())
        return;

    if (_data.state == State::ERROR)
    {
        _data.state = State::CONNECTING;
        _render();
        _fetch();
        _render();
    }
    else if (_data.state == State::CONTROLLING)
    {
        _data.showing_analysis = !_data.showing_analysis;
        _render();
    }

    while (_data.hal->tp.isTouched())
    {
        _data.hal->tp.update();
        delay(5);
    }
}


void MoreMenu::_render()
{
    if (_data.state == State::CONNECTING)
    {
        _gui.renderStatus("Loading...", "");
    }
    else if (_data.state == State::ERROR)
    {
        _gui.renderStatus(_data.error_message, "TAP TO RETRY");
    }
    else
    {
        const STOCK_CLIENT::StockItem& stock = _data.stocks[_data.current_index];
        std::string display_name = stock.name.empty() ? stock.code : stock.name;

        if (_data.showing_analysis)
        {
            _gui.renderAnalysis(display_name, stock.one_sentence, stock.analysis_summary,
                                 _data.current_index, (int)_data.stocks.size());
        }
        else
        {
            _gui.renderPage(display_name, stock.price, stock.abs_chg, stock.chg,
                             _data.current_index, (int)_data.stocks.size());
        }
    }
}


void MoreMenu::onRunning()
{
    _handle_encoder();
    _handle_touch();

    /* If button pressed: quit, unconditionally (same convention as every other app) */
    if (!_data.hal->encoder.btn.read())
    {
        while (!_data.hal->encoder.btn.read())
            delay(5);

        destroyApp();
    }
}


void MoreMenu::onDestroy()
{
    _log("onDestroy");

    /* Shared canvas: leave text size back at the default so the launcher's
       tag rendering isn't left using whatever size this app last drew with. */
    _data.hal->canvas->setTextSize(1);
}
