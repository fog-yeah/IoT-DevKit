# DevKit Translator

## Steps to start

1. Setup development environment by following [Get Started](https://microsoft.github.io/azure-iot-developer-kit/docs/get-started/)
2. Open VS Code
3. Press **F1** or **Ctrl + Shift + P** - `IoT Workbench: Examples` and select DevKitTranslator

## Provision Cognitive Service
  * Login to http://portal.azure.com
  * Select the **+ New** option.
  * Select **AI + Cognitive Services** from the list of services.
  * Select **Translator Speech API**. You may need to click "See all" or search to see it.
  * Fill out the rest of the form, and select the **Create** button.
  * You are now subscribed to Microsoft Translator Speech API.
  * Go to **All Resources** and select the Microsoft Translator API you subscribed to.
  * Go to the **Keys** option and copy your subscription key to access the service.

## Provision Azure Services

1. Press **F1** or **Ctrl + Shift + P** in Visual Studio Code - **IoT Workbench:Cloud** and click **Azure Provision**
2. Select a subscription.
3. Select or choose a resource group.
4. Select or create an IoT Hub.
5. Wait for the deployment.
6. Select or create an IoT Hub device. Please take a note of the **device name**.
7. Create Function App. Please take a note of the **Function APP Name**
8. Wait for the deployment.

## Deploy Function App
1. Open **devkit-translator\run.csx** and modify the following part with the IoT Hub device and cognitive subscription key you created.
```
    string subscriptionKey = "";
    string deviceName = "";
```

2. Press **F1** or **Ctrl + Shift + P** in Visual Studio Code - **IoT Workbench: Cloud** and click **Azure Deploy**.
3. Wait for function app code uploading.

## Configure IoT Hub Device Connection String in DevKit

1. Connect your DevKit to your machine.
2. Press **F1** or **Ctrl + Shift + P** in Visual Studio Code - **IoT Workbench: Device** and click **config-device-connection**.
3. Hold button A on DevKit, then press rest button, and then release button A to enter config mode.
4. Wait for connection string configuration to complete.

## Upload Arduino Code to DevKit

1. Open **azure_config.h** under **Device** folder and set Function APP name in the following line:
```
#define AZURE_FUNCTION_APP_NAME ""
```

2. Connect your DevKit to your machine.
3. Press **F1** or **Ctrl + Shift + P** in Visual Studio Code - **IoT Workbench:Device** and click **Device Upload**.
4. Wait for arduino code uploading.
