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

// later: buttons[currBtn] = {LED color, button info, note frequency, etc}

// PORT: GPIOA, GPIOB, GPIOC
// PIN: PIN NUMBER (0-15)
// LEDS: PC10, PC 12, PA15, PC2
#define LED_YELLOW_PIN
#define LED_YELLOW_PORT    
#define LED_BLUE_PIN
#define LED_BLUE_PORT
#define LED_RED_PIN
#define LED_RED_PORT
#define LED_GREEN_PIN
#define LED_GREEN_PORT
// BUTTONS: PB12, PB2, PB15, PB14
#define BUTTON_YELLOW_PIN
#define BUTTON_YELLOW_PORT
#define BUTTON_BLUE_PIN
#define BUTTON_BLUE_PORT
#define BUTTON_RED_PIN    
#define BUTTON_RED_PORT
#define BUTTON_GREEN_PIN
#define BUTTON_GREEN_PORT
// BUZZER: PE9
#define BUZZER_PIN GPIO_PIN_9
#define BUZZER_PORT GPIOE

// Define the states of the Simon game
typedef enum{
    IDLE,
    COUNTDOWN,
    PLAY_SEQUENCE,
    PLAYER_INPUT,
    GAME_WIN,
    GAME_LOSE
} State_t;

static State_t currentState = IDLE;

static bool levelSuccess = false; // Track if the player has successfully completed the current level
static bool gameLost = false; // Track if the player has lost the game
static bool gameWon = false; // Track if the player has won the game

static uint8_t currentLevel = 0; // Current level of the game
// TODO: assign order of colors
static bool buttonPressed[4] = {false, false, false, false}; // Track button presses for each color  
static bool updateDisplay = false; // Flag to indicate if the display needs to be updated

// TODO: Other variables needed may include a sequence array, a player input array, variables to keep track of the current level or sequence length,
// booleans to track if the sequence was correctly enetered, a number for how many levels there are, a varaibles for what level the player is on, etc.
// things about the game itself
#define NUM_BTNS 4 //4 buttons, 0 = yellow, 1 = blue, 2 = red, 3 = green
#define MAX_SEQ 32 //maximum 32 flashes per level, we can have 32 levels so it increases by 1 each time
#define NOT_PRESSED 255 //no btn pressed

// need a sequence for game, sequence for user inupt, current level, current user input, current color, last button pressed
uint8_t COLOR_SEQUENCE[MAX_SEQ]; // sequence of button presses for the game
uint8_t USER_SEQUENCE[MAX_SEQ]; // sequence of button presses for the player

uint8_t CURRENT_LEVEL = 0; // current level of the game -- also representing how long the sequence will get for this lvl
uint8_t CURRENT_INPUT = 0; // index of the current button press in the player's input
uint8_t CURRENT_COLOR = 0; // current color being displayed
uint8_t CURRENT_LENGTH = 0; // current length of the sequence of the current level

uint8_t LAST_BTN = NOT_PRESSED; // last button that was pressed -> starts at 255 bc max val for 8-bit unsigned int so it won't correspond to a color button
bool WAIT_FOR_RELEASE = false; //wait after a button press until it's let go

bool FIRST_LOOP = true; //first loop after entering a state
uint8_t PLAYBACK_INDEX = 0; //index of the current btn on the playback

uint16_t SEQ_TIME = 200; //how long the btn color stays up during playback 
static bool gpioReady = false; //only sets up the pins once

#define DEBOUNCE_TIME 20 //how long to wait after btn release
#define SUCCESS_TIME 1000 // how long success message stays up

static GPIO_TypeDef* LED_PORTS[NUM_BTNS] = {LED_YELLOW_PORT, LED_BLUE_PORT, LED_RED_PORT, LED_GREEN_PORT}; //array of led ports
static const uint16_t LED_PINS[NUM_BTNS] = {LED_YELLOW_PIN, LED_BLUE_PIN, LED_RED_PIN, LED_GREEN_PIN}; //array of led pins

static GPIO_TypeDef* BUTTON_PORTS[NUM_BTNS] = {BUTTON_YELLOW_PORT, BUTTON_BLUE_PORT, BUTTON_RED_PORT, BUTTON_GREEN_PORT}; //array of button ports
static const uint16_t BUTTON_PINS[NUM_BTNS] = {BUTTON_YELLOW_PIN, BUTTON_BLUE_PIN, BUTTON_RED_PIN, BUTTON_GREEN_PIN}; //array of button pins

