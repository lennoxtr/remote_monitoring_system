#include "AlarmModule.h"
#include <tgbot/tgbot.h>
#include <iostream>
#include <string>
#include <mutex>
#include <chrono>
#include <thread>
#include <unordered_set>


AlarmModule::AlarmModule(
    const std::string& token,
    ManagementCallbacks management_callbacks
)    : bot_(token),
    next_send_(std::chrono::steady_clock::now()),
    management_callbacks_(std::move(management_callbacks))
{   
    bot_.getEvents().onCommand("start", [this](TgBot::Message::Ptr m) {
        clear_pending(m->chat->id);
        send(m->chat->id, "Hi, this is ST Remote Monitoring Bot.");
        send(m->chat->id, "Hit /subscribe to subscribe to alarm from a vessel.");
        send(m->chat->id, "Many Vessels- Many Operators still in testing phase. For now, you can only receive updates from SC2.");
        });

    bot_.getEvents().onCommand("subscribe", [this](TgBot::Message::Ptr m) {
        clear_pending(m->chat->id);
        send(m->chat->id, "A passcode is required to subscribe to a vessel's alerts.");
        send(m->chat->id, "Enter the passcode of the vessel you wish to subscribe to: ");
        awaiting_passcode_.insert(m->chat->id);
        });

    bot_.getEvents().onCommand("unsubscribe", [this](TgBot::Message::Ptr m) {
        clear_pending(m->chat->id);
        send(m->chat->id, "You have chosen to unsubscribe from vessel's alert.");
        std::vector<std::string> vessel_list;
        if (management_callbacks_.get_subscribed_vessel_list) {
            vessel_list = management_callbacks_.get_subscribed_vessel_list(m->chat->id);
        }
        
        if (!vessel_list.empty()) {
            std::string unsubscribe_confirmation = "You are subscribed to alerts from: \n";

            for (const auto& vessel_name : vessel_list) {
                unsubscribe_confirmation += "\n- " + vessel_name;
            }

            send(m->chat->id, unsubscribe_confirmation);
            send(m->chat->id, "Type the name of the vessel you want to unsubscribe from.");
            awaiting_unsubscribe_confirmation_.insert(m->chat->id);
        }
        else {
            send(m->chat->id, "You are not subscribed to any vessel");
        }
        });

    bot_.getEvents().onCommand("mute", [this](TgBot::Message::Ptr m) {
        clear_pending(m->chat->id);
        std::vector<std::string> vessel_list;
        if (management_callbacks_.get_subscribed_vessel_list) {
            vessel_list = management_callbacks_.get_subscribed_vessel_list(m->chat->id);
        }
        if (!vessel_list.empty()) {
            send(m->chat->id, "You have chosen to mute vessel's alert.");
            send(m->chat->id, "Note that this means you WON'T receive any message updates at all from any vessel!");
            send(m->chat->id, "Please confirm your intention by typing YES");
            awaiting_mute_confirmation_.insert(m->chat->id);
        }
        else {
            send(m->chat->id, "You are not subscribed to any vessel");
        }
        });

    bot_.getEvents().onCommand("unmute", [this](TgBot::Message::Ptr m) {
        clear_pending(m->chat->id);
        std::vector<std::string> vessel_list;
        if (management_callbacks_.get_subscribed_vessel_list) {
            vessel_list = management_callbacks_.get_subscribed_vessel_list(m->chat->id);
        }
        if (!vessel_list.empty()) {
            if (management_callbacks_.set_mute_status) {
                management_callbacks_.set_mute_status(m->chat->id, false);
            }
            send(m->chat->id, "Unmuted vessel's alert.");
        }
        else {
            send(m->chat->id, "You are not subscribed to any vessel");
        }
        });

    bot_.getEvents().onCommand("help", [this](TgBot::Message::Ptr m) {
        clear_pending(m->chat->id);
        send(m->chat->id,
            "List of possible commands:\n"
            "/subscribe - Start receiving alerts from a vessel.\n"
            "/unsubscribe - Stop receiving alerts from a vessel.\n"
            "/mute - Pause ALL alerts until you unmute.\n"
            "/unmute - Resume alerts after a mute.\n"
            "/help - Show list of commands.");
        });

    bot_.getEvents().onNonCommandMessage([this](TgBot::Message::Ptr m) {
        if (awaiting_passcode_.contains(m->chat->id)) {
            clear_pending(m->chat->id);
            std::string passcode = m->text;
            if (management_callbacks_.add_operator) {
                switch (management_callbacks_.add_operator(m->chat->id, passcode)) {
                case AddResult::Added:
                    send(m->chat->id, "Subscribed to alerts.");
                    // TODO: print out subscribed list
                    break;
                case AddResult::AlreadySubscribed:
                    send(m->chat->id, "You are already subscribed to this vessel.");
                    break;
                case AddResult::WrongPasscode:
                    send(m->chat->id, "The passcode you entered does not match any vessel. Try again by typing /subscribe");
                    break;
                }
            }
        }
        
        else if (awaiting_mute_confirmation_.contains(m->chat->id)) {
            clear_pending(m->chat->id);
            std::string reply = m->text;
            if (reply == "YES") {
                if (management_callbacks_.set_mute_status) {
                    management_callbacks_.set_mute_status(m->chat->id, true);
                }
                send(m->chat->id, "Muted.");
            }
            else {
                send(m->chat->id, "Mute cancelled.");
            }
        }

        else if (awaiting_unsubscribe_confirmation_.contains(m->chat->id)) {
            clear_pending(m->chat->id);
            std::string vessel_name = m->text;
            if (management_callbacks_.remove_operator) {
                if (management_callbacks_.remove_operator(m->chat->id, vessel_name)) {
                    // Check
                    send(m->chat->id, "Unsubscribed");
                }
                else {
                    send(m->chat->id, "The name you entered does not match any vessel in record. Try again by typing /unsubscribe");
                }
            }
        }

        else {
            send(m->chat->id, "Not sure I understand that. Type /help for a list of commands.");
        }
        });
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
    std::this_thread::sleep_until(next_send_);

    try {
        bot_.getApi().sendMessage(chat_id, message);
    }
    catch (const TgBot::TgException& e) {
        std::cerr << "Telebot Exception" << std::endl;
    }
    next_send_ = std::chrono::steady_clock::now() + send_interval_;

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
    awaiting_passcode_.erase(chat_id);
    awaiting_mute_confirmation_.erase(chat_id);
    awaiting_unsubscribe_confirmation_.erase(chat_id);
}