#include <stdio.h>
#include "main.h"
#include "stm32h5xx_it.h"
#include "Adafruit_ST7735_Macros.h"
#include "Adafruit_TFTShield18_Macros.h"
#include "dwt_module.h"
#include "Adafruit_TFTShield18_API.h"
#include "Adafruit_ST7735_API.h"
#include "stm32h5xx_hal.h"

#include "buzzer.h"
#include "stm32h5xx_hal_tim.h"
#include <stdbool.h>

// display:
// left port middle row -> 9th pin from the right in the top row of CN10
// right port middle row -> 2nd pin from the right in the top row of CN7

// PORT: GPIOA, GPIOB, GPIOC
// PIN: PIN NUMBER (0-15)
// LEDS: PC12, PC 10, PC0, PC2
#define LED_YELLOW_PIN GPIO_PIN_12
#define LED_YELLOW_PORT GPIOC

#define LED_BLUE_PIN GPIO_PIN_10 
#define LED_BLUE_PORT GPIOC

#define LED_RED_PIN GPIO_PIN_0 
#define LED_RED_PORT GPIOC

#define LED_GREEN_PIN GPIO_PIN_2 
#define LED_GREEN_PORT GPIOC

// BUTTONS: PC3, PC5, PC11, PB8
#define BUTTON_YELLOW_PIN GPIO_PIN_3 
#define BUTTON_YELLOW_PORT GPIOC

#define BUTTON_BLUE_PIN GPIO_PIN_5 
#define BUTTON_BLUE_PORT GPIOC

#define BUTTON_RED_PIN GPIO_PIN_11 
#define BUTTON_RED_PORT GPIOC

#define BUTTON_GREEN_PIN GPIO_PIN_8 
#define BUTTON_GREEN_PORT GPIOB

// BUZZER: PA8
#define BUZZER_PIN GPIO_PIN_8
#define BUZZER_PORT GPIOA

// Define the states of the Simon game
typedef enum{
    IDLE,
    COUNTDOWN,
    PLAY_SEQUENCE,
    PLAYER_INPUT,
    GAME_WIN,
    GAME_LOSE
} State_t;

// static variable to keep track of the state
static State_t currentState = IDLE;
// tracks if the player has successfully completed the current level
static bool levelSuccess = false; 
// Flag to indicate if the display needs to be updated
static bool updateDisplay = true; 
//4 buttons, 0 = yellow, 1 = blue, 2 = red, 3 = green
static bool buttonPressed[4] = {false, false, false, false}; // Track button presses for each color  
#define NUM_BTNS 4 

#define MAX_SEQ 5 //maximum 32 flashes per level, we can have 32 levels so it increases by 1 each time
#define NOT_PRESSED 255 //no btn pressed

// need a sequence for game, sequence for user inupt, current level, current user input, current color, last button pressed
uint8_t COLOR_SEQUENCE[MAX_SEQ]; // sequence of button presses for the game
uint8_t USER_SEQUENCE[MAX_SEQ]; // sequence of button presses for the player

#define HISTORY_SIZE 5

uint8_t BUTTON_HISTORY[HISTORY_SIZE];
uint8_t HISTORY_COUNTER = 0;

uint8_t CURRENT_LEVEL = 0; // current level of the game -- also representing how long the sequence will get for this lvl
uint8_t CURRENT_INPUT = 0; // index of the current button press in the player's input
uint8_t CURRENT_COLOR = 0; // current color being displayed
uint8_t CURRENT_LENGTH = 0; // current length of the sequence of the current level

uint8_t LAST_BTN = NOT_PRESSED; // last button that was pressed -> starts at 255 bc max val for 8-bit unsigned int so it won't correspond to a color button
bool WAIT_FOR_RELEASE = false; //wait after a button press until it's let go

bool FIRST_LOOP = true; //first loop after entering a state
uint8_t PLAYBACK_INDEX = 0; //index of the current btn on the playback

uint16_t SEQ_TIME = 200; //how long the btn color stays up during playback 

#define DEBOUNCE_TIME 20 //how long to wait after btn release
#define SUCCESS_TIME 1000 // how long success message stays up

static GPIO_TypeDef* LED_PORTS[NUM_BTNS] = {LED_YELLOW_PORT, LED_BLUE_PORT, LED_RED_PORT, LED_GREEN_PORT}; //array of led ports
static const uint16_t LED_PINS[NUM_BTNS] = {LED_YELLOW_PIN, LED_BLUE_PIN, LED_RED_PIN, LED_GREEN_PIN}; //array of led pins

