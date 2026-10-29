#include "AlarmModule.h"
#include <tgbot/tgbot.h>
#include <iostream>
#include <string>
#include <mutex>
#include <chrono>
#include <thread>
#include <unordered_set>
#include <tgbot/net/CurlHttpClient.h>
#include <algorithm>
#include <stdexcept>
#include <cmath>

template <typename Callback>
void require(const Callback& callback, const char* name)
{
    if (!callback) {
        throw std::invalid_argument(std::string("AlarmModule: missing callback ") + name);
    }
}


AlarmModule::AlarmModule(
    const std::string& telegram_token,
    const std::string& admin_broadcast_password,
    ManagementCallbacks management_callbacks,
    MonitoringCallbacks monitoring_callbacks
)    : bot_(telegram_token, curlHttpClient_),
    admin_broadcast_password_(admin_broadcast_password),
    management_callbacks_(std::move(management_callbacks)),
    monitoring_callbacks_(std::move(monitoring_callbacks))
{
    //Management Callback
    require(management_callbacks_.add_operator, "add_operator");
    require(management_callbacks_.remove_operator, "remove_operator");
    require(management_callbacks_.set_mute_status, "set_mute_status");
    require(management_callbacks_.is_muted, "is_muted");
    require(management_callbacks_.get_subscribed_vessel_list, "get_subscribed_vessel_list");
    require(management_callbacks_.get_operator_list, "get_operator_list");
    require(management_callbacks_.get_all_vessels, "get_all_vessels");
    require(management_callbacks_.get_all_operators, "get_all_operators");

    //Monitoring Callback
    require(monitoring_callbacks_.get_vessel_status, "get_vessel_status");
    require(monitoring_callbacks_.set_alarm_threshold, "set_alarm_threshold");

    curlHttpClient_._timeout = 15;
    register_commands();
}

