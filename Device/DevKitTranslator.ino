// Copyright (c) Microsoft. All rights reserved.
// Licensed under the MIT license. 
// To get started please visit https://microsoft.github.io/azure-iot-developer-kit/docs/projects/devkit-translator/?utm_source=ArduinoExtension&utm_medium=ReleaseNote&utm_campaign=VSCode
#include "AudioClassV2.h"
#include "AZ3166WiFi.h"
#include "AzureIotHub.h"
#include "DevKitMQTTClient.h"
#include "OLEDDisplay.h"
#include "SystemTickCounter.h"

#include "azure_config.h"
#include "http_client.h"

#define LANGUAGES_COUNT     9
#define MAX_RECORD_DURATION 2
#define MAX_UPLOAD_SIZE     (64 * 1024)
#define LOOP_DELAY          100
#define PULL_TIMEOUT        10000
#define AUDIO_BUFFER_SIZE   ((32000 * MAX_RECORD_DURATION) - 16000 + 44 + 1)

#define APP_VERSION         "ver=1.0"

enum STATUS
{
  Idle,
  Recording,
  Recorded,
  WavReady,
  Uploaded,
  FetchResult,
  ResultAvailable,
  Playing
};

static int wavFileSize;
static char *waveFile = NULL;
static char azureFunctionUriStt[128];
static char azureFunctionUriTts[128];

// The timeout for retrieving the result
static uint64_t result_timeout_ms;

// Audio instance
static AudioClass& Audio = AudioClass::getInstance();

// Indicate the processing status
static STATUS status = Idle;

// Indicate whether WiFi is ready
static bool hasWifi = false;

// Indicate whether IoT Hub is ready
static bool hasIoTHub = false;

// Inditactes whether there is a message to play
static bool messageToPlay = false;


//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Utilities
static void InitWiFi()
{
  Screen.print(2, "Connecting...");
  
  if (WiFi.begin() == WL_CONNECTED)
  {
    IPAddress ip = WiFi.localIP();
    Screen.print(1, ip.get_address());
    hasWifi = true;
    Screen.print(2, "Running... \r\n");
  }
  else
  {
    hasWifi = false;
    Screen.print(1, "No Wi-Fi\r\n ");
  }
}

static void EnterIdleState(bool clean = true)
{
  status = Idle;
  if (clean)
  {
    Screen.clean();
    Screen.print(0, "Hack Zurich 2018");
    Screen.print(1, "Talk:  Hold B");
    Screen.print(2, "Play:  Press A");
  }
}

static int HttpTriggerUpload(const char *content, int length)
{
  if (content == NULL || length <= 0 || length > MAX_UPLOAD_SIZE)
  {
    Serial.println("Content not valid");
    return -1;
  }
  HTTPClient client = HTTPClient(HTTP_POST, azureFunctionUriStt);
  client.set_header("Content-Type", "application/octet-stream");
  const Http_Response *response = client.send(content, length);
  
  if (response != NULL && response->status_code == 200)
  {
    Screen.clean();
    Screen.print(0, "Hack Zurich 2018");
    Screen.print(1, "Talk:  Hold B");
    Screen.print(2, "Play:  Press A");
    Screen.print(3, response->body);
    status = Idle;
    return 0;
  }
  return -1;
}