static GPIO_TypeDef* BUTTON_PORTS[NUM_BTNS] = {BUTTON_YELLOW_PORT, BUTTON_BLUE_PORT, BUTTON_RED_PORT, BUTTON_GREEN_PORT}; //array of button ports
static const uint16_t BUTTON_PINS[NUM_BTNS] = {BUTTON_YELLOW_PIN, BUTTON_BLUE_PIN, BUTTON_RED_PIN, BUTTON_GREEN_PIN}; //array of button pins

extern Buzzer buzzer1; //extern buzzer struct defined in main.c
extern RNG_HandleTypeDef hrng;

#define MODER_MASK(n) (GPIO_MODER_MODE0   << (2 * (n))) //2-bit mode field for pin n
#define MODER_OUT(n) (GPIO_MODER_MODE0_0 << (2 * (n))) //that field set to 01 = output
#define PUPDR_MASK(n) (GPIO_PUPDR_PUPD0   << (2 * (n))) //2-bit pull field for pin n
#define PUPDR_PU(n) (GPIO_PUPDR_PUPD0_1 << (2 * (n))) //that field set to 10 = pull-down
#define PIN_NUM(mask) (__builtin_ctz(mask)) //pin n's bit in ODR or IDR
#define BUZZER_OCTAVE 2 //buzzer octave

// helper functions
// Adds random colors to the game sequence
static void addToSequence() {
    if (CURRENT_LEVEL >= MAX_SEQ) {
        return; // If at the maximum length, no more levels
    } else {
        uint32_t randomNum = 0;
        HAL_RNG_GenerateRandomNumber(&hrng, &randomNum); //generate random num for randNum
        COLOR_SEQUENCE[CURRENT_LEVEL] = randomNum % NUM_BTNS; //mod by 4 to get a number 0-3 for a color
        CURRENT_LEVEL++; //increase level
    }
}

const Note NOTE_ARR[NUM_BTNS] = {NOTE_B, NOTE_C_SHARP, NOTE_G, NOTE_A}; //yellow, blue, red, green

// Plays note for each color
void play_note(uint8_t color_index) {
    if (color_index >= NUM_BTNS) { //ignores invalid colors
        return;
    } else {
        //valid colors
        buzzerSetNote(&buzzer1, NOTE_ARR[color_index], BUZZER_OCTAVE); //set buzzer to note
        buzzerEnable(&buzzer1); //enable buzzer
    }
}
// Stops the note being played
void stop_note(void) {
    buzzerDisable(&buzzer1); //disable buzzer
}
// Handles turning on and off the LEDs
void lightPad(uint8_t color, bool on) {
    if (color >= NUM_BTNS) { //ignores invalid colors
        return;
    } else {
        //valid colors
        if (on) {
            // if 'on'is true
            LED_PORTS[color]->ODR |= LED_PINS[color]; //set bit to HIGH, so led on
        } else {
            // if 'on' is false
            LED_PORTS[color]->ODR &= ~LED_PINS[color]; //set bit to LOW, so led off
        }
    }
    // update array for graphics when button turns on
    buttonPressed[color]= on;
    // update the display
    updateDisplay = true;
}
// Reads button inputs from user
uint8_t readButton() {
    uint8_t butt = NOT_PRESSED;
    for (uint8_t i = 0; i < NUM_BTNS; i++) {
        if ((BUTTON_PORTS[i]->IDR & BUTTON_PINS[i]) != 0) {
            //0 = pressed (pull down)
            butt = i; //return color name
        }
    }
    return butt;
}

void In_Game_Display(void); // forward dec for the display function

//checks if the select button has been pressed
static bool checkSelectPress(void) {
    if (currentState == IDLE) {
        return false;
    }
    if (TFTShield18_readButtons() & TFTSHIELD_BUTTON_IN) {
        return false;
    } // not pressed

    for (uint8_t i = 0; i < NUM_BTNS; i++) {
        lightPad(i, false);
    }
    stop_note();

    levelSuccess = false;
    FIRST_LOOP = true;
    WAIT_FOR_RELEASE = false;
    LAST_BTN = NOT_PRESSED;
    CURRENT_LEVEL = 0;
    CURRENT_LENGTH = 0;
    CURRENT_INPUT = 0;
    HISTORY_COUNTER = 0;

    currentState = IDLE;
    updateDisplay = true;

    while (!(TFTShield18_readButtons() & TFTSHIELD_BUTTON_IN)) HAL_Delay(10);  // wait for release
    HAL_Delay(DEBOUNCE_TIME);

    In_Game_Display();   // redraw the welcome screen
    return true;
}

