#include "KitBounce.h"
#include <stdio.h>

// N_PINS must be at most half the bit-width of an int (so can't be >16 right now, AFAIK)
#define N_PINS 5

#define LED_PIN 13

#define RIGHT_SQUEEZE 0b01111
#define LEFT_SQUEEZE  0b11110
#define ALL_SQUEEZE   0b11111

#define DEBUG

#ifdef DEBUG
#define BUF_SIZE 128
char buffer[BUF_SIZE];
#define PRINT(stuff) Serial.print(stuff)
#define PRINTLN(stuff) Serial.println(stuff)
#define PRINTF(...) do { snprintf(buffer, BUF_SIZE, __VA_ARGS__); PRINT(buffer); } while(0)
#else
#define PRINT(stuff)
#define PRINTLN(stuff)
#define PRINTF(...)
#endif

const int mask = ~((~0) << N_PINS);

// For figuring out when to consider a real button (the actual physical ones) pressed, or when a chord has been pressed
enum ButtonsState {
    START,
    SINGLE_1,
    SINGLE_2,
    HOLDING,
    CHORDING,
    CHORD_1,
    CHORD_2,
    CHORD_HOLDING
};

// For figuring out when to emit a keyboard button press to the computer
enum KeyboardState {
    K_START,
    AWAIT_SECOND
};

const char mapping[N_PINS][N_PINS] = {
    {'a', 'b', 'c', 'd', 'e'},
    {'f', 'g', 'h', 'i', 'j'},
    {'k', 'l', 'm', 'n', 'o'},
    {'p', 'q', 'r', 's', 't'},
    {'u', 'v', 'w', 'x', 'y'},
};

// button_states:
//  Each entry is a byte, we treat each bit in that byte as whether the button was pressed at that moment in time (with
//  the rightmost bit being the current state)
uint8_t button_states[N_PINS];

// buttons_now:
//  A full int of history of the buttons, the rightmost N_PINS bits are the buttons right now
uint buttons_now = 0;

// buttons:
//  Array of debouncer objects
Bounce buttons[N_PINS];

// The two state machines we use.
ButtonsState buttons_state = START;
KeyboardState keyboard_state = K_START;


// High level idea behind buttons_state state machine:
// * If pressed for three "moments" and nothing else pressed, consider it held (until released)
//  - If it's released during the three moments, consider it pressed
// * If something else is pressed as well during these three moments, start building a chord
// * If the chord completes, do the chord action
// * If the chord releases without completing, act as though the buttons were pressed in order according to their
//   millisecond press times (which I'll need to fork the bounce library to allow us to access).

int count(){
    // How many buttons are currently pressed
    return __builtin_popcount(buttons_now & mask);
}

int which(){
    // Which button is currently pressed (lowest index).
    // -1 if nothing pressed
    return __builtin_ffs(buttons_now & mask) - 1;
}

void emit_1(int pin){
    if(pin < 0 || pin >= N_PINS){
        return;
    }
    // A real button has been considered pressed
    PRINTF("EMIT %d ", pin);

    static int first;
    switch(keyboard_state){
        case K_START:
            first = which();
            keyboard_state = AWAIT_SECOND;
            break;

        case AWAIT_SECOND:
            keyboard_state = K_START;
            Keyboard.press(lookup(first, which()));
            break;
    }
}

char lookup(int first, int second){
    // Get the virtual key to be pressed (the thing to send to the computer) from a pair of real button presses
    if(first >= N_PINS || first < 0){
        PRINTF("\r\nFirst pressed is wrong: %d\n", first);
    }
    if(second >= N_PINS || second < 0){
        PRINTF("\r\nSecond pressed is wrong: %d\n", second);
    }
    return mapping[first][second];
}

void clearSerial(){
    while(Serial.available() > 0){
        if(Serial.read() == 'r'){
            _reboot_Teensyduino_();
        }
    }
}

void setup(){
    #ifdef DEBUG
    Serial.begin(9600);
    while(!Serial){
        ;
    }
    #endif

    for(int i = 0; i < N_PINS; i++){
        // Initialize the debouncers
        buttons[i] = Bounce();
        buttons[i].attach(i, INPUT_PULLUP);
        buttons[i].interval(1);

        // Set all the button states up
        button_states[i] = 0;
    }

    buttons_state = START;
    keyboard_state = K_START;

    // If we ever want to use it
    pinMode(LED_PIN, OUTPUT);

    Keyboard.begin();

    PRINTF("|%20s|%20s| %6s | %6s | %6s | ", "Buttons State", "Keyboard State", "pin", "nbits", "bnm");
}

