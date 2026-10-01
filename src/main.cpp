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
    
    // do a config file to read in ip and port
    std::string ip = "10.0.0.8";
    int port = 4004;
    std::string passcode = "Password123!";

    

    ManagementModule management_module;

    AlarmModule alarm_module(telegram_token, passcode, {
        .add = [&](std::int64_t id) { return management_module.add_operator(id); },
        .remove = [&](std::int64_t id) { return management_module.remove_operator(id); },
        .get_operator_list = [&]() { return management_module.get_operators(); },
    });

    std::function<void(const std::string&)> alarm_callback = [&](const std::string& message) { return alarm_module.broadcast(message); };

    MonitoringModule monitoring_module(ip, port, alarm_callback);

    std::thread monitor_thread([&monitoring_module] {
        monitoring_module.start();
        });

    alarm_module.start();   
    monitor_thread.join();
}