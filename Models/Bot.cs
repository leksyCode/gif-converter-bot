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

        private static ImageFileType formatToConvert = ImageFileType.Gif;

        public static readonly string supportedFormats = "jpg, png, gif, bmp, ico";
        public static ImageFileType FormatToConvert
        {
            get { return formatToConvert; }

            set
            {
                if (supportedFormats.Contains(value))
                {
                    formatToConvert = value;
                }
            }
        }
  
        public static async Task<TelegramBotClient> GetBotClientAsync()
        {
            if (botClient != null)
            {
                return botClient;
            }

            commandsList = new List<Command>();
            commandsList.Add(new StartCommand());
            commandsList.Add(new GIFConvertCommand());
            commandsList.Add(new PNGConvertCommand());
            commandsList.Add(new JPGConvertCommand());
            commandsList.Add(new BMPConvertCommand());
            commandsList.Add(new ICOConvertCommand());



            botClient = new TelegramBotClient(AppSettings.ApiKey);
            string hook = string.Format(AppSettings.Url, "api/message/update");
            await botClient.SetWebhookAsync(hook);
            return botClient;
        }
    }
}