extern Buzzer buzzer; //extern buzzer struct defined in main.c

#define MODER_MASK(n) (GPIO_MODER_MODE0   << (2 * (n))) //2-bit mode field for pin n
#define MODER_OUT(n) (GPIO_MODER_MODE0_0 << (2 * (n))) //that field set to 01 = output
#define PUPDR_MASK(n) (GPIO_PUPDR_PUPD0   << (2 * (n))) //2-bit pull field for pin n
#define PUPDR_PU(n) (GPIO_PUPDR_PUPD0_0 << (2 * (n))) //that field set to 01 = pull-up
#define PIN_BIT(n) (1 << (n)) //pin n's bit in ODR or IDR
#define BUZZER_OCTAVE 2 //buzzer octave

// helper functions
static void addToSequence() {
    if (CURRENT_LEVEL >= MAX_SEQ) {
        return; // no more levels!
    } else {
        uint32_t randomNum = 0;
        HAL_RNG_GenerateRandomNumber(&hrng, &randomNum); //generate random num for randNum
        COLOR_SEQUENCE[CURRENT_LEVEL] = randomNum % NUM_BTNS; //mod by 4 to get a number 0-3 for a color
        CURRENT_LEVEL++; //increase level
    }
}

const Note NOTE_ARR[NUM_BTNS] = {NOTE_B, NOTE_C_SHARP, NOTE_G, NOTE_A}; //yellow, blue, red, green

void play_note(uint8_t color_index) {
    if (color_index >= NUM_BTNS) { //ignores invalid colors
        return;
    } else {
        //valid colors
        buzzerSetNote(&buzzer, NOTE_ARR[color_index], BUZZER_OCTAVE); //set buzzer to note
        buzzerEnable(&buzzer); //enable buzzer
    }
}

void stop_note(void) {
    buzzerDisable(&buzzer); //disable buzzer
}

void lightPad(uint8_t color, bool on) {
    if (color >= NUM_BTNS) { //ignores invalid colors
        return;
    } else {
        //valid colors
        if (on) {
            // if 'on'is true
            LED_PORTS[color]->ODR |= PIN_BIT(LED_PINS[color]); //set bit to HIGH, so led on
        } else {
            // if 'on' is false
            LED_PORTS[color]->ODR &= ~PIN_BIT(LED_PINS[color]); //set bit to LOW, so led off
        }
    }
}

uint8_t readButton() {
    for (uint8_t i = 0; i < NUM_BTNS; i++) {
        if ((BUTTON_PORTS[i]->IDR & PIN_BIT(BUTTON_PINS[i])) == 0) {
            //0 = pressed (pull up)
            return i; //return color name
        }
    }
    return NOT_PRESSED;
}

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
        LED_PORTS[i]->MODER &= ~MODER_MASK(LED_PINS[i]); // clear the field
        LED_PORTS[i]->MODER |= MODER_OUT(LED_PINS[i]); // set 01 = output
        LED_PORTS[i]->ODR &= ~PIN_BIT(LED_PINS[i]); // low = LED off

        // initialize btn pins to input w internal pull-up
        BUTTON_PORTS[i]->MODER &= ~MODER_MASK(BUTTON_PINS[i]); // clear field -> 00 = input
        BUTTON_PORTS[i]->PUPDR &= ~PUPDR_MASK(BUTTON_PINS[i]); // clear pull field
        BUTTON_PORTS[i]->PUPDR |= PUPDR_PU(BUTTON_PINS[i]); // set 01 = pull-up
    }
}


