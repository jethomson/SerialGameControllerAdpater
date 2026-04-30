#include <Bluepad32.h>
#include <stdint.h>
#include "ControllerConfig.h"

#define LED_PIN 2

// This code uses the same input device naming scheme as used in Bluepad32
// Controller: A generic input device. Could be a gamepad, mouse, keyboard, or special controller.
// Gamepad: A specific type of controller with buttons, triggers, sticks, etc.
// The distinction is made for this code because a bluetooth keyboard can be used as an input device.


// Face Buttons / Action Buttons mappings to NES
// for example,  SNES (ClockWise): X, A, B, Y | PlayStation (CW): Triangle, Circle, Cross, Square | Xbox (CW): Y, B, X, A
// generally (CW): North, East, South, West    # not to be confused with directional pad: Up, Right, Down, Left
//  West --> NES B
// South --> NES A


#undef DEBUG_CONSOLE
//#define DEBUG_CONSOLE Serial
#if defined DEBUG_CONSOLE && !defined DEBUG_PRINTLN
  #define DEBUG_BEGIN(x)     DEBUG_CONSOLE.begin (x)
  #define DEBUG_PRINT(x)     DEBUG_CONSOLE.print (x)
  #define DEBUG_PRINTDEC(x)     DEBUG_PRINT (x, DEC)
  #define DEBUG_PRINTLN(x)  DEBUG_CONSOLE.println (x)
  #define DEBUG_PRINTF(...) DEBUG_CONSOLE.printf(__VA_ARGS__)
  #define DEBUG_FLUSH()     DEBUG_CONSOLE.flush ()
#else
  #define DEBUG_BEGIN(x)
  #define DEBUG_PRINT(x)
  #define DEBUG_PRINTDEC(x)
  #define DEBUG_PRINTLN(x)
  #define DEBUG_PRINTF(...)
  #define DEBUG_FLUSH()
#endif


enum ControllerType : uint8_t {
    CTL_NC = 255,  // no input device, always outputs 0x00 so code operates properly when a controller is not connected.
    GP_DIY = 0,    // custom input device
    GP_NES = 1,    // wired NES controller
    GP_SNES = 2,   // wired SNES controller
    GP_PSX = 3,    // wired playstation controller using playstation connector
    // bluetooth gamepad and bluetooth keyboard are combined to simplify the potential UI
    CTL_BLUETOOTH_CONTROLLER = 5
};

enum Buttons : uint8_t {
    A = (1 << 0), // A Button
    B = (1 << 1), // B Button
    Select = (1 << 2), // Select Button
    Start = (1 << 3), // Start Button
    Up = (1 << 4), // Up Button
    Down = (1 << 5), // Down Button
    Left = (1 << 6), // Left Button
    Right = (1 << 7)  // Right Button
};


ControllerPtr ctl;

volatile uint8_t buttons_state = 0x00;
uint8_t (*readController)() = nullptr;


void initController(ControllerType controller_type) {
    switch (controller_type) {
        case GP_DIY:
            // ESP32 board has boot loop problem
            // turning the LED on indicates the code is running properly
            pinMode(DIY_A_BUTTON, INPUT_PULLUP);
            pinMode(DIY_B_BUTTON, INPUT_PULLUP);
            pinMode(DIY_LEFT_BUTTON, INPUT_PULLUP);
            pinMode(DIY_RIGHT_BUTTON, INPUT_PULLUP);
            pinMode(DIY_UP_BUTTON, INPUT_PULLUP);
            pinMode(DIY_DOWN_BUTTON, INPUT_PULLUP);
            pinMode(DIY_START_BUTTON, INPUT_PULLUP);
            pinMode(DIY_SELECT_BUTTON, INPUT_PULLUP);
            readController = readGpioGamepad;
            break;
        case GP_NES:
            pinMode(GAMEPAD_NES_CLK, OUTPUT);
            pinMode(GAMEPAD_NES_LATCH, OUTPUT);
            pinMode(GAMEPAD_NES_DATA, INPUT_PULLUP);
            readController = readNesGamepad;
            break;

        case GP_SNES:
            pinMode(GAMEPAD_SNES_CLK, OUTPUT);
            pinMode(GAMEPAD_SNES_LATCH, OUTPUT);
            pinMode(GAMEPAD_SNES_DATA, INPUT_PULLUP);
            readController = readSnesGamepad;
            break;

        case GP_PSX:
            pinMode(GAMEPAD_PSX_DATA, INPUT_PULLUP);
            pinMode(GAMEPAD_PSX_COMMAND, OUTPUT);
            pinMode(GAMEPAD_PSX_ATTENTION, OUTPUT);
            pinMode(GAMEPAD_PSX_CLK, OUTPUT);

            digitalWrite(GAMEPAD_PSX_ATTENTION, HIGH);
            digitalWrite(GAMEPAD_PSX_CLK, HIGH);
            delayMicroseconds(10);

            // Dummy transfer bytes to clean internal controller state
            for (int i = 0; i < 2; i++) {
                digitalWrite(GAMEPAD_PSX_ATTENTION, LOW);
                delayMicroseconds(10);

                transferPsxByte(0);
                delayMicroseconds(10);

                digitalWrite(GAMEPAD_PSX_ATTENTION, HIGH);
                delayMicroseconds(12);
            }
            readController = readPsxGamepad;
            break;

        case CTL_BLUETOOTH_CONTROLLER:
            BP32.setup(&onConnectedController, &onDisconnectedController);
            BP32.forgetBluetoothKeys();
            // Enables mouse / touchpad support for gamepads that support them.
            BP32.enableVirtualDevice(false);

            readController = readBluetoothController;
            break;

        case CTL_NC:
        default:
            // catch all includes NO_CONTROLLER (255)
            readController = readDummyController;
            break;
    }
}