void AlarmModule::register_commands()
{
    auto& events = bot_.getEvents();

    events.onCommand("start", [this](TgBot::Message::Ptr m) {
        clear_pending(m->chat->id);
        send(m->chat->id, "Hi, this is ST Remote Monitoring Bot.");
        send(m->chat->id, "Hit /subscribe to subscribe to alarm from a vessel.");
        });

    events.onCommand("subscribe", [this](TgBot::Message::Ptr m) {
        clear_pending(m->chat->id);
        send(m->chat->id, "A passcode is required to subscribe to a vessel's alerts.");
        send(m->chat->id, "Enter the passcode of the vessel you wish to subscribe to: ");
        pending_[m->chat->id] = { PendingItem::PasscodeVessel };
        });

    events.onCommand("unsubscribe", [this](TgBot::Message::Ptr m) {
        clear_pending(m->chat->id);
        std::vector<std::string> vessel_list = management_callbacks_.get_subscribed_vessel_list(m->chat->id);

        if (!vessel_list.empty()) {
            send(m->chat->id, "You have chosen to unsubscribe from vessel's alert.");
            std::string unsubscribe_confirmation = "You are subscribed to alerts from: \n";

            for (const auto& vessel_name : vessel_list) {
                unsubscribe_confirmation += "\n- " + vessel_name;
            }

            send(m->chat->id, unsubscribe_confirmation);
            send(m->chat->id, "Type the name of the vessel you want to unsubscribe from.");
            pending_[m->chat->id] = { PendingItem::VesselToUnsubscribe };
        }
        else {
            send(m->chat->id, "You are not subscribed to any vessel.");
        }
        });

    events.onCommand("vessel_check", [this](TgBot::Message::Ptr m) {
        clear_pending(m->chat->id);
        std::vector<std::string> vessel_list = management_callbacks_.get_subscribed_vessel_list(m->chat->id);

        if (!vessel_list.empty()) {
            send(m->chat->id, "You will have 10 min to conduct checks on vessel floatswitches. During this time, alarm message will be sent if floatswitch is triggered for 5 seconds. After 10min, alarm will be sent if floatswitch is triggered for 60 seconds.\n");

            std::string presea_confirmation = "\nChoose the vessel: \n";

            for (const auto& vessel_name : vessel_list) {
                presea_confirmation += "\n- " + vessel_name;
            }

            send(m->chat->id, presea_confirmation);
            pending_[m->chat->id] = { PendingItem::VesselToPresea };
        }
        else {
            send(m->chat->id, "You are not subscribed to any vessel.");
        }
        });

    events.onCommand("mute", [this](TgBot::Message::Ptr m) {
        clear_pending(m->chat->id);
        std::vector<std::string> vessel_list = management_callbacks_.get_subscribed_vessel_list(m->chat->id);

        if (vessel_list.empty()) {
            send(m->chat->id, "You are not subscribed to any vessel.");
        }
        else if (management_callbacks_.is_muted(m->chat->id)) {
            send(m->chat->id, "Already muted.");
        }
        else {
            std::string msg = "You have chosen to mute vessel's alert.\n";
            msg += "\nNOTE: you WON'T receive any message updates at all from any vessel!\n";
            msg += "\nPlease confirm your intention by typing YES";
            send(m->chat->id, msg);
            pending_[m->chat->id] = { PendingItem::ConfirmMute };
        }
        });

    events.onCommand("unmute", [this](TgBot::Message::Ptr m) {
        clear_pending(m->chat->id);
        std::vector<std::string> vessel_list = management_callbacks_.get_subscribed_vessel_list(m->chat->id);

        if (vessel_list.empty()) {
            send(m->chat->id, "You are not subscribed to any vessel.");
        }
        else if (!management_callbacks_.is_muted(m->chat->id)) {
            send(m->chat->id, "You are not muted.");
        }
        else {
            management_callbacks_.set_mute_status(m->chat->id, false);
            send(m->chat->id, "Unmuted vessel's alert.");
        }
        });

    events.onCommand("my_status", [this](TgBot::Message::Ptr m) {
        clear_pending(m->chat->id);
        std::vector<std::string> vessel_list = management_callbacks_.get_subscribed_vessel_list(m->chat->id);

        if (vessel_list.empty()) {
            send(m->chat->id, "You are not subscribed to any vessel.");
            return;
        }

        std::string reply = "Your Current Status:\n";

        reply += management_callbacks_.is_muted(m->chat->id)
            ? "\n🔇 You are muted. No alert messages will be received!\n"
            : "\n🔔 You are not muted.\n";

        reply += "\nYou are subscribed to alerts from: \n";
        for (const auto& vessel_name : vessel_list) {
            reply += "\n- " + vessel_name;
            /*
            VesselStatus vessel_status = monitoring_callbacks_.get_vessel_status(vessel_name);
            std::vector<FloatSwitchCondition> floatswitch_status_list = vessel_status.floatswitch_status_list;
            for (FloatSwitchCondition fs : floatswitch_status_list) {
                reply += "\n" + fs.name + (fs.status == DeviceState::Alarm ? " has been activated for " + fs.elapsed : " state cannot be read");
            }
            reply += "\n"
            std::vector<BatteryCondition> batteries_status_list = vessel_status.batteries_status_list;
            for (BatteryCondition batt : batteries_status_list) {
                reply += "\n" + batt.name +
                    (batt.status != DeviceState::Unreadable ?
                        " state cannot be read" :
                        " SOC: " + std::to_string(std::lround(batt.soc_pct)) + "%, time to go: " + batt.time_to_go);
            }
            */
        }
        send(m->chat->id, reply);
        send(m->chat->id, "To see a vessel's status, type its name:");
        pending_[m->chat->id] = { PendingItem::VesselToShowStatus };
        });

    events.onCommand("help", [this](TgBot::Message::Ptr m) {
        clear_pending(m->chat->id);
        send(m->chat->id,
            "List of possible commands:\n"
            "/subscribe - Start receiving alerts from a vessel.\n"
            "/unsubscribe - Stop receiving alerts from a vessel.\n"
            "/mute - Pause ALL alerts until you unmute.\n"
            "/unmute - Resume alerts after a mute.\n"
            "/my_status - Show current mute status and alert subscription.\n"
            "/vessel_check - For presea/postsea checks.\n"
            "/help - Show list of commands.");
        });

    events.onCommand("adminbroadcast", [this](TgBot::Message::Ptr m) {
        clear_pending(m->chat->id);
        send(m->chat->id, "A passcode is required to broadcast messages to operators.");
        send(m->chat->id, "Enter admin broadcast passcode: ");
        pending_[m->chat->id] = { PendingItem::PasscodeAdminBroadcast };
        });

    events.onNonCommandMessage([this](TgBot::Message::Ptr m) {
        const auto id = m->chat->id;
        auto it = pending_.find(id);
        if (it == pending_.end()) {
            send(id, "Not sure I understand that. Type /help for a list of commands.");
            return;
        }

        const PendingState pending_state = it->second;
        clear_pending(m->chat->id);
        handle_pending_reply(m, pending_state);
        });
}

