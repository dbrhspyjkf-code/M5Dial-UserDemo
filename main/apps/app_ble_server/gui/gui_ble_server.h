/**
 * @file gui_ble_server.h
 * @brief Renderer for the unread-email viewer app. Pure display —
 * takes primitive values, draws them. No state or network calls here.
 */
#pragma once
#include "../../utilities/gui_base/gui_base.h"
#include <string>


class GUI_BLE_Server : public GUI_Base
{
    public:
        void init() override;

        /**
         * @brief Render the loading/error screen
         */
        void renderStatus(const std::string& line1, const std::string& line2);

        /**
         * @brief Render one folder's unread mail summary
         *
         * @param folderName mailbox/folder name (e.g. "INBOX", may be Chinese)
         * @param unreadCount unread message count in this folder
         * @param sender latest unread message's sender
         * @param subject latest unread message's subject
         * @param index 0-based position in the list
         * @param count total number of folders with unread mail
         */
        void renderPage(const std::string& folderName, int unreadCount,
                         const std::string& sender, const std::string& subject,
                         int index, int count);
};
