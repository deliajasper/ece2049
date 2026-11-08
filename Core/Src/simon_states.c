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
static bool updateDisplay = true;

// TODO: Other variables needed may include a sequence array, a player input array, variables to keep track of the current level or sequence length,
// booleans to track if the sequence was correctly enetered, a number for how many levels there are, a varaibles for what level the player is on, etc.

void Simon_State_Machine(void){

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
            break;

        case PLAY_SEQUENCE: // Tran
            // Play the sequence of notes
            // TODO: Implement method(s) to play the sequence on the LEDs, one LED at a time, 
            // and play a unique note on the buzzer for each LED
            // TODO: Transition to PLAYER_INPUT after sequence is finished
            // TODO: Implement In_Game_Display() to show the current sequence on the screen
            currentState = PLAYER_INPUT;
            break;

        case PLAYER_INPUT: // Tran
            // Wait for player input and check against the sequence
            // TODO: Implement method(s) to read player input from the buttons and compare it to the sequence
            // TODO: Transition back to PLAY_SEQUENCE if the player input is correct AND the game is not over
            // transition to GAME_LOSE if the player input is incorrect, or transition to GAME_WIN if the 
            // player has completed all sequences correctly
            // TODO: Implement In_Game_Display() to show the current sequence on the screen
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

// Constants for GUI
static const uint16_t brightColors[4] = {0x07E0, 0xF800, 0xFFE0, 0x001F};
static const uint16_t dimColors[4] = {0x0200, 0x5000, 0x5280, 0x000A};
static const int16_t xPos[4] = {8, 66, 8, 66};
static const int16_t yPos[4] = {22, 22, 66, 66};

void In_Game_Display(void){ // Delia graphics
    // TODO: Implement display logic for the game, including showing the current sequence, 
    // player input, current level, and game status (win/lose)
    // TODO: Implement a success message when the player enters the sequence corrent for one level
    // TODO: Show history of last five buttons
    // Graphics will be more than this and likely multiple methods

    // Avoid redrawing the display if nothing needs to be updated
    if(!updateDisplay){
        return;
    }

    updateDisplay = false;
    Display_setTextSize(1, 1);
    Display_setTextColor(ST7735_BLACK);

    snprintf(levelString, sizeof(levelString), "Level: %d", currentLevel);

    Display_setCursor(8, 5);
    Display_print(levelString);

    // Drawing buttons
    for(uint8_t index = 0; index < 4; index++){
        if(buttonPressed[index]){
            Display_fillRoundRect(xPos[index], yPos[index], 50, 50, 5, brightColors[index]);
        } else {
            Display_fillRoundRect(xPos[index], yPos[index], 50, 50, 5, dimColors[index]);
        }
    }
    Display_drawRoundRect(xPos[i], yPos[i], 54, 40, 10,
            buttonPressed[i] ? ST7735_WHITE : brightColors[i]
        );

    // Add history of last five buttons pressed to display after knowing what the data structure looks like

}

// add more graphics method here