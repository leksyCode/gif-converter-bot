using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Threading.Tasks;
using GroupDocs.Conversion;
using GroupDocs.Conversion.Options.Convert;
using Microsoft.AspNetCore.Http;
using Microsoft.AspNetCore.Mvc;
using StickersGIFBot.Models;
using Telegram.Bot.Types;
using Telegram.Bot.Types.Enums;
using Telegram.Bot.Types.InputFiles;

namespace StickersGIFBot.Controllers
{    
    [Route("api/message/update")] // webhook
    public class MessageController : Controller
    {
      

        // GET api/message/update
        [HttpGet]
        public string Get()
        {
            return "Endpoint for webhooks";
        }

        // POST api/message/update
        [HttpPost]
        public async Task<OkResult> Post([FromBody]Update update)
        {
            if (update == null) return Ok();

            var commands = Bot.Commands;
            var message = update.Message;
            var botClient = await Bot.GetBotClientAsync();
            var format = Bot.FormatToConvert;

            // for stickers
            if (message.Type == MessageType.Sticker) 
            {
                await botClient.SendTextMessageAsync(message.Chat, string.Format("Associated emoji: {0}\nAnimeted: {1}\nPack: {2}",
                    message.Sticker.Emoji, message.Sticker.IsAnimated, message.Sticker.SetName), replyToMessageId: message.MessageId);

                try
                {
                    // Download sticker
                    var file = await botClient.GetFileAsync(message.Sticker.FileId);
                    var localSavePath = "..\\" + message.Sticker.FileUniqueId + ".webp";
                   
                    using (var saveImageStream = new FileStream(localSavePath, FileMode.Create))
                    {
                        await botClient.DownloadFileAsync(file.FilePath, saveImageStream);

                    }

                    // Convert sticker 
                    var localUplaodPath = "..\\" + message.Sticker.FileUniqueId + "." + format.ToString();
                                      
                    using (Converter converter = new Converter(localSavePath))
                    {
                        ImageConvertOptions options = new ImageConvertOptions
                        { // Set the conversion format
                            Format = format
                        };
                        converter.Convert(localUplaodPath, options);
                    }

                    // Upload sticker
                    using (FileStream fs = System.IO.File.OpenRead(localUplaodPath))
                    {
                        InputOnlineFile inputOnlineFile = new InputOnlineFile(fs, message.Sticker.FileUniqueId + "." + format.ToString());
                        await botClient.SendDocumentAsync(message.Chat, inputOnlineFile);
                    }

                }
                catch (Exception e )
                {
                    await botClient.SendTextMessageAsync(message.Chat, "Server error. \n " + e.Message);

                }

                //await botClient.SendTextMessageAsync(message.Chat, string.Format("Associated emoji:{0}\nAnimeted:{1}\nstickers pack:{2}",
                //    message.Sticker.Emoji, message.Sticker.IsAnimated, message.Sticker.ToString()), replyToMessageId: message.MessageId);
            }

            // for commands
            foreach (var command in commands)
            {
                if (command.Contains(message)) 
                {
                    command.Execute(message, botClient);
                    break;
                } 
            }


            return Ok();
        }

    }
}