// like the HAL_Delay, but returns it true if SELECT interrupted it (already reset)
static bool waitMs(uint32_t ms) {
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < ms) {
        if (checkSelectPress()) return true;
    }
    return false;
}

// random flashes for game over scene
static bool randomFlashes(uint32_t durationMs) {
    uint32_t start = HAL_GetTick();
    while (HAL_GetTick() - start < durationMs) {
        uint32_t randomNum = 0;
        HAL_RNG_GenerateRandomNumber(&hrng, &randomNum);
        uint8_t color = randomNum % NUM_BTNS;   // 0-3

        lightPad(color, true); //led on
        play_note(color); //matching buzzer sound
        HAL_Delay(100); //keep it lit for a bit
        lightPad(color, false); //led off
        stop_note(); //buzzer off
        //HAL_Delay(50); //pause before the next one
        if (waitMs(50)) {
            return true;
        } // wait 50ms, but return if SELECT pressed
    }
    return false;
}

// Initalizes all GPIO pins
void Game_GPIO_Init() {
    // wake up clocks first
    // RCC RCC_AHB2ENR
    // bit 0: IO port A
    // bit 1: IO port B
    // bit 2: IO port C

    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN | RCC_AHB2ENR_GPIOCEN;
    (void)RCC->AHB2ENR; //dummy read for clock to start before we try to use it

    for (uint8_t i = 0; i < NUM_BTNS; i++) {
        //initialize led pins to outputs
        uint32_t ledN = PIN_NUM(LED_PINS[i]);
        LED_PORTS[i]->MODER &= ~MODER_MASK(ledN); // clear the field
        LED_PORTS[i]->MODER |= MODER_OUT(ledN); // set 01 = output
        LED_PORTS[i]->ODR   &= ~LED_PINS[i]; // low = LED off

        // initialize btn pins to input w internal pull-up
        uint32_t btnN = PIN_NUM(BUTTON_PINS[i]);
        BUTTON_PORTS[i]->MODER &= ~MODER_MASK(btnN); // clear field -> 00 = input
        BUTTON_PORTS[i]->PUPDR &= ~PUPDR_MASK(btnN); // clear pull field
        BUTTON_PORTS[i]->PUPDR |= PUPDR_PU(btnN); // set 10 = pull-down 
    }
    
}

// Constants for GUI, tune when display is on
static const uint16_t brightColors[4] = {
    0xFFE0, // Yellow
    0x001F, // Blue
    0xF800, // Red
    0x07E0  // Green
};
static const uint16_t dimColors[4] = {
    0x5280, // Yellow
    0x000A, // Blue
    0x5000, // Red
    0x0200  // Green
};
// Positions for each button rectangle
static const int16_t xPos[4] = {20, 86, 20, 86};
static const int16_t yPos[4] = {22, 22, 66, 66};

// Helper function to update the button memory
void addToHistory(uint8_t buttonIn)
{
    // if there arent five buttons in the array yet, add the current one
    if (HISTORY_COUNTER < HISTORY_SIZE) {
        BUTTON_HISTORY[HISTORY_COUNTER] = buttonIn;
        HISTORY_COUNTER++;
    }
    else {
        // if there are more, shift older entries out 
        for (uint8_t i = 0; i < HISTORY_SIZE - 1; i++) {
            BUTTON_HISTORY[i] = BUTTON_HISTORY[i + 1];
        }
        // save new input
        BUTTON_HISTORY[HISTORY_SIZE - 1] = buttonIn;
    }
    updateDisplay = true;
}

