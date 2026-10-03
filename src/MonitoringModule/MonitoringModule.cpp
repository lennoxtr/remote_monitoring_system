#include "MonitoringModule.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

Watchdog::Watchdog(
	std::chrono::seconds watchdog_timeout_freq,
	std::chrono::seconds broadcast_freq,
	AlertCallback alert_callback
	)
	: watchdog_timeout_freq_(watchdog_timeout_freq),
	broadcast_freq_(broadcast_freq),
	last_reset_time_(std::chrono::steady_clock::now()),
	next_alert_time_(last_reset_time_),
	watcher_thread_(&Watchdog::monitor, this),
	alert_callback_(alert_callback)
{
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
	if (was_timed_out && alert_callback_) {
		// Handle restore msg if have
		alert_callback_("✅ Link to USV restored");
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
			if (alert_callback_) {
				alert_callback_("Link to USV Lost");
			}          
			lock.lock();
			continue;
		}
		cv_.wait_until(lock, next_alert_time_, [this] { return !running_; });
	}
}

MonitoringModule::MonitoringModule(
	const std::string& ip,
	int port,
	std::chrono::seconds watchdog_timeout_freq,
	std::chrono::seconds broadcast_freq,
	AlertCallback alert_callback
)
	: ip_(ip),
	port_(port),
	alert_callback_(alert_callback),
	watchdog_(
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
	// Found pos -> end of msg
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

		std::string message_type = parsed_message["type"];

		if (message_type == "healthcheck") {
			std::cout << "Healthcheck Message" << std::endl;
			return 1;
		}
		else
		{	
			auto now = std::chrono::steady_clock::now();
			json alarm_payload = parsed_message["payload"];

			std::string alert_broadcast = "🚨FLOAT SWITCH ALARM!!!🚨\n";

			for (const auto& [name, info] : alarm_payload.items()) {
				int elapsed = info.value("elapsed", 0);
				alert_broadcast += "\n- " + name + ": " + std::to_string(elapsed) + " s";
				std::cout << name << " in alarm for " << elapsed << " s" << std::endl;
			}
			//pass to alarm module
			if (alert_callback_ && (now >= next_broadcast_time_)) {
				alert_callback_(alert_broadcast);
				next_broadcast_time_ = now + broadcast_freq_;
			}
			return 1;
		}
	}

	catch (...) {
		std::cerr << "Failed to parse message" << std::endl;
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

					if (process_messages(received_msg)) {
						watchdog_.reset();
					}
				}
				end();
			}
			std::this_thread::sleep_for(std::chrono::seconds(3));
		}
	}
	catch (...) {
		std::cout << "LOL" << std::endl;
	}
}

int MonitoringModule::end()
{
	if (client_socket_ >= 0) {
		close(client_socket_);
		client_socket_ = -1;
	}
	return 1;
}