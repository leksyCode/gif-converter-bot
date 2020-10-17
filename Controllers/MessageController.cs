using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
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
        // GET api/values
        [HttpGet]
        public string Get()
        {
            return "Endpoint for webhooks";
        }

        // POST api/values
        [HttpPost]
        public async Task<OkResult> Post([FromBody]Update update)
        {
            if (update == null) return Ok();

            var commands = Bot.Commands;
            var message = update.Message;
            var botClient = await Bot.GetBotClientAsync();

            

            foreach (var command in commands)
            {
                if (command.Contains(message)) // for commands
                {
                    await command.Execute(message, botClient);
                    break;
                } 
                else if (message.Type == MessageType.Sticker) // for stickers
                {
                    await botClient.SendStickerAsync(message.Chat, message.Sticker.FileId);
                    await botClient.SendTextMessageAsync(message.Chat, string.Format("Emoji:{0}\nAnimeted:{1}", 
                        message.Sticker.Emoji, message.Sticker.IsAnimated), replyToMessageId:message.MessageId);
                    break;
                }
            }
            return Ok();
        }
    }
}