# Wemos D1 Mini OLED Instagram Followers

A small Wemos D1 mini project for the 0.66" OLED Hat (64x48) that fetches an Instagram follower count from RapidAPI and displays it on the screen.

## Features

- connects to WiFi
- requests Instagram profile data through [RapidAPI](https://rapidapi.com/ariefsam/api/easy-instagram-service)
- extracts the `follower` field from the API response
- displays the follower count on a 64x48 OLED
- formats larger numbers with `K` for better readability on a very small screen

## Number Display

Examples shown on the OLED:

- `4889`
- `10.2K`
- `48.9K`
- `101K`

Formatting rules:

- below `10000`: raw value, for example `4889`
- from `10000` to `99999`: `xx.xK`, for example `10.4K`
- from `100000`: rounded `xxxK`, for example `101K`

## Hardware

- Wemos D1 mini
- OLED Hat 0.66" SSD1306 64x48

## Environment

- PlatformIO
- Arduino framework
- board: `d1_mini`

## Libraries

This project uses:

- `Adafruit GFX Library`
- `Adafruit SSD1306`
- `ArduinoJson`
- `ESP8266HTTPClient`
- `ESP8266WiFi`

## Configuration

Main configuration values are in [main.cpp](main.cpp):

- `ssid`
- `password`
- `instagram_username`
- `rapidapi_key`
- `rapidapi_host`
- `rapidapi_path`

## Refresh Interval

The project refreshes the follower count every `600000 ms`, which is about `10 minutes`.

That results in roughly `4320` requests per month, which stays safely below a `5k/month` API limit.

Refresh constant:

- `checkIntervalMs = 600000`

If you want to change the refresh rate:

- lower value = more frequent refreshes
- higher value = lower API usage

## Request Flow

The code uses a simple HTTP path:

- `WiFiClientSecure` with `HTTPClient`
- HTTPS request to RapidAPI
- up to `3` retries on failure
- lightweight parsing focused on the `follower` field only

This keeps the implementation small enough for the ESP8266 and easy to maintain.

## How To Run

1. Open the project in PlatformIO.
2. Fill in your WiFi and RapidAPI credentials in [main.cpp](main.cpp).
3. Upload the firmware to the board.
4. Open the serial monitor.

## Serial Output

Typical serial output includes:

- WiFi connection status
- HTTP response code
- current follower count

Example:

```text
WiFi Connected!
Starting RapidAPI request (attempt 1/3)...
HTTP code: 200
Followers: 3300
```

## Error Handling

If the API temporarily fails, the code:

- retries up to `3` times
- shows `ERR` on the OLED
- prints the error code to the serial monitor

Common error codes:

- `-1` HTTP connection error
- `-2` HTTP response other than `200`
- `-3` empty response body
- `-4` follower field not found in the response
