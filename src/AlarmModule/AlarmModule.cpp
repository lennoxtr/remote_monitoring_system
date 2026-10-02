#include "AlarmModule.h"
#include <tgbot/tgbot.h>
#include <string>
#include <mutex>


AlarmModule::AlarmModule(
    const std::string& token,
    std::string passcode,
    ManagementCallbacks management_callbacks
)    : bot_(token),
    passcode_(passcode),
    management_callbacks_(std::move(management_callbacks))
{   
    bot_.getEvents().onCommand("start", [this](TgBot::Message::Ptr m) {
        send(m->chat->id, "Hi, this is ST Remote Monitoring Bot.");
        send(m->chat->id, "Hit /subscribe <PASSCODE> to subscribe to alarm from a Vessel.");
        send(m->chat->id, "Passcode generaiton still in testing phase. Get passcode from Khang.");
        send(m->chat->id, "Many Vessel- Many Operators still in testing phase. You will receive updates from SC2.");
    });

    bot_.getEvents().onCommand("subscribe", [this](TgBot::Message::Ptr m) {
        auto sp = m->text.find(' ');
        std::string code = (sp == std::string::npos) ? "" : m->text.substr(sp + 1);
        if (code != passcode_) {
            send(m->chat->id, "Wrong or missing passcode.");
            send(m->chat->id, "Correct syntax is /subscribe <PASSCODE>");
            return;
        }
        bool added = management_callbacks_.add && management_callbacks_.add(m->chat->id);
        send(m->chat->id, added ? "Subscribed to alarms." : "Already subscribed.");
    });

    bot_.getEvents().onCommand("unsubscribe", [this](TgBot::Message::Ptr m) {
        if (management_callbacks_.remove) {
            management_callbacks_.remove(m->chat->id);
        }
        send(m->chat->id, "Unsubscribed.");
        });
}


void AlarmModule::start()
{
    TgBot::TgLongPoll longPoll(bot_);
    while (true) {
        longPoll.start();
    }
}

void AlarmModule::send(std::int64_t chat_id, const std::string& message)
{
    std::lock_guard<std::mutex> lock(api_mtx_);
    bot_.getApi().sendMessage(chat_id, message);
}

void AlarmModule::broadcast(const std::string& message)
{
    if (management_callbacks_.get_operator_list) {
        std::vector<std::int64_t> operator_list =  management_callbacks_.get_operator_list();
        for (std::int64_t chat_id : operator_list) {
            send(chat_id, message);
        }
    }
}