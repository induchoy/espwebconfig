## Integration of __Updater__ extension

```cpp
#include <Arduino.h>
#include <ewcConfigServer.h>
#include <extensions/ewcUpdater.h>

EWC::ConfigServer server;
EWC::Updater ewcUpdater;

void setup() {
    Serial.begin(115200);
    // add updater configuration
    EWC::I::get().configFS().addConfig(ewcUpdater);
    // start webserver
	server.setup();
}


void loop() {
    // process dns requests and connection state AP/STA
    server.loop();
    // should be called to perform reboot after successful update
    ewcUpdater.loop();
    if (WiFi.status() == WL_CONNECTED) {
        // do your stuff if connected
    } else {
        // or if not yet connected
    }
    delay(1);
}
```
