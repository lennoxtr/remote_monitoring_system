#include "ManagementModule.h"
#include "AddResult.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <iostream>

ManagementModule::ManagementModule() {
}

AddResult ManagementModule::add_operator(std::int64_t chat_id, const std::string& passcode) {
	std::lock_guard<std::mutex> lock(mtx_);
	const nlohmann::json vessel_record = read_json(vessel_record_path_);
	std::string vessel_name;
	for (const auto& [name, v] : vessel_record.items()) {
		if (v.at("passcode").get<std::string>() == passcode) {
			vessel_name = name;
			break;
		}
	}
	if (vessel_name.empty()) {
		return AddResult::WrongPasscode;
	}
	
	nlohmann::json assignment_record = read_json(assignment_record_path_);
	auto& operators = assignment_record[vessel_name]["operators"];
	for (const auto& id : operators) {
		if (id.get<std::int64_t>() == chat_id) {
			return AddResult::AlreadySubscribed;
		}
	}

	operators.push_back(chat_id);

	nlohmann::json operator_record = read_json(operator_record_path_);
	auto& op = operator_record[std::to_string(chat_id)];
	op["is_muted"] = false;
	auto& subscribed = op["subscribed_vessels"];
	bool listed = false;
	for (const auto& name : subscribed) {
		if (name.get<std::string>() == vessel_name) {
			listed = true;
			break;
		}
	}
	if (!listed) {
		subscribed.push_back(vessel_name);
	}

	write_json(assignment_record_path_, assignment_record);
	write_json(operator_record_path_, operator_record);

	return AddResult::Added;
}

bool ManagementModule::remove_operator(std::int64_t chat_id, const std::string& vessel_name) {
	std::lock_guard<std::mutex> lock(mtx_);

	nlohmann::json assignment_record = read_json(assignment_record_path_);

	auto vessel_it = assignment_record.find(vessel_name);
	if (vessel_it == assignment_record.end()) {
		return false;	// no such vessel
	}
	auto& operators = (*vessel_it)["operators"];

	auto it = std::find_if(operators.begin(), operators.end(),
		[chat_id](const nlohmann::json& id) { return id.get<std::int64_t>() == chat_id; });

	if (it == operators.end()) {
		return false;	// this chat_id was not subscribed to the vessel
	}
	operators.erase(it);

	nlohmann::json operator_record = read_json(operator_record_path_);
	auto& op = operator_record[std::to_string(chat_id)];
	auto& subscribed = op["subscribed_vessels"];

	it = std::find_if(subscribed.begin(), subscribed.end(),
		[vessel_name](const nlohmann::json& name) { return name.get<std::string>() == vessel_name; });
	if (it != subscribed.end()) {
		subscribed.erase(it);
	}


	write_json(assignment_record_path_, assignment_record);
	write_json(operator_record_path_, operator_record);
	return true;
}

void ManagementModule::set_mute_status(std::int64_t chat_id, bool mute_status) {
	std::lock_guard<std::mutex> lock(mtx_);
	nlohmann::json operator_record = read_json(operator_record_path_);
	auto& op = operator_record[std::to_string(chat_id)];
	op["is_muted"] = mute_status;
	write_json(operator_record_path_, operator_record);
}

bool ManagementModule::is_muted(std::int64_t chat_id) {
	std::lock_guard<std::mutex> lock(mtx_);
	const nlohmann::json operator_record = read_json(operator_record_path_);

	const auto& operator_snippet = operator_record.at(std::to_string(chat_id));

	bool is_muted = operator_snippet.at("is_muted");
	return is_muted;
}

std::vector<std::string> ManagementModule::get_subscribed_vessel_list(std::int64_t chat_id) {
	std::lock_guard<std::mutex> lock(mtx_);
	const nlohmann::json operator_record = read_json(operator_record_path_);

	auto it = operator_record.find(std::to_string(chat_id));
	if (it == operator_record.end()) {
		return {};
	}
	return it->value("subscribed_vessels", std::vector<std::string>{});

}

std::vector<std::int64_t> ManagementModule::get_operator_list(const std::string& vessel_name) {
	std::lock_guard<std::mutex> lock(mtx_);

	const nlohmann::json assignment_record = read_json(assignment_record_path_);

	auto it = assignment_record.find(vessel_name);
	if (it == assignment_record.end()) {
		return {};
	}
	return it->at("operators").get<std::vector<std::int64_t>>();
}

std::vector<VesselInfo> ManagementModule::get_all_vessels()
{	
	std::lock_guard<std::mutex> lock(mtx_);

	const nlohmann::json record = read_json(vessel_record_path_);

	std::vector<VesselInfo> vessels;
	for (const auto& [name, v] : record.items()) {
		vessels.push_back({
			name,
			v.at("target_ip").get<std::string>(),
			v.at("target_port").get<std::uint16_t>()
			});
	}
	return vessels;
}

nlohmann::json ManagementModule::read_json(const std::string& path) {
	std::ifstream file(path);
	if (!file) {
		std::cerr << "Cannot open " + path << std::endl;
	}
	return nlohmann::json::parse(file);
}

bool ManagementModule::write_json(const std::string& path, const nlohmann::json& data) {
	std::ofstream file(path);
	if (!file) {
		std::cerr << "Cannot write " + path << std::endl;
		return false;
	}
	file << data.dump(2) << '\n';
	return true;
}
