#See https://aka.ms/containerfastmode to understand how Visual Studio uses this Dockerfile to build your images for faster debugging.

FROM mcr.microsoft.com/dotnet/core/aspnet:3.1-buster-slim AS base
WORKDIR /app
EXPOSE 80
EXPOSE 443

FROM mcr.microsoft.com/dotnet/core/sdk:3.1-buster AS build
WORKDIR /src
COPY ["StickersGIFBot.csproj", ""]
RUN dotnet restore "./StickersGIFBot.csproj"
COPY . .
WORKDIR "/src/."
RUN dotnet build "StickersGIFBot.csproj" -c Release -o /app/build

FROM build AS publish
RUN dotnet publish "StickersGIFBot.csproj" -c Release -o /app/publish

FROM base AS final
WORKDIR /app

RUN apt-get update && apt-get -y install libxml2 libgdiplus libc6-dev
RUN apt-get install -y git g++ cmake python-pip
RUN pip install conan
COPY --from=build /src/tgs-to-gif /app/tgs-to-gif
VOLUME /src/tgs-to-gif /app/tgs-to-gif/cache
RUN git clone https://github.com/Samsung/rlottie.git
RUN (cd rlottie && cmake CMakeLists.txt && make -j && make -j install)
RUN rm -fr rlottie
WORKDIR /app/tgs-to-gif
RUN conan install .
RUN cmake CMakeLists.txt && make -j

COPY --from=publish /app/publish .
ENTRYPOINT ["dotnet", "StickersGIFBot.dll"]