// Handles the graphics logic for the Simon Says game
void In_Game_Display(void){ 
    static State_t previousState = GAME_LOSE;
    bool previousSuccess = false;

    // Avoids updating the display if it does not need to be
    if(!updateDisplay){
        return;
    }
    updateDisplay = false; // Reset the flag after updating the display

    // Only fill the screen when a new screen is being shown to avoid overlapping
    if (currentState != previousState || levelSuccess != previousSuccess) {
        Display_fillScreen(ST7735_WHITE);
        previousState = currentState;
    }
    // Level success screen
    if (levelSuccess) {
        Display_setTextSize(2, 2);
        Display_setCursor(32, 48);
        Display_setTextColor(ST7735_GREEN);
        Display_print("GOOD JOB!");
        return;
    }
    
    Display_setTextSize(1, 1);
    Display_setTextColor(ST7735_BLACK);
    // Handles screens for each state except for coutdown
    if (currentState == IDLE) {
        Display_setCursor(24, 35);
        Display_print("Simon Says Game");
        Display_setTextSize(1, 1);
        Display_setCursor(24, 76);
        Display_print("Press SELECT to start");
        return;
    }
    if (currentState == GAME_WIN) {
        Display_setTextSize(2, 2);
        Display_setCursor(32, 48);
        Display_setTextColor(ST7735_GREEN);
        Display_print("YOU WIN!");
        return;
    }
    if (currentState == GAME_LOSE) {
        Display_setTextSize(2, 2);
        Display_setCursor(26, 48);
        Display_setTextColor(ST7735_RED);
        Display_print("GAME OVER");
        return;
    }

    // Displays the level at the top of the screen
    char levelString[32];
    snprintf(levelString, sizeof(levelString), "Level: %u / %u", CURRENT_LEVEL, MAX_SEQ);
    Display_setTextColor(ST7735_BLACK);
    Display_setTextSize(1, 1);
    Display_setCursor(8, 5);
    Display_print(levelString);

    // Loops through drawing each button (avoids redundant code)
    for (uint8_t i = 0; i < 4; i++) {
        Display_fillRoundRect(xPos[i], yPos[i], 54, 39, 6, 
            buttonPressed[i] ? brightColors[i] : dimColors[i]);
    }
    
    // Displays the last five buttons pressed at the bottom of the screen
    Display_setTextColor(ST7735_BLACK);
    Display_setCursor(8, 115);
    Display_print("Last 5:");
    // loops through drawing each button color to avoid redundant code
    for (uint8_t i = 0; i < HISTORY_COUNTER; i++) {
        Display_fillCircle( 66 + i * 17, 118, 5,brightColors[BUTTON_HISTORY[i]]);
    }
    
}

