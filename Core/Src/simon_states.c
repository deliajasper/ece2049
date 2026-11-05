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
// TODO: IOther variables needed may include a sequence array, a player input array, variables to keep track of the current level or sequence length,
// booleans to track if the sequence was correctly enetered, a number for how many levels there are, a varaibles for what level the player is on, etc.

void Simon_State_Machine(void){

    switch(currentState){
        case IDLE:
            // Wait for user input to start the game
            // TODO: Display welcome screen
            // TODO: Transition to COUNTDOWN when user starts the game by pressing select
            break;

        case COUNTDOWN:
            // Perform countdown logic here
            // TODO: Display countdown on the screen
            // TODO: Transition to PLAY_SEQUENCE after countdown
            currentState = PLAY_SEQUENCE;
            break;

        case PLAY_SEQUENCE:
            // Play the sequence of notes
            // TODO: Implement method(s) to play the sequence on the LEDs, one LED at a time, 
            // and play a unique note on the buzzer for each LED
            // TODO: Transition to PLAYER_INPUT after sequence is finished
            // TODO: Implement In_Game_Display() to show the current sequence on the screen
            currentState = PLAYER_INPUT;
            break;

        case PLAYER_INPUT:
            // Wait for player input and check against the sequence
            // TODO: Implement method(s) to read player input from the buttons and compare it to the sequence
            // TODO: Transition back to PLAY_SEQUENCE if the player input is correct AND the game is not over
            // transition to GAME_LOSE if the player input is incorrect, or transition to GAME_WIN if the 
            // player has completed all sequences correctly
            // TODO: Implement In_Game_Display() to show the current sequence on the screen
            break;

        case GAME_WIN:
            // Handle game win logic
            // TODO: Add win message to screen
            // TODO: Transition to IDLE after length of time has passed
            currentState = IDLE;
            break;

        case GAME_LOSE:
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

// TODO: Implement memory data structure to keep track of the full history of button presses
// TODO: Implement random number generator to generate the sequence of button presses for the game
// TODO: Other methods needed include countdown timers (maybe), sequence playback, player input reading, and input checking

void In_Game_Display(void){
    // TODO: IMplement display logic for the game, including showing the current sequence, 
    // player input, current level, and game status (win/lose)
    // TODO: Each buttone should have its own element showing button state and should be "visually appealing"
    // TODO: Implement a success message when the player enters the sequence corrent for one level
    // TODO: Show history of last five buttons
}