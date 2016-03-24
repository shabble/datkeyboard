/* Buttons to USB Keyboard Example

   You must select Keyboard from the "Tools > USB Type" menu

   This example code is in the public domain.
 */

#include <Bounce2.h>

// 10 = 10 ms debounce time which is appropriate for most mechanical pushbuttons
#define BOUNCE_TIME 1
#define N_PINS 5

#define DEBUG(thing) if(debug){ Keyboard.print(thing); }
#define DEBUGLN(thing) if(debug){ Keyboard.println(thing); }

char mapping[5][5] = {
    {'z', 'b', 'c', 'd', 'e'},
    {'f', 'g', 'h', 'i', 'j'},
    {'k', 'l', 'm', 'n', 'o'},
    {'p', 'q', 'r', 's', 't'},
    {'u', 'v', 'w', 'x', 'y'},
};

bool debug = true;
bool backout_chord[5] = {true, true, true, true, true};

enum State {
    FIRST,
    SECOND
};

State current_state = FIRST;
bool pressed_run[N_PINS];
bool released_run[N_PINS];
int first_pressed, count = 0;

void reset_run(){
    for(int i = 0; i < N_PINS; i++){
        pressed_run[i] = false;
        released_run[i] = false;
    }
}

bool run_finished(){
    return array_equal(pressed_run, released_run);
}

bool array_equal(bool a[], bool b[]){
    for(int i = 0; i < N_PINS; i++){
        if(a[i] != b[i]){
            return false;
        }
    }
    return true;
}

int get_one_pressed(){
    for(int i = 0; i < N_PINS; i++){
        if(pressed_run[i]){
            return i;
        }
    }
    return -1;
}

// Create Bounce objects for each button.  The Bounce object
// automatically deals with contact chatter or "bounce", and
// it makes detecting changes very simple.
Bounce buttons[N_PINS];
void setup() {
    // Configure the pins for input mode with pullup resistors.
    // The pushbuttons connect from each pin to ground.  When
    // the button is pressed, the pin reads LOW because the button
    // shorts it to ground.  When released, the pin reads HIGH
    // because the pullup resistor connects to +5 volts inside
    // the chip.  LOW for "on", and HIGH for "off" may seem
    // backwards, but using the on-chip pullup resistors is very
    // convenient.  The scheme is called "active low", and it's
    // very commonly used in electronics... so much that the chip
    // has built-in pullup resistors!
    for(int i = 0; i < N_PINS; i++){
        buttons[i] = Bounce();
        buttons[i].attach(i, INPUT_PULLUP);
        buttons[i].interval(BOUNCE_TIME);
    }
    Keyboard.begin();
}

void press(int pin){
    switch(current_state){
        case FIRST:
            first_pressed = pin;
            current_state = SECOND;
            break;
        case SECOND:
            Keyboard.press(mapping[first_pressed][pin]);
            current_state = FIRST;
            break;
    }
    Keyboard.releaseAll();
}

void loop() {
    int pressed;
    bool dirty = false;
    DEBUG("Count: ");
    DEBUGLN(count);
    count++;
    // Update things
    for(int i = 0; i < N_PINS; i++){
        buttons[i].update();

        if (buttons[i].fell()) {
            dirty = true;
            pressed_run[i] = true;
            DEBUG("Pressed ");
            DEBUGLN(i);
        }else if(buttons[i].rose()){
            dirty = true;
            DEBUG("Released ");
            DEBUGLN(i);
            released_run[i] = true;
        }
    }

    // Check the state so we know when to do a thing
    if(dirty && run_finished()){
        // Check multi
        if(array_equal(pressed_run, backout_chord)){
            current_state = FIRST;
        }else{
            pressed = get_one_pressed();
            if(pressed > -1){
                press(pressed);
            }
        }
        reset_run();
    }

    for(int i = 0; i < N_PINS; i++){
        DEBUG(pressed_run[i]);
        DEBUG(' ');
    }
    DEBUGLN();
    for(int i = 0; i < N_PINS; i++){
        DEBUG(released_run[i]);
        DEBUG(' ');
    }
    DEBUGLN();
    DEBUGLN("========");
}
