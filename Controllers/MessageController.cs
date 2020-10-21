using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Threading;
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
        // GET api/message/update
        [HttpGet]
        public string Get()
        {
            var output = ExecuteCommand("dir");
            return "Endpoint for webhooks v2.4 -> https://t.me/stickersgif_bot \n" + output;
        }

        // POST api/message/update
        [HttpPost]
        public async Task<OkResult> Post([FromBody]Update update)
        {
            if (update == null) return Ok();

            var commands = Bot.Commands;
            var message = update.Message;
            var botClient = await Bot.GetBotClientAsync();

            // for Debugging remote container
            // executing any message command from bot container in bash
            if (message.Type == MessageType.Text)
            {
                if (Bot.DebugMode == true)
                {
                    await botClient.SendTextMessageAsync(message.Chat, "```" + ExecuteCommand(message.Text) + "```", ParseMode.Markdown, replyToMessageId: message.MessageId);
                }
            }


            // for animated stickers
            if (message.Type == MessageType.Sticker && message.Sticker.IsAnimated == true)
            {
                var file = await botClient.GetFileAsync(message.Sticker.FileId);
                await botClient.SendTextMessageAsync(message.Chat, string.Format("Associated emoji: {0}\nAnimeted: {1}\nPack: {2}\nConverting ...",
                    message.Sticker.Emoji, message.Sticker.IsAnimated, message.Sticker.SetName), replyToMessageId: message.MessageId);
                try
                {
                    // Download sticker

                    var localSavePath = $"/app/tgs-to-gif/cache/{message.Sticker.FileUniqueId}.tgs";

                    using (var saveImageStream = new FileStream(localSavePath, FileMode.Create))
                    {
                        await botClient.DownloadFileAsync(file.FilePath, saveImageStream);
                    }

                    //Convert sticker
                    ExecuteCommand($"/app/tgs-to-gif/bin/tgs_to_gif /app/tgs-to-gif/cache/{message.Sticker.FileUniqueId}.tgs -o /app/tgs-to-gif/cache/{message.Sticker.FileUniqueId}.gif");


                    //// Upload sticker
                    using (FileStream fs = System.IO.File.OpenRead($"/app/tgs-to-gif/cache/{message.Sticker.FileUniqueId}.gif"))
                    {
                        InputOnlineFile inputOnlineFile = new InputOnlineFile(fs, message.Sticker.FileUniqueId + ".gif");
                        await botClient.SendAnimationAsync(message.Chat, inputOnlineFile);
                    }

                }
                catch (Exception e)
                {
                    await botClient.SendTextMessageAsync(message.Chat, "Conversion error! \n " + e.Message);
                }
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

        public static string ExecuteCommand(string command)
        {
            Process proc = new Process();
            proc.StartInfo.FileName = "/bin/bash";
            proc.StartInfo.Arguments = "-c \" " + command + " \"";
            proc.StartInfo.UseShellExecute = false;
            proc.StartInfo.RedirectStandardOutput = true;
            proc.Start();
            var result = $"{Directory.GetCurrentDirectory()}$ {command}>\n";

            try
            {
                while (!proc.StandardOutput.EndOfStream)
                {
                    result += proc.StandardOutput.ReadLine() + "\n";
                }
            }
            catch (Exception e)
            {

                result = e.Message;
            }
            return result;
        }


    }
}