void indicateRead() {
#ifdef LED_PIN
    digitalWrite(LED_PIN, HIGH);
#endif
}


// This is useful where user wants to see the board running before getting a controller wired up.
uint8_t readDummyController() {
    return 0x00; // no buttons pressed
}


uint8_t readGpioGamepad() {
    indicateRead();

    uint8_t state = 0x00;
    if (digitalRead(DIY_A_BUTTON)      == LOW) state |= Buttons::A;
    if (digitalRead(DIY_B_BUTTON)      == LOW) state |= Buttons::B;
    if (digitalRead(DIY_SELECT_BUTTON) == LOW) state |= Buttons::Select;
    if (digitalRead(DIY_START_BUTTON)  == LOW) state |= Buttons::Start;
    if (digitalRead(DIY_UP_BUTTON)     == LOW) state |= Buttons::Up;
    if (digitalRead(DIY_DOWN_BUTTON)   == LOW) state |= Buttons::Down;
    if (digitalRead(DIY_LEFT_BUTTON)   == LOW) state |= Buttons::Left;
    if (digitalRead(DIY_RIGHT_BUTTON)  == LOW) state |= Buttons::Right;

    return state;
}


uint8_t readNesGamepad() {
    indicateRead();

    uint8_t state = 0x00;
    digitalWrite(GAMEPAD_NES_LATCH, HIGH);
    delayMicroseconds(12);
    digitalWrite(GAMEPAD_NES_LATCH, LOW);
    delayMicroseconds(6);

    for (int i = 0; i < 8; i++) {
        if (digitalRead(GAMEPAD_NES_DATA) == LOW) state |= (1 << i);
        digitalWrite(GAMEPAD_NES_CLK, LOW);
        delayMicroseconds(6);
        digitalWrite(GAMEPAD_NES_CLK, HIGH);
        delayMicroseconds(6);
    }

    return state;
}


uint8_t readSnesGamepad() {
    indicateRead();

    // SNES bits
    // 0 - B
    // 1 - Y
    // 2 - Select
    // 3 - Start
    // 4 - Up
    // 5 - Down
    // 6 - Left
    // 7 - Right
    // 8 - A
    // 9 - X
    // 10 - L
    // 11 - R

    uint8_t state = 0x00;
    uint16_t snes_state = 0x0000;
    digitalWrite(GAMEPAD_SNES_LATCH, HIGH);
    delayMicroseconds(12);
    digitalWrite(GAMEPAD_SNES_LATCH, LOW);
    delayMicroseconds(6);

    for (int i = 0; i < 12; i++)
    {
        if (digitalRead(GAMEPAD_SNES_DATA) == LOW) snes_state |= (1 << i);
        digitalWrite(GAMEPAD_SNES_CLK, LOW);
        delayMicroseconds(6);
        digitalWrite(GAMEPAD_SNES_CLK, HIGH);
        delayMicroseconds(6);
    }

    // NES compatible bits
    state |= snes_state & 0xFF;

    // Map extra bits to A and B buttons
    if (snes_state & (1 << 8)) state |= Buttons::A;
    if (snes_state & (1 << 9)) state |= Buttons::B;
    if (snes_state & (1 << 10)) state |= Buttons::B;
    if (snes_state & (1 << 11)) state |= Buttons::A;

    return state;
}


