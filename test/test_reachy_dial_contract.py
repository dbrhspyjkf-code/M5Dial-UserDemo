import pathlib
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class ReachyDialContractTests(unittest.TestCase):
    def test_launcher_replaces_ac_slot_with_reachy(self):
        render = (ROOT / "main/apps/launcher/launcher_render_callback.hpp").read_text()
        launcher = (ROOT / "main/apps/launcher/launcher.cpp").read_text()
        header = (ROOT / "main/apps/launcher/launcher.h").read_text()

        self.assertIn('"REACHY", ""', render)
        self.assertIn("#include \"../app_reachy/app_reachy.h\"", header)
        self.assertIn("case 6:", launcher)
        self.assertIn("new MOONCAKE::USER_APP::AppReachy", launcher)

    def test_reachy_app_fetches_recent_chat_markers(self):
        client = (ROOT / "main/apps/utilities/reachy_client/reachy_client.cpp").read_text()
        app = (ROOT / "main/apps/app_reachy/app_reachy.cpp").read_text()

        self.assertIn("/api/logs?filter=chat&limit=", client)
        for marker in ("xz stt:", "xz tts text:", "qwen stt:", "qwen response:"):
            self.assertIn(marker, client)
        self.assertIn("fetch_recent_turn", app)

    def test_reachy_manager_disables_idle_screen_only_for_reachy(self):
        launcher = (ROOT / "main/apps/launcher/launcher.cpp").read_text()
        self.assertIn("_simple_app_manager(app_ptr, selectedNum == 6)", launcher)
        manager = launcher.split("void Launcher::_simple_app_manager", 1)[1]
        self.assertIn("suppress_idle_screen", manager)
        self.assertIn("if (suppress_idle_screen || !IDLE_SCREEN::tick(_data.hal))", manager)

    def test_reachy_page_layout_keeps_ai_text_clear_of_bottom(self):
        gui = (ROOT / "main/apps/app_reachy/gui/gui_reachy.cpp").read_text()
        chat = gui.split("void GUI_Reachy::renderChat", 1)[1].split("void GUI_Reachy::renderAudio", 1)[0]
        self.assertNotIn("drawCenterString(status.c_str()", chat)
        self.assertNotIn("_draw_page_hint", chat)
        self.assertIn('_canvas->drawString("AI", 36, 128);', gui)
        self.assertIn("y = 145;", gui)
        self.assertIn("y += 20;", gui)

    def test_reachy_other_pages_use_chat_body_font(self):
        gui = (ROOT / "main/apps/app_reachy/gui/gui_reachy.cpp").read_text()
        self.assertIn("canvas->setFont(GUI_FONT_CN_SMALL);", gui)

        audio = gui.split("void GUI_Reachy::renderAudio", 1)[1].split("void GUI_Reachy::renderSystem", 1)[0]
        system = gui.split("void GUI_Reachy::renderSystem", 1)[1].split("void GUI_Reachy::renderMode", 1)[0]
        self.assertNotIn("setFont(&fonts::Font0);", audio)
        self.assertNotIn("setFont(&fonts::Font0);", system)
        self.assertIn("_draw_row(_canvas, 58", audio)
        self.assertIn("_draw_row(_canvas, 148", audio)
        self.assertIn("_draw_row(_canvas, 58", system)
        self.assertIn("_draw_row(_canvas, 148", system)

    def test_reachy_icon_generated_from_user_image(self):
        icon = (ROOT / "main/apps/launcher/launcher_icons/icon_reachy.h").read_text()
        self.assertIn("Simplified Reachy icon", icon)

    def test_reachy_refreshes_with_low_latency(self):
        app = (ROOT / "main/apps/app_reachy/app_reachy.cpp").read_text()
        self.assertIn("static const uint32_t POLL_MS = 800;", app)

    def test_reachy_encoder_controls_volume_without_page_switching(self):
        app = (ROOT / "main/apps/app_reachy/app_reachy.cpp").read_text()
        handler = app.split("void AppReachy::_handle_encoder()", 1)[1].split("void AppReachy::_handle_touch()", 1)[0]
        self.assertIn("if (_data.page == REACHY::CHAT)", handler)
        self.assertIn("_ensure_mic_enabled();", handler)
        self.assertIn("_adjust_volume(direction);", handler)
        self.assertNotIn("_next_page(direction);", handler)
        self.assertIn("void AppReachy::_adjust_volume(int direction)", app)
        self.assertIn("void AppReachy::_ensure_mic_enabled()", app)

    def test_reachy_progress_ring_can_fill_to_100_percent(self):
        gui = (ROOT / "main/apps/app_reachy/gui/gui_reachy.cpp").read_text()
        progress = gui.split("static void _draw_edge_progress", 1)[1].split("void GUI_Reachy::renderChat", 1)[0]
        self.assertIn("360 * percent", progress)
        self.assertIn("if (percent >= 100)", progress)

    def test_reachy_has_multi_page_audio_system_mode_controls(self):
        header = (ROOT / "main/apps/app_reachy/app_reachy.h").read_text()
        app = (ROOT / "main/apps/app_reachy/app_reachy.cpp").read_text()
        gui = (ROOT / "main/apps/app_reachy/gui/gui_reachy.h").read_text()
        client = (ROOT / "main/apps/utilities/reachy_client/reachy_client.cpp").read_text()

        for page in ("CHAT", "AUDIO", "MODE", "SYSTEM"):
            self.assertIn(page, header)
        enum_body = header.split("enum Page", 1)[1].split("};", 1)[0]
        self.assertNotIn("VIDEO", enum_body)
        self.assertLess(enum_body.index("CHAT"), enum_body.index("AUDIO"))
        self.assertLess(enum_body.index("AUDIO"), enum_body.index("MODE"))
        self.assertLess(enum_body.index("MODE"), enum_body.index("SYSTEM"))
        self.assertIn("_handle_touch", app)
        self.assertIn("_handle_encoder", app)
        self.assertIn("renderAudio", gui)
        self.assertIn("renderSystem", gui)
        for endpoint in (
            "/api/volume",
            "/api/audio/input",
            "/api/audio/vad",
            "/api/system/restart",
        ):
            self.assertIn(endpoint, client)
        self.assertIn("HTTP_METHOD_PUT", client)
        self.assertIn("HTTP_METHOD_POST", client)

    def test_reachy_mode_page_controls_backend_voice_and_restart_confirm(self):
        header = (ROOT / "main/apps/app_reachy/app_reachy.h").read_text()
        app = (ROOT / "main/apps/app_reachy/app_reachy.cpp").read_text()
        gui_h = (ROOT / "main/apps/app_reachy/gui/gui_reachy.h").read_text()
        gui = (ROOT / "main/apps/app_reachy/gui/gui_reachy.cpp").read_text()
        client_h = (ROOT / "main/apps/utilities/reachy_client/reachy_client.h").read_text()
        client = (ROOT / "main/apps/utilities/reachy_client/reachy_client.cpp").read_text()

        self.assertIn("struct ModeState", client_h)
        self.assertIn("fetch_mode_state", client_h)
        self.assertIn("set_backend", client_h)
        self.assertIn("set_voice", client_h)
        self.assertIn("/api/conversation/backend", client)
        self.assertIn("/api/conversation/voice", client)
        self.assertIn('\\"backend\\"', client)
        self.assertIn('\\"voice\\"', client)

        self.assertIn("MODE_EDIT_BACKEND", header)
        self.assertIn("MODE_EDIT_VOICE", header)
        self.assertIn("restart_confirm", header)
        self.assertIn("renderMode", gui_h)
        self.assertIn("renderMode", gui)
        self.assertIn("QWEN", gui)
        self.assertIn("XIAOZHI", gui)
        self.assertIn("Disable", gui)
        self.assertIn("Restart now?", gui)
        self.assertIn("_fetch_mode", app)
        self.assertIn("_apply_mode", app)
        self.assertIn("set_backend", app)
        self.assertIn("set_voice", app)
        self.assertIn("restart_yrobot", app)
        self.assertIn("_cycle_voice", app)

    def test_launcher_defaults_to_reachy_after_first_menu_update(self):
        launcher = (ROOT / "main/apps/launcher/launcher.cpp").read_text()
        simple_menu = (ROOT / "main/apps/utilities/smooth_menu/src/simple_menu/simple_menu.cpp").read_text()

        self.assertIn("DEFAULT_SELECTED_APP = 6", launcher)
        self.assertIn("_data.menu->goToItem(DEFAULT_SELECTED_APP)", launcher)
        self.assertNotIn("_selector->goToItem(0);", simple_menu)

    def test_reachy_video_page_is_removed(self):
        header = (ROOT / "main/apps/app_reachy/app_reachy.h").read_text()
        app = (ROOT / "main/apps/app_reachy/app_reachy.cpp").read_text()
        gui = (ROOT / "main/apps/app_reachy/gui/gui_reachy.cpp").read_text()
        gui_h = (ROOT / "main/apps/app_reachy/gui/gui_reachy.h").read_text()
        client_h = (ROOT / "main/apps/utilities/reachy_client/reachy_client.h").read_text()
        client = (ROOT / "main/apps/utilities/reachy_client/reachy_client.cpp").read_text()

        for removed in ("VIDEO", "_fetch_video", "_fetch_frame", "_prepare_frame",
                        "_toggle_video", "last_video_fetch_ms", "last_frame_fetch_ms",
                        "renderVideo", "VideoState", "JpegFrame", "fetch_camera_jpeg",
                        "prepare_camera_jpeg_buffer", "set_video_enabled",
                        "/api/conversation/video", "/api/camera/frame"):
            self.assertNotIn(removed, header)
            self.assertNotIn(removed, app)
            self.assertNotIn(removed, gui)
            self.assertNotIn(removed, gui_h)
            self.assertNotIn(removed, client_h)
            self.assertNotIn(removed, client)


if __name__ == "__main__":
    unittest.main()
