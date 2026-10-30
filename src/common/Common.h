#pragma once
#include <optional>
#include <string>
#include <vector>

enum class AddResult { Added, AlreadySubscribed, WrongPasscode };
enum class DeviceType { Unknown, Battery, FloatSwitch };
enum class DeviceState { Unknown, Ok, Unreadable, Alarm };
enum class PreseaStatus { Activated, AlreadyActive, LinkDown };

struct FloatSwitchCondition {
	std::string name;
	DeviceState status = DeviceState::Unknown;
	std::optional<std::string> elapsed;
};

struct BatteryCondition {
	std::string name;
	DeviceState status = DeviceState::Unknown;
	std::optional<float> soc_pct;
	std::optional<std::string> time_to_go;
};

struct VesselStatus {
	bool link_up = false;
	bool has_data = false;
	std::vector<FloatSwitchCondition> floatswitch_status_list;
	std::vector<BatteryCondition> batteries_status_list;
};