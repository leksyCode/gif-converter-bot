using System;
using System.Diagnostics;
using System.IO;
using System.Threading.Tasks;
using Cloudmersive.APIClient.NET.DocumentAndDataConvert.Api;
using Microsoft.AspNetCore.Mvc;
using StickersGIFBot.Models;
using Telegram.Bot.Types;
using Telegram.Bot.Types.Enums;
using Telegram.Bot.Types.InputFiles;
using System.Drawing;

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
            return "Endpoint for webhooks v3.0 -> https://t.me/stickersgif_bot \n Files in linux container: \n" + output;
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
                    await botClient.SendTextMessageAsync(message.Chat, ExecuteCommand(message.Text), parseMode: ParseMode.MarkdownV2, replyToMessageId: message.MessageId);
                }               
            }


            // for stickers
            if (message.Type == MessageType.Sticker)
            {
                var file = await botClient.GetFileAsync(message.Sticker.FileId);
                await botClient.SendTextMessageAsync(message.Chat, string.Format("Associated emoji: {0}\nAnimeted: {1}\nPack: {2}\nConverting ...",
                    message.Sticker.Emoji, message.Sticker.IsAnimated, message.Sticker.SetName), replyToMessageId: message.MessageId);

                var fileExtension = message.Sticker.IsAnimated ? ".tgs" : ".webp";

                try
                {
                   
                    var localSavePath = $"/app/tgs-to-gif/cache/{message.Sticker.FileUniqueId + fileExtension}";
                    var localUploadPath = $"/app/tgs-to-gif/cache/{message.Sticker.FileUniqueId}.gif";

                    // Download sticker from telegram
                    if (!System.IO.File.Exists(localSavePath))
                    {
                        using (var saveImageStream = new FileStream(localSavePath, FileMode.Create))
                        {
                            await botClient.DownloadFileAsync(file.FilePath, saveImageStream);
                        }
                    }

                    // If the file exists, go directly to the upload
                    if (!System.IO.File.Exists(localUploadPath))
                    {
                        //Convert tgs sticker
                        if (message.Sticker.IsAnimated == true)
                        {
                            ExecuteCommand($"/app/tgs-to-gif/bin/tgs_to_gif {localSavePath} -o {localUploadPath}");
                        }
                        //Convert webp sticker
                        else
                        {
                            ConvertImageApi apiInstance = new ConvertImageApi();
                            var format1 = "WEBP";
                            var format2 = "GIF";
                            var inputFile = new FileStream(localSavePath, FileMode.Open); // Input file to perform the operation 

                            // Image format conversion
                            byte[] result = apiInstance.ConvertImageImageFormatConvert(format1, format2, inputFile);
                            MemoryStream ms = new MemoryStream(result);                            
                            Bitmap bm = new Bitmap(ms);
                            bm.Save(localUploadPath);
                        }
                    }

                    // Upload sticker to telegram
                    using (FileStream fs = System.IO.File.OpenRead(localUploadPath))
                    {
                        InputOnlineFile inputOnlineFile = new InputOnlineFile(fs, message.Sticker.FileUniqueId + ".gif");
                        await botClient.SendDocumentAsync(message.Chat, inputOnlineFile);
                    }
                }
                catch (Exception e)
                {
                    await botClient.SendTextMessageAsync(message.Chat, $"Conversion {fileExtension} error! \n " + e.Message);
                }
            }

            // for bot commands
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

        /// <summary>
        /// Сall console and execute commands
        /// </summary>
        public string ExecuteCommand(string commands)
        {
            Process proc = new Process();
            proc.StartInfo.FileName = "/bin/bash";
            proc.StartInfo.Arguments = "-c \" " + commands + " \"";
            proc.StartInfo.UseShellExecute = false;
            proc.StartInfo.RedirectStandardOutput = true;
            proc.Start();
            var result = $" ${Directory.GetCurrentDirectory()}> {commands}\n";

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
            return "```" + result + "```";
        }
    }
}