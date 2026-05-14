  1. Class = Blueprint, Instance = The Real Thing

  A class is a blueprint. An instance is a real object built from that blueprint.

  // This is the BLUEPRINT (class definition)
  class RelayController {
      uint8_t _pins[32];
      bool _states[32];
      void begin(...) { ... }
      bool setState(...) { ... }
  };

  // This is the REAL THING (instance)
  RelayController relayController;

  Think of it like: RelayController is the architectural drawing. relayController is the actual house built from it.

  In main.cpp, you create 4 instances from 4 classes:

  ConfigManager    configManager;     // instance 1
  RelayController  relayController;   // instance 2
  ThingsBoardManager tbManager(...);  // instance 3
  ConfigMenu       configMenu(...);   // instance 4

  Each instance has its own copy of data. If you created two relay controllers:
  RelayController controller1;  // has its own _pins[], _states[]
  RelayController controller2;  // has its own separate _pins[], _states[]
  They don't share anything. Changing controller1 doesn't affect controller2.

  ---
  2. Public vs Private — Who Can Touch What

  class RelayController {
  public:                          // ANYONE can call these
      void begin(...);
      bool setState(...);
      bool getState(...);
      uint8_t getCount();

  private:                         // ONLY the class itself can touch these
      uint8_t _pins[MAX_RELAYS];
      bool _states[MAX_RELAYS];
      uint8_t _count;
      Preferences _prefs;
      void saveState();            // private helper
  };

  From main.cpp:
  relayController.setState(0, true);   // OK — public
  relayController.getCount();          // OK — public
  relayController._pins[0];           // ERROR — private, can't touch
  relayController.saveState();         // ERROR — private, can't call

  Why? Protection. The outside world says "turn relay 2 ON" via setState(). The class internally handles GPIO, validation, NVS saving. Nobody outside can corrupt _pins[] or _states[]
  directly.

  The _ prefix convention (like _pins, _count) is a naming habit that means "this is private/internal". It's not required by C++, just a readability convention.

  ---
  3. References (&) — Sharing Without Copying

  This is the most important concept in your codebase. Look at main.cpp:

  ConfigManager configManager;
  RelayController relayController;
  ThingsBoardManager tbManager(relayController, configManager);  // ← passing references
  ConfigMenu configMenu(configManager);                          // ← passing reference

  Now look at ThingsBoardManager's constructor:

  class ThingsBoardManager {
  public:
      ThingsBoardManager(RelayController& relayCtrl, ConfigManager& cfg)
          : _relayCtrl(relayCtrl)    // store the reference
          , _cfg(cfg)                // store the reference
      { }

  private:
      RelayController& _relayCtrl;   // NOT a copy — points to the original
      ConfigManager& _cfg;           // NOT a copy — points to the original
  };

  The & means reference — it's not a copy, it's the same object.

  Memory:

    main.cpp creates:
    ┌──────────────────┐
    │ configManager     │ ← the ONE real object
    └──────┬───────────┘
           │
           │ reference (not copy)
           │
    ┌──────┼───────────────────────────────┐
    │      ▼                               │
    │ tbManager._cfg ──► same object       │
    │                                      │
    │ configMenu._cfg ──► same object      │
    └──────────────────────────────────────┘

  So when the serial config menu changes _cfg.wifiSsid, ThingsBoardManager sees the change too — because they both point to the same configManager.

  Without & (pass by value), each class would get its own copy. Changing one wouldn't affect the other. That would break everything.

  ---
  4. Constructor & Initializer List

  ThingsBoardManager(RelayController& relayCtrl, ConfigManager& cfg)
      : _relayCtrl(relayCtrl)          // initializer list
      , _cfg(cfg)                      // runs BEFORE the { } body
      , _mqttClient(_wifiClient)
      , _serverRpc()
      , _apis{&_serverRpc}
      , _tb(_mqttClient, MAX_MESSAGE_SIZE, Default_Max_Stack_Size, _apis)
      , _rpcSubscribed(false)
  {
      g_tbManagerInstance = this;       // constructor body
  }

  The : ... part is the initializer list. It sets up member variables before the constructor body { } runs. For references (&) and const members, you must use the initializer list — you
  can't assign them later.

  The order matters — it follows the order members are declared in the class, not the order in the list.

  ---
  5. How Objects Connect — The Full Picture

  main.cpp creates everything and wires them together:

    ┌─────────────────┐
    │  ConfigManager   │  ← owns all config data (WiFi, TB, pins)
    │  configManager   │
    └────┬────────┬────┘
         │        │
         │ &ref   │ &ref
         │        │
    ┌────▼────┐  ┌▼──────────────────┐
    │ConfigMenu│  │ThingsBoardManager │
    │          │  │                   │
    │ can read │  │ reads wifiSsid,   │
    │ & write  │  │ tbServer, tbToken │
    │ config   │  │ to connect        │
    └──────────┘  └────────┬──────────┘
                           │
                           │ &ref
                           │
                  ┌────────▼──────────┐
                  │ RelayController    │
                  │                    │
                  │ setState()         │
                  │ getStateBitmask()  │
                  └────────────────────┘

  In main.cpp loop():
  void loop() {
      configMenu.check();      // reads serial, can modify configManager
      tbManager.update();      // uses configManager to connect, uses relayController for RPC
  }

  ---
  6. Static — Shared Across All Instances

  static ThingsBoardManager* g_tbManagerInstance = nullptr;

  static void onSetValue(JsonVariantConst const& params, JsonDocument& response) {
      if (!g_tbManagerInstance) return;
      // ...
      g_tbManagerInstance->_relayCtrl.setState(index, value);
  }

  Why static? The ThingsBoard library needs a plain function pointer for RPC callbacks. It can't accept a member function (because member functions are tied to an instance — they need this).

  So we use a static function (no this) + a global pointer to find the instance:

  ThingsBoard library calls:  onSetValue(params, response)
                                      │
                                      ▼
                              static function
                              (no 'this' pointer)
                                      │
                        g_tbManagerInstance->_relayCtrl
                                      │
                                      ▼
                              the real instance

  This is a common workaround in embedded C++ when libraries expect C-style function pointers.

  ---
  7. Composition — Objects Inside Objects

  class ThingsBoardManager {
  private:
      WiFiClient _wifiClient;                              // object inside object
      Arduino_MQTT_Client _mqttClient;                     // wraps _wifiClient
      Server_Side_RPC<1U, MAX_RPC_RESPONSE> _serverRpc;   // RPC handler
      ThingsBoard _tb;                                     // wraps _mqttClient
  };

  This is composition — objects contain other objects. The construction chain:

  _wifiClient          (raw TCP socket)
      └─► _mqttClient  (MQTT protocol on top of TCP)
              └─► _tb  (ThingsBoard API on top of MQTT)

  Each layer adds functionality on top of the previous one.

  ---
  8. The this Pointer

  Inside any member function, this is a pointer to the current instance:

  ThingsBoardManager(RelayController& relayCtrl, ConfigManager& cfg)
      : ...
  {
      g_tbManagerInstance = this;  // "this" = the instance being constructed
  }

  When main.cpp creates ThingsBoardManager tbManager(...), inside that constructor, this == &tbManager.

  ---
  9. Why This Architecture is Clean

  Your codebase follows the Single Responsibility Principle:
  ┌────────────────────┬──────────────────────────────┐
  │       Class        │           One Job            │
  ├────────────────────┼──────────────────────────────┤
  │ ConfigManager      │ Load/save configuration      │
  ├────────────────────┼──────────────────────────────┤
  │ RelayController    │ Control GPIO pins            │
  ├────────────────────┼──────────────────────────────┤
  │ ThingsBoardManager │ Handle network communication │
  ├────────────────────┼──────────────────────────────┤
  │ ConfigMenu         │ Serial user interface        │
  └────────────────────┴──────────────────────────────┘
  They communicate through references, not by knowing each other's internals. ThingsBoardManager calls relayController.setState() — it doesn't directly write to GPIO pins. If you later
  change how relays work internally, ThingsBoardManager doesn't need to change.

  // ThingsBoardManager doesn't know about GPIO
  relay.setState(index, value);     // just says "set relay ON"

  // RelayController handles the details internally
  digitalWrite(_pins[index], _activeHigh ? HIGH : LOW);
  _prefs.putUInt("rs", mask);

  That separation is what makes the code maintainable.
