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


typedef enum{
    IDLE,
    COUNTDOWN,
    PLAY_SEQUENCE,
    PLAYER_INPUT,
    GAME_WIN,
    GAME_LOSE
} State_t;

static State_t currentState = IDLE;



void Simon_State_Machine(void){

    switch(currentState){
        case IDLE:
            // Wait for user input to start the game
            if(!(TFTShield18_readButtons() & TFTSHIELD_BUTTON_1)){
                currentState = COUNTDOWN;
            }
            break;

        case COUNTDOWN:
            // Perform countdown logic here
            // Transition to PLAY_SEQUENCE after countdown
            currentState = PLAY_SEQUENCE;
            break;

        case PLAY_SEQUENCE:
            // Play the sequence of notes
            // Transition to PLAYER_INPUT after sequence is played
            currentState = PLAYER_INPUT;
            break;

        case PLAYER_INPUT:
            // Wait for player input and check against the sequence
            // Transition to GAME_WIN or GAME_LOSE based on input
            break;

        case GAME_WIN:
            // Handle game win logic
            // Transition back to IDLE or restart game
            currentState = IDLE;
            break;

        case GAME_LOSE:
            // Handle game lose logic
            // Transition back to IDLE or restart game
            currentState = IDLE;
            break;

        default:
            currentState = IDLE;
            break;
    }

}