uint8_t readPsxGamepad() {
    indicateRead();

    // Communication Protocol
    // First three bytes - Header
    // Following bytes - Digital Mode (2 bytes) / Analog Mode (18 bytes)

    // First byte
    // Command: 0x01 (indicates new packet)
    // Data: 0xFF

    // Second byte
    // Command: Main command (poll or configure controller)
    //          Polling: 0x42
    // Data: Device Mode
    //       Upper 4 bits: mode (4 = digital, 7 = analog, F = config)
    //       Lower 4 bits: how many 16 bit words follow the header

    // Third byte
    // Command : 0x00
    // Data: 0x5A

    // Button Mappings
    // Digital Mode
    // 0 - Select
    // 1 - L3
    // 2 - R3
    // 3 - Start
    // 4 - Up
    // 5 - Right
    // 6 - Down
    // 7 - Left
    // 8 - L2
    // 9 - R2
    // 10 - L1
    // 11 - R1
    // 12 - Triangle
    // 13 - O
    // 14 - X
    // 15 - Square

    // Analog Mode
    // - Analog sticks range 0x00 - 0xFF, 0x7F at rest
    // - Pressure buttons range 0x00 - 0xFF, 0xFF is fully pressed
    // 0-15 - Same as digital mode
    // Byte 2 - RX
    // Byte 3 - RY
    // Byte 4 - LX
    // Byte 5 - LY
    // Byte 6 - Right
    // Byte 7 - Left
    // Byte 8 - Up
    // Byte 9 - Down
    // Byte 10 - Triangle
    // Byte 11 - O
    // Byte 12 - X
    // Byte 13 - Square
    // Byte 14 - L1
    // Byte 15 - R1
    // Byte 16 - L2
    // Byte 17 - R2

    int b1, b2;
    uint8_t state = 0x00; 
    uint16_t psx_state = 0x0000;

    // Initiate transfer
    delayMicroseconds(2);
    digitalWrite(GAMEPAD_PSX_ATTENTION, LOW);

    transferPsxByte(0x01);
    transferPsxByte(0x42); 
    transferPsxByte(0xFF); 
    b1 = transferPsxByte(0xFF);
    b2 = transferPsxByte(0xFF);

    psx_state = (b2 << 8) | b1;

    // Map PSX bits to NES bits
    constexpr uint16_t PSX_SELECT = (1 << 0);
    constexpr uint16_t PSX_START  = (1 << 3);
    constexpr uint16_t PSX_A_MASK =
        (1 << 11) | // R1
        (1 << 9)  | // R2
        (1 << 2)  | // R3
        (1 << 14) | // X
        (1 << 13);  // O
    constexpr uint16_t PSX_B_MASK =
        (1 << 10) | // L1
        (1 << 8)  | // L2
        (1 << 1)  | // L3
        (1 << 15) | // Square
        (1 << 12);  // Triangle
    constexpr uint16_t PSX_UP    = (1 << 4);
    constexpr uint16_t PSX_DOWN  = (1 << 6);
    constexpr uint16_t PSX_LEFT  = (1 << 7);
    constexpr uint16_t PSX_RIGHT = (1 << 5);

    if (psx_state & PSX_SELECT) state |= Buttons::Select;
    if (psx_state & PSX_START) state |= Buttons::Start;

    if (psx_state & PSX_A_MASK) state |= Buttons::A;
    if (psx_state & PSX_B_MASK) state |= Buttons::B;

    if (psx_state & PSX_UP) state |= Buttons::Up;
    if (psx_state & PSX_DOWN) state |= Buttons::Down;
    if (psx_state & PSX_LEFT) state |= Buttons::Left;
    if (psx_state & PSX_RIGHT) state |= Buttons::Right;

    // End transfer
    digitalWrite(GAMEPAD_PSX_ATTENTION, HIGH);   

    return state;
}


uint8_t transferPsxByte(uint8_t byte) {
    uint8_t temp = 0;
    for (int i = 0; i < 8; i++) {
        digitalWrite(GAMEPAD_PSX_COMMAND, (byte >> i) & 1);    

        digitalWrite(GAMEPAD_PSX_CLK, LOW);
        delayMicroseconds(10); // allow some settling time

        // data should be read while low, not after returning high
        if (digitalRead(GAMEPAD_PSX_DATA) == LOW) temp |= (1 << i);

        digitalWrite(GAMEPAD_PSX_CLK, HIGH);
        delayMicroseconds(10);
    }

    return temp;
}


