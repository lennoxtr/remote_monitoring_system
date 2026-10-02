#pragma once
#include <string>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <thread>

class Watchdog {
public:
	using AlertCallback = std::function<void(const std::string&)>;

	Watchdog(
		std::chrono::seconds timeout_freq_,
		std::chrono::seconds alert_freq_,
		AlertCallback alert_callback
	);
	~Watchdog();

	void reset();
private:

	void monitor();
	std::chrono::seconds timeout_freq_;
	std::chrono::seconds alert_freq_;
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
		const std::string& ip,
		int port,
		AlertCallback alert_callback
	);
	~MonitoringModule();

	void start();

private:
	AlertCallback alert_callback_;

	std::string ip_;
	int port_;
	int client_socket_ = -1;
	std::string rx_buffer_;

	Watchdog watchdog_;
	std::string build_alert_message(const std::string& cerbo_gx_payload);
	bool initialize_socket();
	bool get_message_once(std::string& out_message);
	int process_messages(const std::string& message);
	int end();
};