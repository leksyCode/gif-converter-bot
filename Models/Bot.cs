using GroupDocs.Conversion.FileTypes;
using StickersGIFBot.Commands;
using StickersGIFBot.Models.Commands;
using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using Telegram.Bot;

namespace StickersGIFBot.Models
{
    public static class Bot
    {
        private static TelegramBotClient botClient;       
        private static List<Command> commandsList;
        public static IReadOnlyList<Command> Commands => commandsList.AsReadOnly();
        public static bool DebugMode { get; set; } = false;

  
        public static async Task<TelegramBotClient> GetBotClientAsync()
        {
            if (botClient != null)
            {
                return botClient;
            }

            commandsList = new List<Command>();
            commandsList.Add(new StartCommand());
            commandsList.Add(new DebugModeCommand());

            botClient = new TelegramBotClient(AppSettings.ApiKey);
            string hook = string.Format(AppSettings.Url, "api/message/update");
            await botClient.SetWebhookAsync(hook);
            return botClient;
        }
    }
}
