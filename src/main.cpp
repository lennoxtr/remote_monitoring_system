#include <tgbot/tgbot.h>

#include <iostream>
#include <cstdlib>
#include <memory>

int main()
{   
    /*
    const char* token_env = std::getenv("TELEGRAM_BOT_TOKEN");

    if (token_env == nullptr) {
        std::cerr << "CRITICAL SECURITY ERROR: TELEGRAM_BOT_TOKEN environment variable is not set!" << std::endl;
        return 1;
    }

    std::string telegram_token(token_env);
    */
    // do a config file to read in ip and port
    std::string ip = "10.0.0.8";
    int port = 4004;

    //AlarmModule alarm_module = AlarmModule(telegram_token);
    MonitoringModule monitoring_module = MonitoringModule(ip, port);

    //alarm_module.start();
    monitoring_module.start();

    /*

    const char* token = "8890754207:AAEvgpVnPqhAXrF4uG_uXbId4iYBVUsCRdc";
    */
}