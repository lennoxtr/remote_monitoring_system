#include "MonitoringModule.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>
#include <cctype>
#include <set>
#include <algorithm>
#include <string>
#include <cmath>

using json = nlohmann::json;

Watchdog::Watchdog(
	const std::string& usv_name,
	std::chrono::seconds watchdog_timeout_freq,
	std::chrono::seconds broadcast_freq,
	AlertCallback alert_callback
	)
	: usv_name_(usv_name),
	watchdog_timeout_freq_(watchdog_timeout_freq),
	broadcast_freq_(broadcast_freq),
	last_reset_time_(std::chrono::steady_clock::now()),
	next_alert_time_(last_reset_time_),
	watcher_thread_(&Watchdog::monitor, this),
	alert_callback_(alert_callback)
{
	if (!alert_callback_) {
		std::cerr << "No alert callback" << std::endl;
	}
}

Watchdog::~Watchdog()
{
	{
		std::lock_guard<std::mutex> lock(mtx_);
		running_ = false;
	}
	cv_.notify_all();          
	if (watcher_thread_.joinable()) {
		watcher_thread_.join();
	}
}

void Watchdog::reset()
{	
	bool was_timed_out;
	{
		std::lock_guard<std::mutex> lock(mtx_);
		last_reset_time_ = std::chrono::steady_clock::now();
		next_alert_time_ = last_reset_time_ + watchdog_timeout_freq_;
		was_timed_out = timed_out_;
		timed_out_ = false;
	}
	if (was_timed_out) {
		// Handle restore msg if have
		std::string restore_msg = "✅ Link to " + usv_name_ + " restored✅";
		alert_callback_(restore_msg);
	}
}

void Watchdog::monitor()
{	
	std::unique_lock<std::mutex> lock(mtx_);
	while (running_) {
		auto now = std::chrono::steady_clock::now();

		if (now >= next_alert_time_) {
			timed_out_ = true;
			next_alert_time_ = now + broadcast_freq_;
			lock.unlock();
			
			std::string lost_msg = "❗Link to " + usv_name_ + " lost❗";
			alert_callback_(lost_msg);
			        
			lock.lock();
			continue;
		}
		cv_.wait_until(lock, next_alert_time_, [this] { return !running_; });
	}
}

MonitoringModule::MonitoringModule(
	const std::string& name,
	const std::string& ip,
	int port,
	std::chrono::seconds watchdog_timeout_freq,
	std::chrono::seconds broadcast_freq,
	AlertCallback alert_callback
)
	: name_(name),
	ip_(ip),
	port_(port),
	alert_callback_(alert_callback),
	watchdog_(
		name,
		watchdog_timeout_freq,
		broadcast_freq,
		alert_callback
	),
	watchdog_timeout_freq_(watchdog_timeout_freq),
	broadcast_freq_(broadcast_freq),
	next_broadcast_time_(std::chrono::steady_clock::now())
	
{
}

MonitoringModule::~MonitoringModule()
{
	end();
}