void Simon_State_Machine(void){
    if (!gpioReady) {
        Game_GPIO_Init(); //initializes gpio
        gpioReady = true; //sets flag to be true
    }

    switch(currentState){
        case IDLE: // Isaac
            // Wait for user input to start the game
            // TODO: Display welcome screen
            Display_setTextSize(1, 1);
            Display_setTextColor(ST7735_BLACK);
            Display_setCursor(Display_width() / 2 - 58, Display_height() / 2 - 5);
            Display_print("Welcome to the Simon Says Game!");
            // TODO: Transition to COUNTDOWN when user starts the game by pressing select
            break;

        case COUNTDOWN: // Isaac
            // Perform countdown logic here
            // TODO: Display countdown on the screen
            // TODO: Transition to PLAY_SEQUENCE after countdown
            currentState = PLAY_SEQUENCE;
            addToSequence(); //add first color to sequence
            CURRENT_LENGTH = 1; //start with 1 color in sequence
            FIRST_LOOP = true; //set for game
            break;

        case PLAY_SEQUENCE: // Tran
            // Play the sequence of notes
            // TODO: Implement method(s) to play the sequence on the LEDs, one LED at a time, 
            // and play a unique note on the buzzer for each LED
            // TODO: Transition to PLAYER_INPUT after sequence is finished
            // TODO: Implement In_Game_Display() to show the current sequence on the screen

            if (FIRST_LOOP) { //first pass in seq -- set up playback
                PLAYBACK_INDEX = 0; // first btn in playback sequence
                FIRST_LOOP = false;
            }

            if (PLAYBACK_INDEX < CURRENT_LENGTH) { //still more colors to play
                CURRENT_COLOR = COLOR_SEQUENCE[PLAYBACK_INDEX];

                lightPad(CURRENT_COLOR, true); //led on
                play_note(CURRENT_COLOR); //note on
                DWT_delayMs(SEQ_TIME); //keep it lit
                lightPad(CURRENT_COLOR, false); //led off
                stop_note(); //note off
                DWT_delayMs(SEQ_TIME / 2); //short gap before the next color

                PLAYBACK_INDEX++;
            } else { //done with sequence
                FIRST_LOOP = true; //reset for next time
                currentState = PLAYER_INPUT; //wait for player input now
            }

            break;

        case PLAYER_INPUT: // Tran
            // Wait for player input and check against the sequence
            // TODO: Implement method(s) to read player input from the buttons and compare it to the sequence
            // TODO: Transition back to PLAY_SEQUENCE if the player input is correct AND the game is not over
            // transition to GAME_LOSE if the player input is incorrect, or transition to GAME_WIN if the 
            // player has completed all sequences correctly
            // TODO: Implement In_Game_Display() to show the current sequence on the screen

            if (FIRST_LOOP) { //first pass in player input -- set up input
                CURRENT_INPUT = 0; // first btn in player input sequence
                FIRST_LOOP = false;
                LAST_BTN = NOT_PRESSED;
                WAIT_FOR_RELEASE = (readButton() != NOT_PRESSED); //waits for release if a button is already pressed
            }

            uint8_t btnPressed = readButton();

            if (btnPressed != NOT_PRESSED) { //a button is pressed
                if (!WAIT_FOR_RELEASE) { //we're not waiting for a prev btn to be released
                    WAIT_FOR_RELEASE = true; //only false if btn not pressed
                    LAST_BTN = btnPressed;

                    lightPad(LAST_BTN, true); //led on
                    play_note(LAST_BTN); //note on
                }
            } else if (WAIT_FOR_RELEASE) { //btn was just released
                uint8_t releasedBtn = LAST_BTN; //btn that was just released
                LAST_BTN = NOT_PRESSED; //reset last btn
                WAIT_FOR_RELEASE = false; //no longer waiting for release

                lightPad(releasedBtn, false); //led off
                stop_note(); //note off
                DWT_delayMs(DEBOUNCE_TIME); //debounce delay

                //check btn
                if (releasedBtn != COLOR_SEQUENCE[CURRENT_INPUT]) { //wrong btn
                    FIRST_LOOP = true; //reset for next time
                    currentState = GAME_LOSE; //lose game... whomp whomp
                } else { //right btn, yay!
                    USER_SEQUENCE[CURRENT_INPUT] = releasedBtn; //store btn in user sequence
                    CURRENT_INPUT++; //next btn in user input sequence

                    //if user entered all the correct btns for the current level
                    if (CURRENT_INPUT == CURRENT_LENGTH) { //finished current user's seq
                        if (CURRENT_LENGTH < CURRENT_LEVEL) { // lvl not done, show more colors
                            CURRENT_LENGTH++; //next round of current lvl's sequence
                            FIRST_LOOP = true; //reset for next round in same level
                            currentState = PLAY_SEQUENCE; //replay lvl with one more color
                        } else if (CURRENT_LEVEL >= MAX_SEQ) { //finished the last color of the last level
                            FIRST_LOOP = true; //reset for next time user plays
                            currentState = GAME_WIN; //win game, yay!
                        } else { //finish current level, move on to next
                            DWT_delayMs(SUCCESS_TIME); //wait a bit before next level
                            addToSequence(); //add new color and level++
                            CURRENT_LENGTH = 1; //next level starts back at 1 color
                            FIRST_LOOP = true; //reset
                            currentState = PLAY_SEQUENCE; //play next level
                        }
                    }
                }
            }

            break;

        case GAME_WIN: // Isaac
            // Handle game win logic
            // TODO: Add win message to screen
            // TODO: Transition to IDLE after length of time has passed
            currentState = IDLE;
            break;

        case GAME_LOSE: // Isaac
            // Handle game lose logic
            // TODO: Add lose message to screen with "annoying" buzzer sound
            // TODO: Transition to IDLE after length of time has passed
            currentState = IDLE;
            break;

        default:
            currentState = IDLE;
            break;
    }

}

