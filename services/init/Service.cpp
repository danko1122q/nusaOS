/*
    This file is part of nusaOS.
    Copyright (c) Byteduck 2016-2022. All rights reserved.
*/
#include "Service.h"
#include "libnusa/Log.h"
#include <libnusa/Config.h>
#include <libnusa/StringStream.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
#include <cstdlib>

Duck::ResultRet<Service> Service::load_service(Duck::Path path) {
	auto config_res = Duck::Config::read_from(path);
	if(config_res.is_error())
		return config_res.result();

	auto& config = config_res.value();
	if(!config.has_section("service"))
		return Duck::Result(-EINVAL);

	auto& service = config["service"];
	return Service(service["name"], service["exec"], service["after"]);
}

std::vector<Service> Service::get_all_services() {
	auto res = Duck::Path("/etc/init/services").get_directory_entries();
	if(res.is_error())
		return {};

	auto ret = std::vector<Service>();
	for(auto& entry : res.value()) {
		if(!entry.is_regular() || entry.path().extension() != "service")
			continue;

		auto service_res = load_service(entry.path());
		if(service_res.is_error()) {
			Duck::Log::warn("Failed to load service at ", entry.path().string(), ": ", service_res.result().strerror());
			continue;
		}

		ret.push_back(service_res.value());
	}

	return std::move(ret);
}

const std::string& Service::name() const { return m_name; }
const std::string& Service::exec() const { return m_exec; }
const std::string& Service::after() const { return m_after; }

pid_t Service::execute(char** envp) const {
	Duck::Log::info("Starting service ", m_name, "...");
	pid_t pid = fork();

	if(pid == 0) {
		// FIX Bug 2: Gunakan environment dari parent (envp dari main)
		// bukan array kosong {NULL} — service butuh PATH, HOME, dll.
		Duck::StringInputStream exec_stream(m_exec);
		exec_stream.set_delimeter(' ');

		std::vector<std::string> args;
		std::string arg;
		while(!exec_stream.eof()) {
			exec_stream >> arg;
			if(!arg.empty())
				args.push_back(arg);
		}

		if(args.empty()) {
			Duck::Log::err("Service ", m_name, " has empty exec command");
			exit(-1);
		}

		const char* c_args[args.size() + 1];
		for(size_t i = 0; i < args.size(); i++)
			c_args[i] = args[i].c_str();
		c_args[args.size()] = nullptr;

		execvpe(c_args[0], (char* const*) c_args, envp);
		Duck::Log::err("Failed to execute ", m_exec, ": ", strerror(errno));
		exit(-1);
	}

	return pid;
}

Service::Service(std::string name, std::string exec, std::string after):
	m_name(std::move(name)), m_exec(std::move(exec)), m_after(std::move(after)) {}