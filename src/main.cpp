#include "MonitoringModule.h"
#include "ManagementModule.h"
#include "AlarmModule.h"
#include <iostream>
#include <cstdlib>
#include <memory>
#include <string>
#include <thread>

int main()
{   
    
    const char* token_env = std::getenv("TELEGRAM_BOT_TOKEN");

    if (token_env == nullptr) {
        std::cerr << "CRITICAL SECURITY ERROR: TELEGRAM_BOT_TOKEN environment variable is not set!" << std::endl;
        return 1;
    }

    std::string telegram_token(token_env);

    ManagementModule management_module;

    AlarmModule alarm_module(telegram_token, {
        .add_operator = [&](std::int64_t id) { return management_module.add_operator(id); },
        .remove_operator = [&](std::int64_t id) { return management_module.remove_operator(id); },
        .get_vessel_list = [&]() { return management_module.get_vessel_list(); },
        .get_operator_list = [&]() { return management_module.get_operator_list(); },
    });

    std::chrono::seconds watchdog_timeout_freq_(60);
    std::chrono::seconds broadcast_freq_(60);

    std::vector<std::thread> monitor_threads;    
    for (const auto& vessel : management_module.get_all_vessels()) {
        monitor_threads.emplace_back([&, vessel] {
            std::function<void(const std::string&)> alarm_callback =
                [&alarm_module, name = vessel.name](const std::string& message) {
                alarm_module.broadcast(message, name);
                };

            MonitoringModule monitoring_module(
                vessel.target_ip, vessel.target_port,
                watchdog_timeout_freq_, broadcast_freq_, alarm_callback);

            monitoring_module.start();
            });
    }

    alarm_module.start(); 

    for (auto& monitor_thread : monitor_threads) {
        monitor_thread.join();
    }
}