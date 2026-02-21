#include <csignal>
#include <cstdio>
#include <unistd.h>
#include <sys/wait.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <libnusa/Config.h>
#include <sstream>
#include <vector>

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
    
    Copyright (c) Byteduck 2016-2020. All rights reserved.
*/

// The init system for nusaOS.

#include <libnusa/Log.h>
#include "Service.h"

using Duck::Log, Duck::Config;

int main(int argc, char** argv, char** envp) {
	if(getpid() != 1) {
		printf("pid != 1. Exiting.\n");
		return -1;
	}

	setsid();
	Log::success("Welcome to nusaOS!");

	//Load services
	auto services = Service::get_all_services();

	//Start boot services
	for(auto& service : services) {
		if(service.after() == "boot")
			service.execute();
	}

	//Wait for all child processes
	while(1) {
		pid_t pid = waitpid(-1, NULL, 0);
		if(pid < 0 && errno == ECHILD) break; //All child processes exited
	}

	Log::info("All child processes exited. Goodbye!");

	return 0;
}