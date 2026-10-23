#pragma once
#include "AddResult.h"
#include <tgbot/tgbot.h>
#include <tgbot/net/CurlHttpClient.h>
#include <mutex>
#include <chrono>
#include <thread>
#include <unordered_set>
#include <unordered_map>

class AlarmModule 
{
public:

	struct ManagementCallbacks {
		std::function<AddResult(std::int64_t, const std::string&)> add_operator;            
		std::function<bool(std::int64_t, const std::string&)> remove_operator;              
		std::function<void(std::int64_t, bool)> set_mute_status;                            
		std::function<bool(std::int64_t)> is_muted;                                       
		std::function<std::vector<std::string>(std::int64_t)> get_subscribed_vessel_list;              
		std::function<std::vector<std::int64_t>(const std::string&)> get_operator_list;
		std::function<std::vector<std::string>()> get_all_vessels;
		std::function<std::vector<std::int64_t>()> get_all_operators;
	};

	explicit AlarmModule(
		const std::string& telegram_token,
		const std::string& admin_broadcast_password,
		ManagementCallbacks management_callbacks
	);

	void start();
	void broadcast(const std::string& message, const std::string& vessel_name);

private:
	TgBot::CurlHttpClient curlHttpClient_;
	std::string admin_broadcast_password_;
	TgBot::Bot bot_;
	std::unordered_set<std::int64_t> awaiting_passcode_;
	std::unordered_set<std::int64_t> awaiting_mute_confirmation_;
	std::unordered_set<std::int64_t> awaiting_unsubscribe_confirmation_;
	std::unordered_set<std::int64_t> awaiting_admin_broadcast_passcode_;
	std::unordered_set<std::int64_t> awaiting_admin_broadcast_vessel_;
	std::unordered_map<std::int64_t, std::string> awaiting_broadcast_text_;

	ManagementCallbacks management_callbacks_;
	std::mutex api_mtx_;
	std::chrono::steady_clock::time_point next_send_{};
	static constexpr std::chrono::milliseconds send_interval_{ 50 };

	void send(std::int64_t chat_id, const std::string& message);
	void clear_pending(std::int64_t chat_id);
};