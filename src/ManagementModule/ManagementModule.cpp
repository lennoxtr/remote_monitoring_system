#include "ManagementModule.h"
#include <nlohmann/json.hpp>
#include <fstream>

ManagementModule::ManagementModule() {
	std::ifstream in(path_);
	std::int64_t id;
	while (in >> id) ids_list_.insert(id);
}

bool ManagementModule::add_operator(std::int64_t chat_id) {
	std::lock_guard<std::mutex> lock(mtx_);
	bool added = ids_list_.insert(chat_id).second;
	if (added) {
		save();
	}
	return added;
}

bool ManagementModule::remove_operator(std::int64_t chat_id) {
	std::lock_guard<std::mutex> lock(mtx_);
	bool removed = ids_list_.erase(chat_id) > 0;
	if (removed) {
		save();
	}
	return removed;
}

std::vector<std::int64_t> ManagementModule::get_operators()
{
	std::lock_guard<std::mutex> lock(mtx_);
	return { ids_list_.begin(), ids_list_.end() };
}

void ManagementModule::save()
{
	std::ofstream out(path_, std::ios::trunc);
	for (auto id : ids_list_) out << id << '\n';
}