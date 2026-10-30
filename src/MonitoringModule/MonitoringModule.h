#pragma once
#include <string>
#include <chrono>
#include <mutex>
#include <set>
#include <algorithm>
#include <condition_variable>
#include <functional>
#include <thread>
#include <nlohmann/json.hpp>
#include <optional>
#include <vector>

#include "Common.h"

NLOHMANN_JSON_SERIALIZE_ENUM(DeviceType, {
	{DeviceType::Unknown, nullptr},
	{DeviceType::Battery, "battery"},
	{DeviceType::FloatSwitch, "floatswitch"},
})

NLOHMANN_JSON_SERIALIZE_ENUM(DeviceState, {
	{DeviceState::Unknown, nullptr},
	{DeviceState::Ok, "ok"},
	{DeviceState::Unreadable, "unreadable"},
	{DeviceState::Alarm, "alarm"},
})

class Watchdog {
public:
	using AlertCallback = std::function<void(const std::string&)>;

	Watchdog(
		const std::string& usv_name_,
		std::chrono::seconds watchdog_timeout_freq_,
		std::chrono::seconds broadcast_freq_,
		AlertCallback alert_callback
	);
	~Watchdog();

	bool is_link_up() const {
		std::lock_guard<std::mutex> lock(mtx_);
		return !timed_out_;
	}

	void reset();
private:

	void monitor();
	std::string usv_name_;
	std::chrono::seconds watchdog_timeout_freq_;
	std::chrono::seconds broadcast_freq_;
	std::chrono::steady_clock::time_point last_reset_time_;
	std::chrono::steady_clock::time_point next_alert_time_;

	std::mutex mtx_;
	std::condition_variable cv_;
	bool running_ = true;
	bool timed_out_ = false;

	AlertCallback alert_callback_;

	std::thread watcher_thread_;

};

class MonitoringModule {
public:

	using AlertCallback = std::function<void(const std::string&)>;

	MonitoringModule(
		const std::string& name,
		const std::string& ip,
		int port,
		std::chrono::seconds watchdog_timeout_freq_,
		std::chrono::seconds broadcast_freq_,
		AlertCallback alert_callback
	);
	~MonitoringModule();

	void start();
	void end();

	VesselStatus get_vessel_status() const {
		const bool link_up = watchdog_.is_link_up();
		std::lock_guard<std::mutex> lock(mtx_);
		VesselStatus vessel_status;
		vessel_status.link_up = link_up;
		vessel_status.has_data = has_data_;
		vessel_status.batteries_status_list = batteries_status_list_;
		vessel_status.floatswitch_status_list = floatswitch_status_list_;
		return vessel_status; 
	};

	PreseaStatus set_alarm_threshold();

private:
	AlertCallback alert_callback_;

	mutable std::mutex mtx_;

	std::string name_;
	std::string ip_;
	int port_;
	int client_socket_ = -1;

	std::string rx_buffer_;

	std::chrono::seconds watchdog_timeout_freq_;


	std::chrono::seconds broadcast_freq_;
	std::chrono::steady_clock::time_point next_broadcast_time_;

	static constexpr double NormalAlarmThreshold_s = 60.0;
	static constexpr double PreseaAlarmThreshold_s = 4.5;
	static constexpr std::chrono::minutes PreseaDuration{ 10 };
	std::chrono::steady_clock::time_point check_mode_until_{};


	std::set<std::string> last_alarm_keys_;

	Watchdog watchdog_;
	bool initialize_socket();
	bool get_message_once(std::string& out_message);
	int process_messages(const std::string& message);
	
	double current_alarm_threshold() const {
		std::lock_guard<std::mutex> lock(mtx_);
		return std::chrono::steady_clock::now() < check_mode_until_ 
			? PreseaAlarmThreshold_s
			: NormalAlarmThreshold_s;
	}

	bool has_data_ = false;
	std::vector<FloatSwitchCondition> floatswitch_status_list_;
	std::vector<BatteryCondition> batteries_status_list_;

	std::string build_battery_alarm(const nlohmann::json& alarm_info, std::set<std::string>& alarm_keys);
	std::string build_floatswitch_alarm(const nlohmann::json& alarm_info, std::set<std::string>& alarm_keys);

	std::string strip_prefix(const std::string& name, const std::string& prefix);
	std::string format_elapsed(int seconds);
	std::vector<BatteryCondition> parse_batteries(const nlohmann::json& device_info);
	std::vector<FloatSwitchCondition> parse_floatswitches(const nlohmann::json& device_info);
};