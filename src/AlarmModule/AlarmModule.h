#pragma once
#include "AddResult.h"
#include <tgbot/tgbot.h>
#include <mutex>
#include <chrono>
#include <thread>
#include <unordered_set>

class AlarmModule 
{
public:

	struct ManagementCallbacks {
		std::function<AddResult(std::int64_t, const std::string&)> add_operator;            // chat_id, passcode
		std::function<bool(std::int64_t, const std::string&)> remove_operator;              // chat_id, vessel name
		std::function<void(std::int64_t, bool)> set_mute_status;                            // chat_id, muted
		std::function<bool(std::int64_t)> is_muted;                                       // chat_id
		std::function<std::vector<std::string>(std::int64_t)> get_subscribed_vessel_list;              // which vessels an operator subscribes to
		std::function<std::vector<std::int64_t>(const std::string&)> get_operator_list;     // who should receive alerts of that vessel
	};

	explicit AlarmModule(
		const std::string& telegram_token,
		ManagementCallbacks management_callbacks
	);

	void start();
	void broadcast(const std::string& message, const std::string& vessel_name);

private:
	TgBot::Bot bot_;
	std::unordered_set<std::int64_t> awaiting_passcode_;
	std::unordered_set<std::int64_t> awaiting_mute_confirmation_;
	std::unordered_set<std::int64_t> awaiting_unsubscribe_confirmation_;
	ManagementCallbacks management_callbacks_;
	std::mutex api_mtx_;
	std::chrono::steady_clock::time_point next_send_{};
	static constexpr std::chrono::milliseconds send_interval_{ 50 };

	void send(std::int64_t chat_id, const std::string& message);
	void clear_pending(std::int64_t chat_id);
};