uint8_t readBluetoothController() {
    // tested working for Xbox Series X|S and PS5 controllers
    // tested working on Onn bluetooth keyboard
    uint8_t state = 0x00;
    BP32.update();

    if ( ctl && ctl->isConnected() ) {
        // checking ctl->hasData() causes problems for the Xbox controller at a read frequency of 59.94 Hz.
        // it returns false sometimes which causes a state of 0x00 to be returned resulting in jittery buttons.
        // PS5 controller does not have this issue.
        // checking ctl->hasData() causes problems for a keyboard possibly because it only reports
        // when the key is first pressed and when it is released, not when it is held.

        // putting indicateRead() after the isConnected() check helps to show bluetooth connection is established
        // onConnectedController() is not a reliable place to turn on the indicator becasue if controllerSelect()
        // turns off the indicator onConnectedController() will not be called again on an already established connection
        if ( ctl->isGamepad() ) {
            indicateRead();

            if (ctl->a())      state |= Buttons::A;
            if (ctl->x())      state |= Buttons::B;

            if (ctl->b())      state |= Buttons::A;
            if (ctl->y())      state |= Buttons::B;

            if (ctl->miscButtons() & 0x2) state |= Buttons::Select;
            if (ctl->miscButtons() & 0x4)  state |= Buttons::Start;

            if (ctl->dpad() & DPAD_UP)    state |= Buttons::Up;
            if (ctl->dpad() & DPAD_DOWN)  state |= Buttons::Down;
            if (ctl->dpad() & DPAD_LEFT)  state |= Buttons::Left;
            if (ctl->dpad() & DPAD_RIGHT) state |= Buttons::Right;

            // Analog stick fallback for D-pad
            int x = ctl->axisX();
            int y = ctl->axisY();

            if (y < -200) state |= Buttons::Up;
            if (y > 200)  state |= Buttons::Down;
            if (x < -200) state |= Buttons::Left;
            if (x > 200)  state |= Buttons::Right;
        }
        else if ( ctl->isKeyboard() ) {
            indicateRead();

            if (ctl->isKeyPressed(Keyboard_K)) state |= Buttons::A;
            if (ctl->isKeyPressed(Keyboard_J)) state |= Buttons::B;

            if (ctl->isKeyPressed(Keyboard_RightShift)) state |= Buttons::Select;
            if (ctl->isKeyPressed(Keyboard_Enter))      state |= Buttons::Start;

            if (ctl->isKeyPressed(Keyboard_W)) state |= Buttons::Up;
            if (ctl->isKeyPressed(Keyboard_S)) state |= Buttons::Down;
            if (ctl->isKeyPressed(Keyboard_A)) state |= Buttons::Left;
            if (ctl->isKeyPressed(Keyboard_D)) state |= Buttons::Right;

            if (ctl->isKeyPressed(Keyboard_UpArrow)) state |= Buttons::Up;
            if (ctl->isKeyPressed(Keyboard_DownArrow)) state |= Buttons::Down;
            if (ctl->isKeyPressed(Keyboard_LeftArrow)) state |= Buttons::Left;
            if (ctl->isKeyPressed(Keyboard_RightArrow)) state |= Buttons::Right;
        }

    }

    return state;
}


void onConnectedController(ControllerPtr c) {
  ctl = c;
  // it is better to use indicateRead() to show controller is connected elsewhere
  // instead of in this function
  DEBUG_PRINTLN("Controller connected!");
}

void onDisconnectedController(ControllerPtr c) {
  if (ctl == c) {
    digitalWrite(LED_PIN, LOW);
    ctl = nullptr;
    DEBUG_PRINTLN("Controller disconnected!");
  }
}


void debugPrintButtons(uint8_t state) {
    String line = "";

    if (state & Buttons::A)      line += "A ";
    if (state & Buttons::B)      line += "B ";
    if (state & Buttons::Select) line += "Select ";
    if (state & Buttons::Start)  line += "Start ";
    if (state & Buttons::Up)     line += "Up ";
    if (state & Buttons::Down)   line += "Down ";
    if (state & Buttons::Left)   line += "Left ";
    if (state & Buttons::Right)  line += "Right ";

    if (line.length() == 0)
        line = "--------";

    DEBUG_PRINTLN(line);
}


