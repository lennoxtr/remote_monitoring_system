#pragma once
#include "AddResult.h"
#include <tgbot/tgbot.h>
#include <tgbot/net/CurlHttpClient.h>
#include <mutex>
#include <chrono>
#include <thread>
#include <unordered_set>
#include <unordered_map>

enum class PendingItem {
	PasscodeVessel,
	ConfirmMute,
	VesselToUnsubscribe,
	PasscodeAdminBroadcast,
	VesselToBroadcast,
	BroadcastText
};

struct PendingState {
	PendingItem type;
	std::string pending_data;
};


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
	std::string admin_broadcast_password_;

	TgBot::CurlHttpClient curlHttpClient_;
	TgBot::Bot bot_;

	ManagementCallbacks management_callbacks_;
	std::unordered_map<std::int64_t, PendingState> pending_;
	
	std::mutex api_mtx_;

	void send(std::int64_t chat_id, const std::string& message);
	void clear_pending(std::int64_t chat_id);
};