void loop(){
    clearSerial();
    digitalWrite(LED_PIN, keyboard_state == AWAIT_SECOND);

    // Update button states
    for(int i = N_PINS - 1; i >= 0; i--){ // Downwards because we're pushing backwards onto buttons_now
        buttons[i].update();

        // We use the button states as an 8-iteration-long history of the button
        button_states[i] <<= 1;
        button_states[i] |= !buttons[i].read(); // ! because pullup

        buttons_now <<= 1;
        buttons_now |= !buttons[i].read();

    }

    int nbits = count();
    int bnm = buttons_now & mask;

    #ifdef DEBUG
    // Debug stuff
    char bs_buf[20];
    switch(buttons_state){
        case START:
            strcpy(bs_buf, "START");
            break;
        case SINGLE_1:
            strcpy(bs_buf, "SINGLE_1");
            break;
        case SINGLE_2:
            strcpy(bs_buf, "SINGLE_2");
            break;
        case HOLDING:
            strcpy(bs_buf, "HOLDING");
            break;
        case CHORDING:
            strcpy(bs_buf, "CHORDING");
            break;
        case CHORD_1:
            strcpy(bs_buf, "CHORD_1");
            break;
        case CHORD_2:
            strcpy(bs_buf, "CHORD_2");
            break;
        case CHORD_HOLDING:
            strcpy(bs_buf, "CHORD_HOLDING");
            break;
    }

    char ks_buf[20];
    switch(keyboard_state){
        case K_START:
            strcpy(ks_buf, "K_START");
            break;
        case AWAIT_SECOND:
            strcpy(ks_buf, "AWAIT_SECOND");
            break;
    }

    static char last_buffer[BUF_SIZE] = "";
    static char current_buffer[BUF_SIZE] = "";

    if(strcmp(last_buffer, current_buffer) == 0){
        PRINT("\r");
    }else{
        PRINT("\r\n");
    }

    strncpy(last_buffer, current_buffer, BUF_SIZE);
    PRINTF("|%20s|%20s| % 6d | %6u | %6u | ", bs_buf, ks_buf, which(), nbits, bnm);
    strncpy(current_buffer, buffer, BUF_SIZE);

    // </debug stuff>
    #endif

    static ButtonsState last_bs = START;
    bool state_changed = last_bs == buttons_state;
    last_bs = buttons_state;
    // Use the button states
    static int trying_chord;
    switch(buttons_state){
        case START:
            if(state_changed){
                PRINTF("Clean slate");
            }
            Keyboard.releaseAll();

            if(nbits == 1){
                // Only one button has been pressed, none were pressed before, we can just move to the next state
                buttons_state = SINGLE_1;
            }else if(nbits > 2){
                buttons_state = CHORDING;
            } // Otherwise just stay in the START state
            break;
        case SINGLE_1:
            if(nbits == 1){
                // Count 1 *might* mean a different pin; hard to physically do but not impossible.
                uint8_t pressed_history = button_states[which()];
                if((pressed_history & 0b11) == 0b11){ // It was this button last time too
                    buttons_state = SINGLE_2;
                }else{
                    emit_1(which());
                }
            }else if(nbits > 1){
                buttons_state = CHORDING;
            }else{
                // We released it, meaning time to emit a press
                emit_1(which());
                buttons_state = START;
            }
            break;
        case SINGLE_2:
            if(nbits == 1){
                uint8_t pressed_history = button_states[which()];
                if((pressed_history & 0b11111) == 0b11111){
                    emit_1(which());
                    buttons_state = HOLDING;
                }
            }else if(nbits > 1){
                buttons_state = CHORDING;
            }else{
                emit_1(which());
                buttons_state = START;
            }
            break;
        case HOLDING:
            if(nbits < 1){
                buttons_state = START;
            }
            break;
        case CHORDING:
            if(nbits < 1){
                buttons_state = START;
            }else if(bnm == RIGHT_SQUEEZE){
                buttons_state = CHORD_1;
                trying_chord = RIGHT_SQUEEZE;
            }else if(bnm == LEFT_SQUEEZE){
                buttons_state = CHORD_1;
                trying_chord = LEFT_SQUEEZE;
            }else if(bnm == ALL_SQUEEZE){
                buttons_state = CHORD_1;
                trying_chord = ALL_SQUEEZE;
            }
            break;
        case CHORD_1:
            if(bnm == trying_chord){
                buttons_state = CHORD_2;
            }else{
                buttons_state = CHORDING;
            }
            break;
        case CHORD_2:
            if(bnm == trying_chord){
                buttons_state = CHORD_HOLDING;
                switch(bnm){
                    case RIGHT_SQUEEZE:
                        PRINTF("Chord Spacebar ");
                        Keyboard.press(' ');
                        break;
                    case LEFT_SQUEEZE:
                        PRINTF("Chord Backspace ");
                        Keyboard.press(KEY_BACKSPACE);
                        break;
                    case ALL_SQUEEZE:
                        PRINTF("Chord Cancel ");
                        keyboard_state = K_START;
                        break;
                }
            }else{
                buttons_state = CHORDING;
            }
            break;
        case CHORD_HOLDING:
            if(nbits == 0){
                buttons_state = START;
            }
            break;
    }
    // 200Hz because it seems to work fine
    delay(10);
}
