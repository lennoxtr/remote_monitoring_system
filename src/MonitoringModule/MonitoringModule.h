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
		std::chrono::seconds watchdog_timeout_freq_,
		std::chrono::seconds broadcast_freq_,
		AlertCallback alert_callback
	);
	~Watchdog();

	void reset();
private:

	void monitor();
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

private:
	AlertCallback alert_callback_;

	std::string name_;
	std::string ip_;
	int port_;
	int client_socket_ = -1;
	std::string rx_buffer_;
	std::chrono::seconds watchdog_timeout_freq_;
	std::chrono::seconds broadcast_freq_;
	std::chrono::steady_clock::time_point next_broadcast_time_;

	Watchdog watchdog_;
	bool initialize_socket();
	bool get_message_once(std::string& out_message);
	int process_messages(const std::string& message);
	int end();

	std::string strip_prefix(const std::string& name, const std::string& prefix);
	std::string format_elapsed(int seconds);

};