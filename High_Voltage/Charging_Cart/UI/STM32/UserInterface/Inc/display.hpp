#ifndef DISPLAY_HPP
#define DISPLAY_HPP

#include <cstdint>
#include "stm32f0xx_hal.h"
#include "interfaceTemplate.h"

//constants
#define FULL_REFRESH 0b11111111
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define PAGE_SIZE 8

class Display {
public:
    Display(SPI_HandleTypeDef *SPI_HANDLE,
            GPIO_TypeDef *CS_PORT, const uint16_t CS_PIN, 
            GPIO_TypeDef *A0_PORT, const uint16_t A0_PIN,
            GPIO_TypeDef *RST_PORT, const uint16_t RST_PIN,
            TIM_HandleTypeDef *BKLT_TIMER, const uint32_t BKLT_PWM_CHANNEL);
    ~Display();
    
    // Initialization
    void init();

    void on();
    void off();
    
    // Display updates
    void clear();
    void refresh();
    
    // Data updates    
    void setInfo(InfoType type, int16_t value);
    void invertInfo(InfoType type, bool set);
    void boxInfo(InfoType type, bool set);

    void setStatus(const char* msg);
    void setStatus(const char* msg, uint8_t offset, bool fill_spaces);
    
    // Display control
    void setBrightness(uint8_t brightness);

    //testing
    void setAllInfo(uint16_t byte);

private:
    //communication info
    SPI_HandleTypeDef *SPI_HANDLE;
    GPIO_TypeDef *CS_PORT;
    uint16_t CS_PIN;
    GPIO_TypeDef *A0_PORT;
    uint16_t A0_PIN;
    GPIO_TypeDef *RST_PORT;
    uint16_t RST_PIN;
    TIM_HandleTypeDef *BKLT_TIMER;
    uint32_t BKLT_PWM_CHANNEL;

    uint8_t needsRefresh; //bitmask of which pages need a refresh
    uint8_t framebuffer[SCREEN_HEIGHT/PAGE_SIZE][SCREEN_WIDTH]; // pages of columns

    
    //bitmask of which data types are currently inverted and a helper function
    uint32_t infoInvertedMask = 0;
    bool isInverted(InfoType type);
    void setInverted(InfoType type, bool set);

    void addToFramebuffer(uint8_t x, uint8_t y, const uint8_t *data, uint8_t width, uint8_t height, bool inverted = false, bool doubled = false);
    void getBoundingBox(InfoType type, uint8_t &x, uint8_t &y, uint8_t &width, uint8_t &height);

    HAL_StatusTypeDef sendData(const uint8_t* data, size_t length);
    HAL_StatusTypeDef sendCommand(const uint8_t* cmd, size_t length);
};

#endif // DISPLAY_HPP