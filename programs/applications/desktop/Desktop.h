/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright © 2025-2026 danko1122q */

#pragma once

#include <libui/Window.h>
#include "DesktopWidget.h"

class Desktop: public Duck::Object {
public:
	NUSA_OBJECT_DEF(Desktop);

private:
	Desktop();

	Duck::Ptr<UI::Window> m_window;
	Duck::Ptr<DesktopWidget> m_widget;
};