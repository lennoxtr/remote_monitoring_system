#include "MonitoringModule.h"
#include "ManagementModule.h"
#include "AlarmModule.h"
#include <iostream>
#include <cstdlib>
#include <string>
#include <thread>
#include <chrono>
#include <functional>
#include <vector>
#include <map>
#include <memory>

int main()
{   
    
    const char* token_env = std::getenv("TELEGRAM_BOT_TOKEN");
    const char* broadcast_pwd_env = std::getenv("ADMIN_BROADCAST_PWD");

    if (token_env == nullptr) {
        std::cerr << "CRITICAL SECURITY ERROR: TELEGRAM_BOT_TOKEN environment variable is not set!" << std::endl;
        return 1;
    }

    if (broadcast_pwd_env == nullptr) {
        std::cerr << "CRITICAL SECURITY ERROR: ADMIN_BROADCAST_PWD environment variable is not set!" << std::endl;
        return 1;
    }

    std::string telegram_token(token_env);
    std::string admin_broadcast_password(broadcast_pwd_env);

    ManagementModule management_module;

    std::map<std::string, std::unique_ptr<MonitoringModule>> monitoring_modules;

    AlarmModule alarm_module(telegram_token, admin_broadcast_password,
        {
        .add_operator = [&](std::int64_t id, const std::string& passcode) { return management_module.add_operator(id, passcode); },
        .remove_operator = [&](std::int64_t id, const std::string& vessel_name) { return management_module.remove_operator(id, vessel_name); },
        .set_mute_status = [&](std::int64_t id, bool muted) { management_module.set_mute_status(id, muted); },
        .is_muted = [&](std::int64_t id) { return management_module.is_muted(id); },
        .get_subscribed_vessel_list = [&](std::int64_t id) { return management_module.get_subscribed_vessel_list(id); },
        .get_operator_list = [&](const std::string& vessel_name) { return management_module.get_operator_list(vessel_name); },
        .get_all_vessels = [&]() {
            std::vector<std::string> vessel_names;
            for (const auto& vessel : management_module.get_all_vessels()) {
                vessel_names.push_back(vessel.name);
            }
            return vessel_names;
        },
        .get_all_operators = [&]() { return management_module.get_all_operators(); }
        },
        {
        .get_vessel_status = [&](const std::string& vessel_name) {
            return monitoring_modules.at(vessel_name)->get_vessel_status();
        },

        .set_alarm_threshold = [&](const std::string& vessel_name) {
            return monitoring_modules.at(vessel_name)->set_alarm_threshold();
        }
        });

    std::chrono::seconds watchdog_timeout_freq(60);
    std::chrono::seconds broadcast_freq(60);
    
    for (const auto& vessel : management_module.get_all_vessels()) {
        std::function<void(const std::string&)> alert_callback =
            [&alarm_module, name = vessel.name](const std::string& message) {
                alarm_module.broadcast(message, name);
            };

        monitoring_modules.emplace(
            vessel.name,
            std::make_unique<MonitoringModule>(
                vessel.name,
                vessel.target_ip,
                vessel.target_port,
                watchdog_timeout_freq,
                broadcast_freq,
                alert_callback));
    }

    std::vector<std::thread> monitor_threads;
    for (auto& [name, module] : monitoring_modules) {
        monitor_threads.emplace_back(&MonitoringModule::start, module.get());
    }


    alarm_module.start(); 

    for (auto& monitor_thread : monitor_threads) {
        monitor_thread.join();
    }
}