void controllerSelect() {
    int b = Serial1.read(); // returns -1 if buffer is empty
    if (CTL_NC <= b && b <= CTL_BLUETOOTH_CONTROLLER) {
        for (int i = 0; i < 3; i++) {
            digitalWrite(LED_PIN, LOW);
            delay(100);
            digitalWrite(LED_PIN, HIGH);
            delay(100);
        }
        digitalWrite(LED_PIN, LOW);
        initController((ControllerType)b);
    }
}


void setup() {
    Serial1.begin(115200, SERIAL_8N1, CONTROLLER_UART_RX, CONTROLLER_UART_TX);

    //DEBUG_BEGIN(74880);
    DEBUG_BEGIN(115200);
#if defined DEBUG_CONSOLE
    delay(2000); // Wait for Serial Monitor to connect
#endif
    DEBUG_PRINT("\n\nSerial output started\n\n");
    DEBUG_FLUSH();

    DEBUG_PRINTF("Firmware: %s\n", BP32.firmwareVersion());
    const uint8_t* addr = BP32.localBdAddress();
    DEBUG_PRINTF("BD Addr: %2X:%2X:%2X:%2X:%2X:%2X\n", addr[0], addr[1], addr[2], addr[3], addr[4], addr[5]);


    DEBUG_PRINTLN("\n--- ESP32 Reset Reason ---");
    esp_reset_reason_t reason = esp_reset_reason();
    DEBUG_PRINT("Reason Code: ");
    DEBUG_PRINTLN(reason);
    switch (reason) {
        case ESP_RST_POWERON:   DEBUG_PRINTLN("POWER ON: Normal startup (Power plugged in or RST button)"); break;
        case ESP_RST_EXT:       DEBUG_PRINTLN("EXTERNAL PIN: Reset by external pin (rarely used)"); break;
        case ESP_RST_SW:        DEBUG_PRINTLN("SOFTWARE: Reset via ESP.restart()"); break;
        case ESP_RST_PANIC:     DEBUG_PRINTLN("CRASH/PANIC: Fatal error (exception/panic)"); break;
        case ESP_RST_INT_WDT:   DEBUG_PRINTLN("WDT INT: Software or hardware Interrupt Watchdog timeout"); break;
        case ESP_RST_TASK_WDT:  DEBUG_PRINTLN("WDT TASK: Task Watchdog timeout (stuck in loop)"); break;
        case ESP_RST_WDT:       DEBUG_PRINTLN("WDT OTHER: Other Watchdog timeout"); break;
        case ESP_RST_DEEPSLEEP: DEBUG_PRINTLN("DEEP SLEEP: Reset after exiting deep sleep mode"); break;
        case ESP_RST_BROWNOUT:  DEBUG_PRINTLN("BROWNOUT: Power supply voltage too low (hardware issue)"); break;
        case ESP_RST_SDIO:      DEBUG_PRINTLN("SDIO: Reset over SDIO"); break;
        default:                DEBUG_PRINTLN("UNKNOWN: Reason could not be determined"); break;
    }

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

#ifndef DEFAULT_CONTROLLER_TYPE
    initController((ControllerType)CTL_NC);
#else
    initController((ControllerType)DEFAULT_CONTROLLER_TYPE);
#endif
}


void loop() {
    const uint32_t period = 1000000 / 59.94; // microseconds
    const uint8_t no_change_limit = (100000 / period) + 0.5; // number of periods in 100 ms, rounded
    static uint32_t pm = 0; // previous micros
    static uint8_t no_change_cnt = 0;

    if ( (micros() - pm) >= period ) {
        pm += period; 

#ifndef DEFAULT_CONTROLLER_TYPE
        controllerSelect(); // calling this in the main loop allows for controller to be selected on the fly
#endif

        buttons_state = readController();
        static uint8_t prev_buttons_state = 0x00;
        if (prev_buttons_state != buttons_state || no_change_cnt >= no_change_limit) {
            // send buttons state on change or every about every 100 ms
            // sending on change keeps from flooding the serial connection
            // send about every 100 ms is a sort of keep alive and guards against lost transmissions
            prev_buttons_state = buttons_state;
            no_change_cnt = 0;
            Serial1.write(buttons_state);
#if defined DEBUG_CONSOLE
            debugPrintButtons(buttons_state);
#endif
        }
        else {
            no_change_cnt++;
        }

    }
}
