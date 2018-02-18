/**************************************************************
   WiFiManager is a library for the ESP8266/Arduino platform
   (https://github.com/esp8266/Arduino) to enable easy
   configuration and reconfiguration of WiFi credentials using a Captive Portal
   inspired by:
   http://www.esp8266.com/viewtopic.php?f=29&t=2520
   https://github.com/chriscook8/esp-arduino-apboot
   https://github.com/esp8266/Arduino/tree/esp8266/hardware/esp8266com/esp8266/libraries/DNSServer/examples/CaptivePortalAdvanced
   Built by AlexT https://github.com/tzapu
   Licensed under MIT license
 **************************************************************/

#ifndef WiFiManager_h
#define WiFiManager_h

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <DNSServer.h>
#include <memory>

extern "C" {
  #include "user_interface.h"
}

const char HTTP_HEAD[] PROGMEM            = "<html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/><title>{v}</title>";
const char HTTP_STYLE[] PROGMEM           = "<style>#password{-webkit-text-security: disc;} form{margin-bottom: -10px;} .header{margin-top: 20px; width:250px; height: 55px; background:url('data:image/png;base64,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') no-repeat left center; background-size: contain;} a{color: #D30000;} a:hover{color: black; text-decoration: none;} a:visited{color: #D30000;} p{margin-bottom: -10px;} .c{text-align: center;} div,input{padding:5px;font-size:1em;} input{width:95%;} body{text-align: center;font-family:verdana;} button{border:0;border-radius:0.3rem;background-color:#D30000;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;} .q{float: right;width: 64px;text-align: right;} .l{background: url(\"data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAMAAABEpIrGAAAALVBMVEX///8EBwfBwsLw8PAzNjaCg4NTVVUjJiZDRUUUFxdiZGSho6OSk5Pg4eFydHTCjaf3AAAAZElEQVQ4je2NSw7AIAhEBamKn97/uMXEGBvozkWb9C2Zx4xzWykBhFAeYp9gkLyZE0zIMno9n4g19hmdY39scwqVkOXaxph0ZCXQcqxSpgQpONa59wkRDOL93eAXvimwlbPbwwVAegLS1HGfZAAAAABJRU5ErkJggg==\") no-repeat left center;background-size: 1em;}</style>";
const char HTTP_SCRIPT[] PROGMEM          = "<script>function c(l){document.getElementById('s').value=l.innerText||l.textContent;document.getElementById('p').focus();}</script>";
const char HTTP_HEAD_END[] PROGMEM        = "</head><body><div style='text-align:left;display:inline-block;min-width:260px;'>";
const char HTTP_PORTAL_OPTIONS[] PROGMEM  = "<form action=\"/wifi\" method=\"get\"><button>Configure Swifitch</button></form><br><form action=\"/toggle_relay\" method=\"get\"><button>Test relay</button></form><br><form action=\"/i\" method=\"get\"><button>Info</button></form>";
const char HTTP_ITEM[] PROGMEM            = "<div><a href='#p' onclick='c(this)'>{v}</a>&nbsp;<span class='q {i}'>{r}%</span></div>";
const char HTTP_FORM_START[] PROGMEM      = "<form method='get' action='wifisave'><input id='s' name='s' length=32 placeholder='SSID'><br><input id='p' name='p' length=64 type='password' placeholder='Password'><br>";
const char HTTP_FORM_LABEL[] PROGMEM      = "<label for=\"{i}\">{p}</label>";
const char HTTP_FORM_PARAM[] PROGMEM      = "<br><input id='{i}' name='{n}' length={l} placeholder='{p}' value='{v}' {c}>";
const char HTTP_FORM_END[] PROGMEM        = "<br><button type='submit'>Save settings</button></form>";
const char HTTP_SCAN_LINK[] PROGMEM       = "<br><div class=\"c\"><a href=\"/wifi\">Re-scan</a></div>";
const char HTTP_SAVED[] PROGMEM           = "<div>Settings Saved<br />Trying to connect Swifitch to network.<br />If it fails reconnect to AP to try again</div>";
const char HTTP_END[] PROGMEM             = "</div></body></html>";

#define WIFI_MANAGER_MAX_PARAMS 10

class WiFiManagerParameter {
  public:
    WiFiManagerParameter(const char *custom);
    WiFiManagerParameter(const char *id, const char *placeholder, const char *defaultValue, int length);
    WiFiManagerParameter(const char *id, const char *placeholder, const char *defaultValue, int length, const char *custom);

    const char *getID();
    const char *getValue();
    const char *getPlaceholder();
    int         getValueLength();
    const char *getCustomHTML();
  private:
    const char *_id;
    const char *_placeholder;
    char       *_value;
    int         _length;
    const char *_customHTML;

    void init(const char *id, const char *placeholder, const char *defaultValue, int length, const char *custom);

