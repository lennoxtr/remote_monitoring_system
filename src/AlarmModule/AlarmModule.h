#pragma once
#include <tgbot/tgbot.h>

class AlarmModule 
{
public:
	explicit AlarmModule(
		const std::string& telegram_token
	);

	void start();

private:
	TgBot::Bot bot_;

	void send_alert();
	void alarm_listener_callback();
	void handle_new_operator();
	void build_message();
	void get_operator_list();
};