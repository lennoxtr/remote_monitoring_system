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

enum class DeviceType { Unknown, Battery, FloatSwitch };
enum class DeviceState { Unknown, Ok, Unreadable, Alarm };


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

struct BatteryCondition {
	std::string name;
	DeviceState status = DeviceState::Unknown;
	std::optional<float> soc_pct;
	std::optional<std::string> time_to_go;
};

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

private:
	AlertCallback alert_callback_;

	std::string name_;
	std::string ip_;
	std::string previous_msg_type_ = "healthcheck";

	int port_;
	int client_socket_ = -1;
	std::string rx_buffer_;
	std::chrono::seconds watchdog_timeout_freq_;

	std::chrono::seconds broadcast_freq_;
	std::chrono::steady_clock::time_point next_broadcast_time_;
	std::set<std::string> last_alarm_keys_;

	Watchdog watchdog_;
	bool initialize_socket();
	bool get_message_once(std::string& out_message);
	int process_messages(const std::string& message);
	
	std::vector<BatteryCondition> batteries_status_list_;

	std::string build_battery_alarm(const nlohmann::json& alarm_info, std::set<std::string>& alarm_keys);
	std::string build_floatswitch_alarm(const nlohmann::json& alarm_info, std::set<std::string>& alarm_keys);

	std::string strip_prefix(const std::string& name, const std::string& prefix);
	std::string format_elapsed(int seconds);
	std::vector<BatteryCondition> parse_batteries(const nlohmann::json& alarm_info);
};