bool MonitoringModule::initialize_socket()
{
	client_socket_ = socket(AF_INET, SOCK_STREAM, 0);

	if (client_socket_ == -1) {
		std::cerr << "Error: Failed to create socket.\n";
		return false;
	}
	timeval timeout{ 15, 0 };
	setsockopt(client_socket_, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

	sockaddr_in serverAddress;
	std::memset(&serverAddress, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(port_);

	if (inet_pton(AF_INET, ip_.c_str(), &serverAddress.sin_addr) <= 0) {
		std::cerr << "Error: Invalid address.\n";
		end();
		return false;
	}

	if (connect(client_socket_, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
		std::cerr << "Error: Connection failed.\n";
		end();
		return false;
	}

	rx_buffer_.clear();
	return true;
}

bool MonitoringModule::get_message_once(std::string& out_message)
{	
	size_t pos;

	while ((pos = rx_buffer_.find('\n')) == std::string::npos) {
		char buffer[1024];
		ssize_t n = recv(client_socket_, buffer, sizeof(buffer), 0);
		if (n <= 0) return false;
		rx_buffer_.append(buffer, n);
	}
	
	out_message = rx_buffer_.substr(0, pos); // clears \n
	rx_buffer_.erase(0, pos + 1);
	if (!out_message.empty() && out_message.back() == '\r') { // clear \r
		out_message.pop_back();
	}
	return true;

}

int MonitoringModule::process_messages(const std::string& message)
{
	try {
		json parsed_message = json::parse(message);
		std::string message_type = parsed_message.at("type");
		const json& payload = parsed_message.at("payload");
		if (auto it = payload.find("battery"); it != payload.end()) {
			batteries_status_list_ = parse_batteries(*it);
		}
		if (message_type == "healthcheck") {
			if (previous_msg_type_ == "alarm") {
				alert_callback_("✅ All " + name_ + "'s Alarm Resolved!!!✅\n");
			}
			
			previous_msg_type_ = message_type;
			last_alarm_keys_.clear();
			return 1;
		}

		// Alarm messages
		std::set<std::string> current_keys;
		std::string alert_broadcast;

		for (const auto& [device_type, device_info] : payload.items()) {
			switch (json(device_type).get<DeviceType>()) {

			case DeviceType::Battery:
				alert_broadcast += build_battery_alarm(device_info, current_keys);
				break;

			case DeviceType::FloatSwitch:
				alert_broadcast += build_floatswitch_alarm(device_info, current_keys);
				break;

			case DeviceType::Unknown:
				std::cerr << "Invalid Message Type!" << std::endl;
				break;
			} 
		}

		bool has_new_alarm = std::any_of(
			current_keys.begin(), current_keys.end(),
			[this](const std::string& k) { return last_alarm_keys_.count(k) == 0; });

		auto now = std::chrono::steady_clock::now();
		if (has_new_alarm || now >= next_broadcast_time_) {
			alert_callback_(alert_broadcast);
			next_broadcast_time_ = now + broadcast_freq_;
		}

		last_alarm_keys_ = std::move(current_keys);
		previous_msg_type_ = message_type;
		return 1;
	}
	catch (const std::exception& e) {
		std::cerr << "Failed to process message: " << e.what() << std::endl;
		return -1;
	}
}

void MonitoringModule::start()
{	
	watchdog_.reset();
	try {
		std::string received_msg;
		std::cout << "Monitoring Module: Running" << std::endl;
		while (true) {
			if (initialize_socket()) {
				while (get_message_once(received_msg)) {
					// reset watchdog everytime a full msg is received

					if (process_messages(received_msg) > 0) {
						watchdog_.reset();
					}
				}
				end();
				last_alarm_keys_.clear();
			}
			std::this_thread::sleep_for(std::chrono::seconds(3));
		}
	}
	catch (...) {
		std::cerr << "Monitoring stopped." << std::endl;
	}
}

std::string MonitoringModule::build_battery_alarm(const json& device_info, std::set<std::string>& alarm_keys) {
	std::string alert_broadcast;
	if (device_info.at("status").get<DeviceState>() != DeviceState::Ok) {
		alert_broadcast += "🚨" + name_ + "'s BATTERY ALARM!!!🚨\n";
	}
	std::string batt_name;
	for (const auto& batt_info : device_info.at("devices")) {
		batt_name = batt_info.at("name").get<std::string>();
		switch (json(batt_info.at("status")).get<DeviceState>()) {
		case DeviceState::Ok:
			break;
		case DeviceState::Alarm:
		{	
			alarm_keys.insert("battery:" + batt_name);
			int time_to_go = batt_info.at("timetogo_s").get<int>();
			float soc = batt_info.at("soc_pct").get<float>();
			alert_broadcast += "\n- Battery " + batt_name + " SOC is " + (soc >= 0 ? std::to_string(std::lround(soc)) + "%" : "n/a") + ", Time To Go : " + (time_to_go >= 0 ? format_elapsed(time_to_go) : "n/a");
			std::cout << batt_name << " SOC is " + (soc >= 0 ? std::to_string(std::lround(soc)) + " % " : "n/a") + ", Time To Go : " + (time_to_go >= 0 ? format_elapsed(time_to_go) : "n/a") << std::endl;
			break;
		}
		case DeviceState::Unreadable:
			alarm_keys.insert("battery:" + batt_name);
			alert_broadcast += "\n- Battery " + batt_name + " state is unreadable";
			break;
		case DeviceState::Unknown:
			std::cout << "Battery " + batt_name + " state is unknown";
			break;
		}

	}
	return alert_broadcast;
}

std::string MonitoringModule::build_floatswitch_alarm(const json& device_info, std::set<std::string>& alarm_keys) {
	std::string alert_broadcast;
	if (device_info.at("status").get<DeviceState>() != DeviceState::Ok) {
		alert_broadcast += "🚨" + name_ + "'s FLOATSWITCH ALARM!!!🚨\n";
	}
	std::string fs_name;
	for (const auto& fs_info : device_info.at("devices")) {
		fs_name = fs_info.at("name").get<std::string>();
		switch (json(fs_info.at("status")).get<DeviceState>()) {
		case DeviceState::Alarm:
		{	
			alarm_keys.insert("floatswitch:" + fs_name);
			int elapsed = fs_info.value("elapsed_s", 0);
			alert_broadcast += "\n- Float Switch " + strip_prefix(fs_name, "FL Switch")
				+ " activated for " + format_elapsed(elapsed);
			std::cout << fs_name << " in alarm for " << elapsed << " s" << std::endl;
			break;
		}
		case DeviceState::Unreadable:
			alarm_keys.insert("floatswitch:" + fs_name);
			alert_broadcast += "\n- Float Switch " + fs_name + " state is unreadable";
			break;
		case DeviceState::Ok:
			break;
		case DeviceState::Unknown:
			std::cout << "Float Switch " + fs_name + " state is unknown";
			break;
		}
		
	}
	return alert_broadcast;
}

std::vector<BatteryCondition> MonitoringModule::parse_batteries(const json& device_info) {
	std::vector<BatteryCondition> result;
	for (const auto& b : device_info.at("devices")) {
		BatteryCondition c;
		c.name = b.at("name").get<std::string>();
		c.status = b.at("status").get<DeviceState>();
		c.soc_pct = b.at("soc_pct").get<float>();
		c.time_to_go = format_elapsed(b.at("timetogo_s").get<int>());
		result.push_back(std::move(c));
	}
	return result;
}


std::string MonitoringModule::strip_prefix(const std::string& name, const std::string& prefix)
{
	size_t start = 0;
	while (start < name.size() && std::isspace(static_cast<unsigned char>(name[start]))) ++start;

	if (name.size() - start < prefix.size()) return name;
	for (size_t i = 0; i < prefix.size(); ++i) {
		if (std::tolower(static_cast<unsigned char>(name[start + i])) !=
			std::tolower(static_cast<unsigned char>(prefix[i]))) {
			return name;                       // doesn't start with the prefix
		}
	}

	start += prefix.size();
	while (start < name.size() && std::isspace(static_cast<unsigned char>(name[start]))) ++start;
	return name.substr(start);
}

std::string MonitoringModule::format_elapsed(int seconds)
{
	if (seconds < 60) {
		return std::to_string(seconds) + " s";
	}
	if (seconds < 3600) {
		return std::to_string(seconds / 60) + " min";
	}
	int hours = seconds / 3600;
	int minutes = (seconds % 3600) / 60;
	return std::to_string(hours) + " h " + std::to_string(minutes) + " min";
}

void MonitoringModule::end()
{
	if (client_socket_ >= 0) {
		close(client_socket_);
		client_socket_ = -1;
	}
}