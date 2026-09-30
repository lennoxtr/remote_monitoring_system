#include "AlarmModule.h"
#include <tgbot/tgbot.h>
#include <string>

AlarmModule::AlarmModule(
	const std::string& telegram_token
)
	: bot_(telegram_token)
{
    bot.getEvents().onCommand("start", [&bot](std::shared_ptr<TgBot::Message> message) {
        // save chat_id here for sms alert later
        bot.getApi().sendMessage(message->chat->id, "Hi, this is ST Engineering Remote Monitoring Bot!");
        bot.getApi().sendMessage(message->chat->id, "Testing Phase: you will receive alerts from SC2");

        /*
        TgBot::InlineKeyboardButton::Ptr button(new TgBot::InlineKeyboardButton);
        button->text = "Subscribe!";
        button->callbackData = "subscribe_clicked";

        std::vector<TgBot::InlineKeyboardButton::Ptr> row;
        row.push_back(button);

        TgBot::InlineKeyboardMarkup::Ptr keyboard(new TgBot::InlineKeyboardMarkup);
        keyboard->inlineKeyboard.push_back(row);

        bot.getApi().sendMessage(message->chat->id, "Hit the button below to subscribe to a vessel's alarm!", false, 0, keyboard);
        */
    });

    bot.getEvents().onAnyMessage([&bot](std::shared_ptr<TgBot::Message> message) {
        const auto text = message->text;
        std::cout << "User wrote " << text << std::endl;
        if (text.starts_with("/start")) {
            return;
        }
        bot.getApi().sendMessage(message->chat->id, "Your message is: " + text);
        });
}

AlarmModule::start()
{
    TgBot::TgLongPoll longPoll(bot);
    while (true) {
        longPoll.start();
    }
}