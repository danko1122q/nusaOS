/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2026 danko1122q */

#pragma once

#include <libui/Window.h>
#include <libui/widget/MenuBar.h>
#include "HexWidget.h"
#include "StatusBar.h"

class HexEditor : public Duck::Object {
public:
	NUSA_OBJECT_DEF(HexEditor);

	void open_file(const std::string& path);

private:
	HexEditor();

	Duck::Ptr<UI::Window>   m_window;
	Duck::Ptr<HexWidget>    m_hex_widget;
	Duck::Ptr<StatusBar>    m_status_bar;
};