static int HttpGetResult()
{
  if(messageToPlay)
  {
    return 0;
  }
  HTTPClient client = HTTPClient(HTTP_GET, azureFunctionUriTts);
  client.set_header("Content-Type", "application/octet-stream");
  const Http_Response *response = client.send();

  if (response != NULL && response->status_code == 200)
  {
    ResultMessageCallback(response->body, response->body_length);
    return 0;
  }
  return -1;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Callback functions
static void ResultMessageCallback(const char *text, int length)
{
  Serial.println("Callback start");

  EnterIdleState();
  if (text == NULL || length == 0)
  {
    return;
  }
  
  Serial.println("Callback load data");
  Screen.print(1, "Result available");
  status = ResultAvailable;

  if(length > AUDIO_BUFFER_SIZE) length = AUDIO_BUFFER_SIZE;
  memcpy(waveFile, text, length);
  wavFileSize = length;
  wavFileSize = Audio.convertToMono(waveFile, wavFileSize, 16);

  messageToPlay = true;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Actions
static void DoIdle()
{
  if (digitalRead(USER_BUTTON_A) == LOW)
  {
    // Enter Select Language mode
    status = FetchResult;
    Screen.clean();
    Screen.print(0, "Press B to play result");
  }
  else if (digitalRead(USER_BUTTON_B) == LOW)
  {
    // Enter the Recording mode
    Screen.clean();
    Screen.print(0, "Recording...");
    Screen.print(1, "Release to send\r\nMax duraion: \r\n1.5 sec");
    memset(waveFile, 0, AUDIO_BUFFER_SIZE);
    Audio.format(8000, 16);
    Audio.startRecord(waveFile, AUDIO_BUFFER_SIZE);
    status = Recording;
  }
  else
  {
    DevKitMQTTClient_Check();
  }
}

static void DoRecording()
{
    if (digitalRead(USER_BUTTON_B) == HIGH)
    {
      Audio.stop();
      status = Recorded;
    }
}

static void DoRecorded()
{
  wavFileSize = Audio.getCurrentSize();
  if (wavFileSize > 0)
  {
    wavFileSize = Audio.convertToMono(waveFile, wavFileSize, 16);
    if (wavFileSize <= 0)
    {
      Serial.println("ConvertToMono failed! ");
      EnterIdleState();
    }
    else
    {
      status = WavReady;
      Screen.clean();
      Screen.print(0, "Processing...");
      Screen.print(1, "Uploading...");
    }
  }
  else
  {
    Serial.println("No Data Recorded! ");
    EnterIdleState();
  }
}

static void DoWavReady()
{
  if (wavFileSize <= 0)
  {
    EnterIdleState();
    return;
  }
  
  if (HttpTriggerUpload(waveFile, wavFileSize) == 0)
  {
    memset(waveFile, 0, AUDIO_BUFFER_SIZE);
    // Start retrieving result timeout clock
    result_timeout_ms = SystemTickCounterRead();
  }
  else
  {
    Serial.println("Error happened when translating: Azure Function failed");
    EnterIdleState();
    Screen.print(2, "Error.......");
  }
}

static void DoFetchResult()
{
  Screen.clean();
  Screen.print(0, "Processing...");

  if(HttpGetResult() == 0)
  {
    status = ResultAvailable;
    Screen.clean();
    Screen.print(0, "Press B to play");
  }
  else
  {
    EnterIdleState(); 
    Serial.println("Failed to get speech");
  }
}

static void DoResultAvailable()
{
  if (digitalRead(USER_BUTTON_B) == LOW)
  {
    // Enter the Playing mode
    Screen.clean();
    Screen.print(0, "Playing...");
    Screen.print(1, "Release to stop");

    Audio.setVolume(60);
    Audio.startPlay(waveFile, wavFileSize);

    status = Playing;
  }
}

static void DoPlaying()
{
  if (digitalRead(USER_BUTTON_B) == HIGH)
  {
    Audio.stop();
    messageToPlay = false;
    memset(waveFile, 0, AUDIO_BUFFER_SIZE);
    EnterIdleState();
  }
}

// static void DoWavReady()
// {
//   if (wavFileSize <= 0)
//   {
//     EnterIdleState();
//     return;
//   }
  
//   if (HttpTriggerTranslator(waveFile, wavFileSize) == 0)
//   {
//     status = Uploaded;
//     Screen.print(1, "Receiving...");
//     // Start retrieving result timeout clock
//     result_timeout_ms = SystemTickCounterRead();
//   }
//   else
//   {
//     Serial.println("Error happened when translating: Azure Function failed");
//     EnterIdleState();
//     Screen.print(1, "Sorry, I can't \r\nhear you.");
//   }
// }

static void DoCheckIncoming()
{
  
  
  if ((int)(SystemTickCounterRead() - result_timeout_ms) >= PULL_TIMEOUT)
  {
    // Timeout
    EnterIdleState();
    // Screen.print(1, ERROR_INFO);
  }  
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Arduino sketch
void setup()
{
  Screen.init();
  Screen.print(0, "Hack Zurich 2018");

  if (strlen(AZURE_FUNCTION_APP_NAME) == 0)
  {
    Screen.print(2, "No Azure Func");
    return;
  }

  Screen.print(2, "Initializing...");
  pinMode(USER_BUTTON_A, INPUT);
  pinMode(USER_BUTTON_B, INPUT);

  Screen.print(3, " > Serial");
  Serial.begin(115200);
  
  // Initialize the WiFi module
  Screen.print(3, " > WiFi");
  hasWifi = false;
  InitWiFi();
  if (!hasWifi)
  {
    return;
  }
  LogTrace("DevKitTranslatorSetup", APP_VERSION);

  // IoT hub
  Screen.print(3, " > IoT Hub");
  DevKitMQTTClient_SetOption(OPTION_MINI_SOLUTION_NAME, "DevKitTranslator");
  if (!DevKitMQTTClient_Init())
  {
    Screen.clean();
    Screen.print(0, "Hack Zurich 2018");
    Screen.print(2, "No IoT Hub");
    hasIoTHub = false;
    return;
  }
  hasIoTHub = true;
  DevKitMQTTClient_SetMessageCallback(ResultMessageCallback);

  // Audio
  Screen.print(3, " > Audio");
  waveFile = (char *)malloc(AUDIO_BUFFER_SIZE);
  if (waveFile == NULL)
  {
    Screen.print(3, "Audio init fails");
    Serial.println("No enough Memory!");
    return;
  }
    
  sprintf(azureFunctionUriStt, "http://%s.azurewebsites.net/api/SpeechToText", (char *)AZURE_FUNCTION_APP_NAME);
  sprintf(azureFunctionUriTts, "http://%s.azurewebsites.net/api/TextToSpeech", (char *)AZURE_FUNCTION_APP_NAME);
  Screen.clean();
  Screen.print(0, "Hack Zurich 2018");
  Screen.print(1, "Talk:  Hold B");
  Screen.print(2, "Play:  Press A");
}

void loop()
{
  if (hasWifi && hasIoTHub)
  {
    switch (status)
    {
      case Idle:
        DoIdle();
        break;

      case Recording:
        DoRecording();
        break;

      case Recorded:
        DoRecorded();
        break;

      case WavReady:
        DoWavReady();
        break;

      case FetchResult:
        DoFetchResult();
        break;

      case ResultAvailable:
        DoResultAvailable();
        break;

      case Playing:
        DoPlaying();
        break;
    }
  }
  delay(LOOP_DELAY);
}