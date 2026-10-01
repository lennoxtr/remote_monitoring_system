#pragma once

#include <string>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <set>

class ManagementModule {
public:
	ManagementModule();
	~ManagementModule();
	void reload_all_records();
	bool add_operator(std::int64_t chat_id);
	void add_vessel();
	void add_assignment();
	bool remove_operator(std::int64_t chat_id);
	void remove_vessel();
	void remove_assignment();
	std::vector<std::int64_t> get_operators();
private:
	void save();
	std::string path_ = "..\\data\\operator_list.txt";;
	std::mutex mtx_;
	std::set<std::int64_t> ids_list_;
	std::string VESSEL_RECORD = "..\\data\\vessel_record.json";;
	std::string OPERATOR_RECORD = "..\\data\\operator_record.json";;
	std::string ASSIGNMENT_RECORD = "..\\data\\assignment_record.json";;
};