// ENTIRE FINITE STATE MACHINE
void Simon_State_Machine(void){
    // trigger for game to start
    uint32_t startgame;

    if (checkSelectPress()) {
        return;
    }

    // STATE SWITCH STATEMENT
    switch(currentState){
        case IDLE: 
            // listne for SELECT button
            startgame = TFTShield18_readButtons(); 
            // if the game is starting, turn everythin off and transition after a delay
            if(!(startgame & TFTSHIELD_BUTTON_IN)){ 
                HAL_Delay(200); // wait 200 milliseconds to prepare player for countdown
                lightPad(0, false);
                lightPad(1, false);
                lightPad(2, false);
                lightPad(3, false);
                currentState = COUNTDOWN;
            }
            break;

        case COUNTDOWN: 
            for(uint8_t i = 3; i >= 1; i--){ // counts down numbers for a 3 second timer
                char seconds_string[2];
                sprintf(seconds_string, "%d", i); // converts current number to a string that Display_print can read
                // displays the number on screen every second
                Display_setTextSize(4, 4);
                Display_setCursor(68, 48);
                Display_setTextColor(ST7735_BLACK);
                Display_fillRect(60, 40, 40, 50, ST7735_WHITE);
                Display_print(seconds_string);
                //HAL_Delay(1000); // wait 1000 milliseconds, 1 second    
                if (waitMs(1000)) {
                    return;
                }
            }
            CURRENT_LEVEL = 0; // reset the level so it starts from 0
            addToSequence(); //add first color to sequence
            CURRENT_LENGTH = 1; //start with 1 color in sequence
            HISTORY_COUNTER = 0;  // reset history 
            FIRST_LOOP = true; //set for game
            currentState = PLAY_SEQUENCE;
            break;

        case PLAY_SEQUENCE: 
            // Play the sequence of notes
            if (FIRST_LOOP) { // first pass in seq -- set up playback
                PLAYBACK_INDEX = 0; // first btn in playback sequence
                FIRST_LOOP = false;
            }
            if (PLAYBACK_INDEX < CURRENT_LENGTH) { //still more colors to play
                CURRENT_COLOR = COLOR_SEQUENCE[PLAYBACK_INDEX];
                // LED logic
                lightPad(CURRENT_COLOR, true); //led on
                play_note(CURRENT_COLOR); //note on
                In_Game_Display();
                //HAL_Delay(SEQ_TIME); //keep it lit
                if (waitMs(SEQ_TIME)) {
                    return;
                }
                lightPad(CURRENT_COLOR, false); //led off
                stop_note(); //note off
                In_Game_Display();
                //HAL_Delay(SEQ_TIME / 2); //short gap before the next color
                if (waitMs(SEQ_TIME/2)) {
                    return;
                }
                PLAYBACK_INDEX++;
            } 
            else { //done with sequence
                FIRST_LOOP = true; //reset for next time
                currentState = PLAYER_INPUT; //wait for player input now
            }
            break;

        case PLAYER_INPUT: { 
            if (FIRST_LOOP) { //first pass in player input -- set up input
                CURRENT_INPUT = 0; // first btn in player input sequence
                FIRST_LOOP = false;
                LAST_BTN = NOT_PRESSED;
                WAIT_FOR_RELEASE = (readButton() != NOT_PRESSED); //waits for release if a button is already pressed
            }
            // current button pressed
            uint8_t btnPressed = readButton();

            if (btnPressed != NOT_PRESSED) { //a button is pressed
                if (!WAIT_FOR_RELEASE) { //we're not waiting for a prev btn to be released
                    WAIT_FOR_RELEASE = true; //only false if btn not pressed
                    LAST_BTN = btnPressed;

                    lightPad(LAST_BTN, true); //led on
                    play_note(LAST_BTN); //note on
                }
            } else if (WAIT_FOR_RELEASE) { //btn was just released
                WAIT_FOR_RELEASE = false; //no longer waiting for release
                uint8_t releasedBtn = LAST_BTN; //btn that was just released
                LAST_BTN = NOT_PRESSED; //reset last btn

                lightPad(releasedBtn, false); //led off
                stop_note(); //note off
                HAL_Delay(DEBOUNCE_TIME); //debounce delay
                // save inputs (correct and incorrect) to the  memory array
                if (releasedBtn != NOT_PRESSED) {
                    addToHistory(releasedBtn);
                }
                //check btn
                if (releasedBtn != COLOR_SEQUENCE[CURRENT_INPUT]) { //wrong btn
                    FIRST_LOOP = true; //reset for next time
                    currentState = GAME_LOSE; //lose game... whomp whomp
                    updateDisplay = true;
                } else { //right btn, yay!
                    USER_SEQUENCE[CURRENT_INPUT] = releasedBtn; //store btn in user sequence
                    CURRENT_INPUT++; //next btn in user input sequence
                    updateDisplay = true;
                    //if user entered all the correct btns for the current level
                    if (CURRENT_INPUT == CURRENT_LENGTH) { //finished current user's seq
                            levelSuccess = true;
                            updateDisplay = true;
                            In_Game_Display();
                            //HAL_Delay(SUCCESS_TIME);
                            if (waitMs(SUCCESS_TIME)) {
                                return;
                            }

                            levelSuccess = false;
                        if (CURRENT_LEVEL >= MAX_SEQ) { //finished the last color of the last level
                            currentState = GAME_WIN; //win game, yay!
                        } else { //finish current level, move on to next
                            addToSequence(); //add new color and level++
                            //HAL_Delay(SUCCESS_TIME); //wait a bit before next level
                            if (waitMs(SUCCESS_TIME)) {
                                return;
                            }
                            CURRENT_LENGTH = CURRENT_LEVEL; // update length of sequence
                            currentState = PLAY_SEQUENCE; //play next level
                        }
                        FIRST_LOOP = true;
                        updateDisplay = true;  
                    }
                }
            }
            break;
        }
        case GAME_WIN: 
            // updates display with win message
            updateDisplay = true;
            In_Game_Display();
            // returns to IDLE after a delay
            //HAL_Delay(SUCCESS_TIME);
            if (waitMs(SUCCESS_TIME)) {
                return;
            }
            currentState = IDLE;
            updateDisplay = true;
            break;

        case GAME_LOSE: 
            // updates the display with game over message
            updateDisplay = true;
            In_Game_Display();
            if (randomFlashes(1000)) { //random flashes for 2 seconds
                return;
            }
            // returns to IDLE after a delay
            //HAL_Delay(SUCCESS_TIME);
            if (waitMs(SUCCESS_TIME)) {
                return;
            }
            currentState = IDLE;
            updateDisplay = true;
            break;

        default: // default catchall
            currentState = IDLE;
            break;
    }
    // update display every state machine cycle
    In_Game_Display();
}