// Tran
// TODO: Implement memory data structure to keep track of the full history of button presses
// TODO: Implement random number generator to generate the sequence of button presses for the game
// TODO: Other methods needed include countdown timers (maybe), sequence playback, player input reading, and input checking

// Constants for GUI, tune when display is on
static const uint16_t brightColors[4] = {0x07E0, 0xF800, 0xFFE0, 0x001F};
static const uint16_t dimColors[4] = {0x0200, 0x5000, 0x5280, 0x000A};
static const int16_t xPos[4] = {8, 66, 8, 66};
static const int16_t yPos[4] = {22, 22, 66, 66};

void In_Game_Display(void){ // Delia graphics
    // TODO: Show history of last five buttons

    // Avoid redrawing the display if nothing needs to be updated
    if(!updateDisplay){
        return;
    }
    updateDisplay = false; // Reset the flag after updating the display

    Display_setTextSize(1, 1);
    Display_setTextColor(ST7735_BLACK);

    snprintf(levelString, sizeof(levelString), "Level: %d", currentLevel);

    Display_setCursor(8, 5);
    Display_print(levelString);

    if (currentState == IDLE) {
        Display_setCursor(12, 35);
        Display_print("SIMON GAME");
        Display_setTextSize(1, 1);
        Display_setCursor(12, 76);
        Display_print("Press SELECT to start");
        return;
    }
    if (currentState == COUNTDOWN) {
        char number[8]; // char to hold the countdown number as a string
        // calculate how much time has passed
        // print the number
        Display_setCursor(12, 35);
        Display_print("Get ready to start in:");
        Display_setCursor(12, 55);
        //Display_print(number);
        return;
    }
    if (currentState == GAME_WIN) {
        DrawMessage("YOU WIN!", "All 32 levels!", ST7735_GREEN);
        return;
    }
    if (currentState == GAME_LOSE) {
        DrawMessage("GAME OVER", "Try again!", ST7735_RED);
        return;
    }

    Display_setTextSize(1, 1);
    Display_setTextColor(ST7735_BLACK);
    Display_fillScreen(ST7735_WHITE);
    
    char label[32];
    snprintf(levelString, sizeof(levelString), "Level: %u / %u", currentLevel, MAX_LEVEL);
    Display_setTextColor(ST7735_WHITE);
    Display_setTextSize(1, 1);
    Display_setCursor(8, 5);
    Display_print(levelString);

    if (levelSuccess) {
        Display_setCursor(100, 5);
        Display_setTextColor(ST7735_GREEN);
        Display_print("GOOD JOB!");
    }
    

    // Drawing buttons, looping to make it easy
    for(uint8_t index = 0; index < 4; index++){
        if(buttonPressed[index]){
            Display_fillRoundRect(xPos[index], yPos[index], 50, 50, 5, brightColors[index]);
        } else {
            Display_fillRoundRect(xPos[index], yPos[index], 50, 50, 5, dimColors[index]);
        }
    }
    // Add history of last five buttons pressed to display after knowing what the data structure looks like
    
}

// display game over message
void GameOverDisplay(void){
    Display_setTextSize(4, 4);
    Display_setTextColor(ST7735_RED);

    Display_setCursor(8, 5);
    Display_print("Game Over!");
}

// display game win message
void GameWinDisplay(void){
    Display_setTextSize(4, 4);
    Display_setTextColor(ST7735_GREEN);

    Display_setCursor(8, 5);
    Display_print("You Win!");
}