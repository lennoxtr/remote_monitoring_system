#pragma once

#include "AddResult.h"
#include <string>
#include <mutex>
#include <vector>
#include <cstdint>
#include <nlohmann/json.hpp>

struct VesselInfo {
	std::string name;
	std::string target_ip;
	std::uint16_t target_port;
};

class ManagementModule {
public:
	ManagementModule();

	AddResult add_operator(std::int64_t chat_id, const std::string& passcode); // to assignment
	bool remove_operator(std::int64_t chat_id, const std::string& vessel_name); // from assignment
	void set_mute_status(std::int64_t chat_id, bool mute_status);
	bool is_muted(std::int64_t chat_id);
	std::vector<std::string> get_subscribed_vessel_list(std::int64_t chat_id);
	std::vector<std::int64_t> get_operator_list(const std::string& vessel_name);

	std::vector<VesselInfo> get_all_vessels();

	void add_vessel();
	void remove_vessel();
private:
	nlohmann::json read_json(const std::string& path);
	bool write_json(const std::string& path, const nlohmann::json& data);

	std::mutex mtx_;
	std::string vessel_record_path_ = "../data/vessel_record.json";
	std::string operator_record_path_ = "../data/operator_record.json";
	std::string assignment_record_path_ = "../data/assignment_record.json";
};