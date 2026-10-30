#pragma once
#include "Common.h"
#include <tgbot/tgbot.h>
#include <tgbot/net/CurlHttpClient.h>
#include <mutex>
#include <chrono>
#include <thread>
#include <unordered_set>
#include <unordered_map>
#include <functional>
#include <vector>

enum class PendingItem {
	PasscodeVessel,
	ConfirmMute,
	VesselToUnsubscribe,
	PasscodeAdminBroadcast,
	VesselToBroadcast,
	BroadcastText,
	VesselToPresea,
	VesselToShowStatus
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

	struct MonitoringCallbacks {
		std::function<VesselStatus(const std::string&)> get_vessel_status;
		std::function<PreseaStatus(const std::string&)> set_alarm_threshold;
	};

	explicit AlarmModule(
		const std::string& telegram_token,
		const std::string& admin_broadcast_password,
		ManagementCallbacks management_callbacks,
		MonitoringCallbacks monitoring_callbacks
	);

	void start();
	void broadcast(const std::string& message, const std::string& vessel_name);

private:
	std::string admin_broadcast_password_;

	TgBot::CurlHttpClient curlHttpClient_;
	TgBot::Bot bot_;

	ManagementCallbacks management_callbacks_;
	MonitoringCallbacks monitoring_callbacks_;
	std::unordered_map<std::int64_t, PendingState> pending_;
	
	std::mutex api_mtx_;

	void register_commands();
	void handle_pending_reply(const TgBot::Message::Ptr& m, const PendingState& state);

	void send(std::int64_t chat_id, const std::string& message);
	void clear_pending(std::int64_t chat_id);
};