void AlarmModule::handle_pending_reply(const TgBot::Message::Ptr& m, const PendingState& state)
{
    const std::string& text = m->text;

    switch (state.type) {
    case PendingItem::PasscodeVessel:
    {
        std::vector<std::string> vessel_list;
        std::string subscribe_confirmation;
        switch (management_callbacks_.add_operator(m->chat->id, text)) {
        case AddResult::Added:
            vessel_list = management_callbacks_.get_subscribed_vessel_list(m->chat->id);

            subscribe_confirmation = "You are subscribed to alerts from: \n";
            for (const auto& vessel_name : vessel_list) {
                subscribe_confirmation += "\n- " + vessel_name;
            }
            send(m->chat->id, subscribe_confirmation);
            break;
        case AddResult::AlreadySubscribed:
            send(m->chat->id, "You are already subscribed to this vessel.");
            break;
        case AddResult::WrongPasscode:
            send(m->chat->id, "The passcode you entered does not match any vessel. Try again by typing /subscribe");
            break;
        }
        break;
    }

    case PendingItem::ConfirmMute:
    {
        if (text == "YES") {
            management_callbacks_.set_mute_status(m->chat->id, true);
            send(m->chat->id, "Muted. To unmute, type /unmute.");
        }
        else {
            send(m->chat->id, "Mute cancelled. To try again, type /mute.");
        }
        break;
    }

    case PendingItem::VesselToUnsubscribe:
    {
        if (management_callbacks_.remove_operator(m->chat->id, text)) {
            send(m->chat->id, "Unsubscribed.");
        }
        else {
            send(m->chat->id, "The name you entered does not match any vessel in record. Try again by typing /unsubscribe");
        }
        break;
    }

    case PendingItem::PasscodeAdminBroadcast:
    {
        if (!admin_broadcast_password_.empty() && text == admin_broadcast_password_) {
            std::vector<std::string> all_vessel_list = management_callbacks_.get_all_vessels();

            std::string broadcast_confirmation = "Which vessel:\n";
            for (const auto& vessel_name : all_vessel_list) {
                broadcast_confirmation += "\n- " + vessel_name;
            }
            broadcast_confirmation += "\n- All";
            send(m->chat->id, broadcast_confirmation);
            pending_[m->chat->id] = { PendingItem::VesselToBroadcast };
        }
        else
        {
            send(m->chat->id, "Try again by typing /adminbroadcast.");
        }
        break;
    }

    case PendingItem::VesselToBroadcast:
    {
        std::vector<std::string> all_vessel_list = management_callbacks_.get_all_vessels();

        if (text == "All" || std::find(all_vessel_list.begin(), all_vessel_list.end(), text) != all_vessel_list.end()) {
            std::string broadcast_confirmation = "You are broadcasting to " + text;
            send(m->chat->id, broadcast_confirmation);
            send(m->chat->id, "Type broadcasting message:");
            pending_[m->chat->id] = { PendingItem::BroadcastText, text };
        }
        else {
            send(m->chat->id, "Cannot find. Try again by typing /adminbroadcast.");
        }
        break;
    }

    case PendingItem::BroadcastText:
    {
        if (text.empty()) {
            send(m->chat->id, "Empty message. Try again by typing /adminbroadcast.");
        }
        else {
            if (state.pending_data == "All") {
                std::vector<std::int64_t> all_operator_list;
                all_operator_list = management_callbacks_.get_all_operators();

                for (std::int64_t operator_chat_id : all_operator_list) {
                    send(operator_chat_id, text);
                }
            }
            else {
                broadcast(text, state.pending_data);
            }

        }
        break;
    }

    case PendingItem::VesselToPresea:
    {
        std::vector<std::string> subscribed_vessel_list = management_callbacks_.get_subscribed_vessel_list(m->chat->id);

        auto it = std::find(subscribed_vessel_list.begin(), subscribed_vessel_list.end(), text);

        if (it != subscribed_vessel_list.end()) {
            if (monitoring_callbacks_.set_alarm_threshold(text)) {
                broadcast(text + " conducting presea/postsea. Alarm will be sent if floatswitch triggered for 5 sec.", text);
            }
            else {
                send(m->chat->id, "Presea/Postsea ongoing.");
            }
            
        }
        else {
            send(m->chat->id, "The name you entered does not match any vessel in record. Try again by typing /vessel_check");
        }
        break;
    }

    case PendingItem::VesselToShowStatus:
    {
        std::vector<std::string> subscribed_vessel_list = management_callbacks_.get_subscribed_vessel_list(m->chat->id);
        auto it = std::find(subscribed_vessel_list.begin(), subscribed_vessel_list.end(), text);
        
        if (it == subscribed_vessel_list.end()) {
            send(m->chat->id, "The name you entered does not match any vessel in record. Try again by typing /my_status");
        }
        else {
            const VesselStatus status = monitoring_callbacks_.get_vessel_status(text);
            
            if (status.floatswitch_status_list.empty() && status.batteries_status_list.empty()) {
                send(m->chat->id, text + ": no data received yet.");
                break;
            }

            std::string reply = "📋 " + text + " status:\n";

            if (!status.floatswitch_status_list.empty()) {
                reply += "\nFloat switches alarm:";
                for (const auto& fs : status.floatswitch_status_list) {
                    reply += "\n- " + fs.name + ": ";
                    switch (fs.status) {
                    case DeviceState::Alarm:
                        reply += "has been activated";
                        if (fs.elapsed) {
                            reply += " for " + fs.elapsed.value_or("n/a");
                        }
                        break;
                    case DeviceState::Unreadable:
                        reply += "status cannot be read";
                        break;
                    case DeviceState::Unknown:
                        reply += "unknown";
                        break;
                    }
                }
                reply += "\n";
            }
            else {
                reply += "ALL OK ✅\n";
            }

            if (!status.batteries_status_list.empty()) {
                reply += "\nBatteries:";
                for (const auto& batt : status.batteries_status_list) {
                    reply += "\n- " + batt.name + ": ";
                    if (batt.status == DeviceState::Unreadable) {
                        reply += "status cannot be read";
                    }
                    else if (batt.status == DeviceState::Unknown) {
                        reply += "unknown";
                    }
                    else {
                        reply += "SOC " + std::to_string(std::lround(*batt.soc_pct)) + "%"
                            + ", time to go " + batt.time_to_go.value_or("n/a");
                        if (batt.status == DeviceState::Alarm) {
                            reply += " ⚠️ LOW";
                        }
                    }
                }
            }

            send(m->chat->id, reply);
        }
        break;
    }
    }
}


void AlarmModule::start()
{
    TgBot::TgLongPoll longPoll(bot_);
    while (true) {
        try {
            longPoll.start();
        }
        catch (const std::exception& e) {
            std::cerr << "Long poll error: " << e.what() << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(5));
        }
    }
}

void AlarmModule::send(std::int64_t chat_id, const std::string& message)
{
    std::lock_guard<std::mutex> lock(api_mtx_);
    try {
        bot_.getApi().sendMessage(chat_id, message);
    }
    catch (const TgBot::TgException& e) {
        std::cerr << "Telebot Exception" << std::endl;
    }
}

void AlarmModule::broadcast(const std::string& message, const std::string& vessel_name)
{

    if (!management_callbacks_.get_operator_list) {
        return;
    }
    try {
        for (std::int64_t chat_id : management_callbacks_.get_operator_list(vessel_name)) {
            if (management_callbacks_.is_muted && management_callbacks_.is_muted(chat_id)) {
                continue;
            }
            send(chat_id, message);
        }
    }
    catch (const std::exception& e) {
        std::cerr << "Broadcast for " << vessel_name << " failed: " << e.what() << std::endl;
    }
}

void AlarmModule::clear_pending(std::int64_t chat_id)
{
    pending_.erase(chat_id);
}