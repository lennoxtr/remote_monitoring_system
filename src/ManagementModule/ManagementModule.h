#pragma once

#include <string>
#include <mutex>
#include <condition_variable>

class Management_Module {
public:
	Management_Module(const std::string& VESSEL_RECORD, const std::string& OPERATOR_RECORD, const std::string& ASSIGNMENT_RECORD);
	void reload_all_records();
	void add_operator();
	void add_vessel();
	void add_assignment();
	void remove_operator();
	void remove_vessel();
	void remove_assignment();
private:
	std::mutex mutex_;
	std::condition_variable cv_;

	std::string VESSEL_RECORD = "..\\data\\vessel_record.json";;
	std::string OPERATOR_RECORD = "..\\data\\operator_record.json";;
	std::string ASSIGNMENT_RECORD = "..\\data\\assignment_record.json";;
};