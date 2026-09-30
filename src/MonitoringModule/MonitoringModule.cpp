#include "MonitoringModule.h"
#include <sys/socket.h>
#include <arpa/inet.h>
#include <iostream>
#include <cstring>
#include <unistd.h>
#include <chrono>
#include <thread>

MonitoringModule::MonitoringModule(
	const std::string& ip,
	int port
)
	: ip_(ip),
	port_(port)
{
}

MonitoringModule::~MonitoringModule()
{
	end();
}

bool MonitoringModule::initialize_socket() 
{
	clientSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (clientSocket == -1) {
		std::cerr << "Error: Failed to create socket.\n";
		return false;
	}

	sockaddr_in serverAddress;
	std::memset(&server_address, 0, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(port_);

	if (inet_pton(AF_INET, ip_.c_str(), &serverAddress.sin_addr) <= 0) {
		std::cerr << "Error: Invalid address.\n";
		end()	;
		return false;
	}

	if (connect(clientSocket, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) < 0) {
		std::cerr << "Error: Connection failed.\n";
		end();
		return false;
	}

	rx_buffer_.clear();
	return true
}

bool MonitoringModule::get_message_once(std::string& out_message)
{	
	size_t pos;

	while ((pos = rx_buffer_.find('\n')) == std::string::npos) {
		char buffer[1024];
		ssize_t n = recv(client_socket_, buffer, sizeof(buffer), 0);
		if (n <= 0) return false;
		rx_buffer_.append(chunk, n);
	}
	// Found pos -> end of msg
	out_message = rx_buffer_.substr(0, pos); // clears \n
	rx_buffer_.erase(0, pos + 1);
	if (!out_message.empty() && out_message.back() == '\r') { // clear \r
		out_message.pop_back();
	}
	return true;

}

void MonitoringModule::process_messages()
{
	std::cout << message << std::endl;
}

void MonitoringModule::start()
{	
	try {
		initialize_socket();
	}
	catch (...) {
		std::cout << "Problem initializing socket" << std::endl;
	}

	try {
		std::string received_msg;
		while (true) {
			if (initialize_socket) {
				while (get_message_once(received_msg)) {
					process_messages(received_msg);
				}
				std::cerr << "Connection lost\n";
			}
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
}