using GroupDocs.Conversion.FileTypes;
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
    public class GIFConvertCommand : Command
    {
        public override string Name => @"/gif_conversion";

        public override async void Execute(Message message, TelegramBotClient botClient)
        {
            Bot.FormatToConvert = ImageFileType.Gif;
            var chatId = message.Chat.Id;
            await botClient.SendTextMessageAsync(chatId, "Conversion format: .gif");
        }
    }
}
