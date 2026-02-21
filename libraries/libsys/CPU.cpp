/*
	This file is part of nusaOS.

	nusaOS is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	nusaOS is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with nusaOS.  If not, see <https://www.gnu.org/licenses/>.

	Copyright (c) Byteduck 2016-2021. All rights reserved.
*/

#include "CPU.h"
#include <libnusa/Config.h>
#include <libnusa/FileStream.h>

using namespace Sys;
using Duck::Result, Duck::ResultRet;

ResultRet<CPU::Info> CPU::get_info(Duck::InputStream& file) {
	file.seek(0, Duck::SET);

	auto cfg_res = Duck::Config::read_from(file);
	if(cfg_res.is_error())
		return cfg_res.result();

	auto& cfg = cfg_res.value();
	if(!cfg.has_section("cpu"))
		return Result::FAILURE;

	return CPU::Info {std::stod(cfg["cpu"]["util"])};
}

ResultRet<CPU::Info> CPU::get_info() {
	auto cpu_file = Duck::File::open("/proc/cpuinfo", "r");
	if(cpu_file.is_error())
		return cpu_file.result();

	auto cpu_stream = Duck::FileInputStream(cpu_file.value());
	return get_info(cpu_stream);
}