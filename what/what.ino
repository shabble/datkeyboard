#include <Bounce2.h>

// N_PINS must be at most half the bit-width of an int (so can't be >16 right now, AFAIK)
#define N_PINS 5

#define RIGHT_SQUEEZE 0b01111
#define LEFT_SQUEEZE  0b11110
#define ALL_SQUEEZE   0b11111

const int mask = ~((~0) << N_PINS);

// For figuring out when to emit a button press internally (because the internal model has a state machine of its own)
enum ButtonsState {
    START,
    SINGLE_1,
    SINGLE_2,
    HOLDING,
    CHORDING,
    CHORD_HOLDING
};

// For figuring out when to emit a keyboard button press to the computer after internal buttons are pressed
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

uint8_t button_states[N_PINS];
uint buttons_now = 0;
const int led_pin = 13;

Bounce buttons[N_PINS];

ButtonsState buttons_state = START;
KeyboardState keyboard_state = K_START;


void setup(){
    for(int i = 0; i < N_PINS; i++){
        buttons[i] = Bounce();
        buttons[i].attach(i, INPUT_PULLUP);
        buttons[i].interval(1);
        button_states[i] = 0;
    }

    pinMode(led_pin, OUTPUT);

    Keyboard.begin();
    Serial.begin(9600);
}

// Plan as of going to bed:
// * Sort by millis for monotonicity
// * Figure out how to do something clever to quickly output a button press if just press, but also make the 3 chords
//   easy

// Properties this should have:
// * If pressed for three "moments" and nothing else pressed, consider it pressed
// * Otherwise start building up a chord
// * If the chord completes, do the chord action
// * If the chord releases without completing, act as though the buttons were pressed in order according to their
//   millisecond press times.

// Use a damn state machine, idiot.

bool on_at(int pin, int n){
    // Was pin `i` on `n` iterations ago
    return (button_states[pin] >> n) & 1;
}

int count(){
    return __builtin_popcount(buttons_now & mask);
}

int which(){
    return __builtin_ffs(buttons_now & mask) - 1;
}

void emit_1(int pin){
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
    return mapping[first][second];
}

void loop(){
    // Update button states
    for(int i = N_PINS - 1; i >= 0; i--){ // Downwards because we're pushing backwards onto buttons_now
        buttons[i].update();

        // We use the button states as an 8-iteration-long history of the button
        button_states[i] <<= 1;
        button_states[i] |= !buttons[i].read(); // ! because pullup

        buttons_now <<= 1;
        buttons_now |= !buttons[i].read();

    }

    static ButtonsState last_state = START;
    static int last_nbits = 0;
    static int last_which = -1;
    static uint last_bn = 0;
    static KeyboardState last_ks = K_START;

    if(buttons_state == last_state &&
       last_nbits == count() &&
       last_which == which() &&
       last_bn == buttons_now &&
       last_ks == keyboard_state){
        Serial.write('\r');
    }else{
        Serial.write("\r\n");
    }
    last_state = buttons_state;
    last_nbits = count();
    last_which = which();
    last_bn    = buttons_now;
    last_ks    = keyboard_state;


    // Use the button states
    int nbits = count();
    int bnm = buttons_now & mask;
    switch(buttons_state){
        case START:
            Serial.print("START   ");
            Keyboard.releaseAll();

            if(nbits == 1){
                // Only one button has been pressed, none were pressed before, we can just move to the next state
                buttons_state = SINGLE_1;
            }else if(nbits > 2){
                buttons_state = CHORDING;
            } // Otherwise just stay in the START state
            break;
        case SINGLE_1:
            Serial.print("SINGLE_1");
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
            Serial.print("SINGLE_2");
            if(nbits == 1){
                uint8_t pressed_history = button_states[which()];
                if((pressed_history & 0b111) == 0b111){
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
            Serial.print("HOLDING ");
            if(nbits != 1){
                buttons_state = START;
            }
            break;
        case CHORDING:
            Serial.print("CHORDING");
            if(nbits < 1){
                buttons_state = START;
            }else if(bnm == RIGHT_SQUEEZE){
                Keyboard.press(' ');
                buttons_state = CHORD_HOLDING;
            }else if(bnm == LEFT_SQUEEZE){
                Keyboard.press(KEY_BACKSPACE);
                buttons_state = CHORD_HOLDING;
            }else if(bnm == ALL_SQUEEZE){
                keyboard_state = K_START;
                buttons_state = CHORD_HOLDING;
            }
            break;
        case CHORD_HOLDING:
            Serial.print("CHORD_HOLDING");
            if(nbits == 0){
                buttons_state = START;
            }
            break;
    }

    Serial.print(" ");
    switch(keyboard_state){
        case K_START:
            Serial.print("K_START");
            break;
        case AWAIT_SECOND:
            Serial.print("AWAIT_SECOND");
            break;
    }

    Serial.print(" ");
    Serial.print(which());
    Serial.print(" ");
    Serial.print(nbits);
    Serial.print(" ");
    Serial.print(bnm);

    delay(5);
}