    friend class WiFiManager;
};


class WiFiManager
{
  public:
    WiFiManager();

    boolean       autoConnect();
    boolean       autoConnect(char const *apName, char const *apPassword = NULL);

    //if you want to always start the config portal, without trying to connect first
    boolean       startConfigPortal();
    boolean       startConfigPortal(char const *apName, char const *apPassword = NULL);

    // get the AP name of the config portal, so it can be used in the callback
    String        getConfigPortalSSID();

    void          resetSettings();

    //sets timeout before webserver loop ends and exits even if there has been no setup.
    //usefully for devices that failed to connect at some point and got stuck in a webserver loop
    //in seconds setConfigPortalTimeout is a new name for setTimeout
    void          setConfigPortalTimeout(unsigned long seconds);
    void          setTimeout(unsigned long seconds);

    //sets timeout for which to attempt connecting, usefull if you get a lot of failed connects
    void          setConnectTimeout(unsigned long seconds);


    void          setDebugOutput(boolean debug);
    //defaults to not showing anything under 8% signal quality if called
    void          setMinimumSignalQuality(int quality = 8);
    //sets a custom ip /gateway /subnet configuration
    void          setAPStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn);
    //sets config for a static IP
    void          setSTAStaticIPConfig(IPAddress ip, IPAddress gw, IPAddress sn);
    //called when AP mode and config portal is started
    void          setAPCallback( void (*func)(WiFiManager*) );
    //called when settings have been changed and connection was successful
    void          setSaveConfigCallback( void (*func)(void) );
    //adds a custom parameter
    void          addParameter(WiFiManagerParameter *p);
    //if this is set, it will exit after config, even if connection is unsucessful.
    void          setBreakAfterConfig(boolean shouldBreak);
    //if this is set, try WPS setup when starting (this will delay config portal for up to 2 mins)
    //TODO
    //if this is set, customise style
    void          setCustomHeadElement(const char* element);
    //if this is true, remove duplicated Access Points - defaut true
    void          setRemoveDuplicateAPs(boolean removeDuplicates);

  private:
    std::unique_ptr<DNSServer>        dnsServer;
    std::unique_ptr<ESP8266WebServer> server;

    //const int     WM_DONE                 = 0;
    //const int     WM_WAIT                 = 10;

    //const String  HTTP_HEAD = "<!DOCTYPE html><html lang=\"en\"><head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\"/><title>{v}</title>";

    void          setupConfigPortal();
    void          startWPS();

    const char*   _apName                 = "no-net";
    const char*   _apPassword             = NULL;
    String        _ssid                   = "";
    String        _pass                   = "";
    unsigned long _configPortalTimeout    = 0;
    unsigned long _connectTimeout         = 0;
    unsigned long _configPortalStart      = 0;

    IPAddress     _ap_static_ip;
    IPAddress     _ap_static_gw;
    IPAddress     _ap_static_sn;
    IPAddress     _sta_static_ip;
    IPAddress     _sta_static_gw;
    IPAddress     _sta_static_sn;

    int           _paramsCount            = 0;
    int           _minimumQuality         = -1;
    boolean       _removeDuplicateAPs     = true;
    boolean       _shouldBreakAfterConfig = false;
    boolean       _tryWPS                 = false;

    const char*   _customHeadElement      = "";

    //String        getEEPROMString(int start, int len);
    //void          setEEPROMString(int start, int len, String string);

    int           status = WL_IDLE_STATUS;
    int           connectWifi(String ssid, String pass);
    uint8_t       waitForConnectResult();

    void          handleRoot();
    void          handleWifi(boolean scan);
    void          handleWifiSave();
    void          handleInfo();

    int           relay_state = 0;
    void          handleToggle();

    void          handleNotFound();
    boolean       captivePortal();
    boolean       configPortalHasTimeout();

    // DNS server
    const byte    DNS_PORT = 53;

    //helpers
    int           getRSSIasQuality(int RSSI);
    boolean       isIp(String str);
    String        toStringIp(IPAddress ip);

    boolean       connect;
    boolean       _debug = true;

    void (*_apcallback)(WiFiManager*) = NULL;
    void (*_savecallback)(void) = NULL;

    WiFiManagerParameter* _params[WIFI_MANAGER_MAX_PARAMS];

    template <typename Generic>
    void          DEBUG_WM(Generic text);

    template <class T>
    auto optionalIPFromString(T *obj, const char *s) -> decltype(  obj->fromString(s)  ) {
      return  obj->fromString(s);
    }
    auto optionalIPFromString(...) -> bool {
      DEBUG_WM("NO fromString METHOD ON IPAddress, you need ESP8266 core 2.1.0 or newer for Custom IP configuration to work.");
      return false;
    }
};

#endif
