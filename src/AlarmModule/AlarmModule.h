#pragma once
#include <tgbot/tgbot.h>
#include <mutex>

class AlarmModule 
{
public:
	struct ManagementCallbacks {
		std::function<bool(std::int64_t)> add;              
		std::function<bool(std::int64_t)> remove;            
		std::function<std::vector<std::int64_t>()> get_operator_list;     
	};


	explicit AlarmModule(
		const std::string& telegram_token,
		std::string passcode, 
		ManagementCallbacks management_callbacks
	);

	void start();
	void broadcast(const std::string& message);

private:
	TgBot::Bot bot_;
	std::string passcode_;
	ManagementCallbacks management_callbacks_;
	std::mutex api_mtx_;

	void send(std::int64_t chat_id, const std::string& message);
};