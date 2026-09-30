#pragma once
#include <string>
#include <iostream>

class MonitoringModule {
public:
	MonitoringModule(
		const std::string& ip_,
		int port_
	);

	void start();

private:
	std::string& ip_;
	int port_;
	int clientSocket;
	std::string rx_buffer_;

	int initialize_socket();
	bool get_message_once(std::string& out_message);
	void process_messages(const std::string& message);
	int end();
};