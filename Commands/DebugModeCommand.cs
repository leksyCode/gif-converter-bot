using StickersGIFBot.Models;
using StickersGIFBot.Models.Commands;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Telegram.Bot;
using Telegram.Bot.Types;
using Telegram.Bot.Types.Enums;

namespace StickersGIFBot.Commands
{
    public class DebugModeCommand : Command
    {
        public override string Name => @"/debug";

        public override async void Execute(Message message, TelegramBotClient botClient)
        {        
            if (Bot.DebugMode == false)
                Bot.DebugMode = true;
            else
                Bot.DebugMode = false;

            var chatId = message.Chat.Id;
            await botClient.SendTextMessageAsync(chatId, $"```Debug mode: {Bot.DebugMode}. In debug mode you have access to bash! Just send the commands like: 'cd cashe; ls'```",   parseMode: ParseMode.Markdown);
        }
    }
}
