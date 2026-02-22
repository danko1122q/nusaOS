/*
    This file is part of nusaOS.
    Copyright (c) Byteduck 2016-2022. All rights reserved.
*/
#pragma once

#include <libnusa/Path.h>
#include <libnusa/Result.h>

class Service {
public:
    static Duck::ResultRet<Service> load_service(Duck::Path path);
    static std::vector<Service> get_all_services();

    const std::string& name() const;
    const std::string& exec() const;
    const std::string& after() const;

    // FIX: Tambah parameter envp agar service dapat environment dari parent
    // dan return pid_t agar init bisa track proses (misal menunggu Pond siap)
    pid_t execute(char** envp) const;

private:
    Service(std::string name, std::string exec, std::string after);

    std::string m_name, m_exec, m_after;
};