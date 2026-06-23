#include "display.hpp"

#include <cstdint>

#include "interfaceTemplate.h"
#include "stm32f0xx_hal.h"

//DISPLAY COMMANDS https://www.hpinfotech.ro/ST7565.pdf
#define CMD_DISPLAY_OFF 0b10101110
#define CMD_DISPLAY_ON  0b10101111
#define CMD_DISPLAY_ALL_ON 0b10100101

//initialization commands
#define CMD_LCD_BIAS    0b10100010
#define CMD_ADC_SELECT  0b10100000 //0b10100000 for normal, 0b10100001 for reverse
#define CMD_COM_OUTPUT  0b11001000 //0b11000000 for normal, 0b11001000 for reverse
#define CMD_REG_DIVIDER 0b00100101 //last three bits are the ratio
#define CMD_VOLUME_MODE 0b10000001
#define CMD_SET_VOLUME  0b00011011 //last five bits are volume level
#define CMD_POWER_CNFG  0b00101111 //all stages enabled (last three bits)

#define CMD_SET_PAGE_0  0b10110000 //last four bits are page number
#define CMD_SET_COL_U_0 0b00010000 //last four bits are upper col address
#define CMD_SET_COL_L_0 0b00000000 //last four bits are lower



Display::Display(SPI_HandleTypeDef *SPI_HANDLE,
                 GPIO_TypeDef *CS_PORT, const uint16_t CS_PIN, 
                 GPIO_TypeDef *A0_PORT, const uint16_t A0_PIN,
                 GPIO_TypeDef *RST_PORT, const uint16_t RST_PIN,
                 TIM_HandleTypeDef *BKLT_TIMER, const uint32_t BKLT_PWM_CHANNEL)
    : SPI_HANDLE(SPI_HANDLE), 
      CS_PORT(CS_PORT), CS_PIN(CS_PIN), 
      A0_PORT(A0_PORT), A0_PIN(A0_PIN), 
      RST_PORT(RST_PORT), RST_PIN(RST_PIN),
      BKLT_TIMER(BKLT_TIMER), BKLT_PWM_CHANNEL(BKLT_PWM_CHANNEL),
      needsRefresh(FULL_REFRESH) {
    clear();
}

Display::~Display() {
}

void Display::init() {
    // Reset display
    HAL_GPIO_WritePin(RST_PORT, RST_PIN, GPIO_PIN_RESET);
    HAL_Delay(10);
    HAL_GPIO_WritePin(RST_PORT, RST_PIN, GPIO_PIN_SET);
    HAL_Delay(10);

    uint8_t cmd[]= {CMD_LCD_BIAS,
                    CMD_ADC_SELECT,
                    CMD_COM_OUTPUT,

                    CMD_REG_DIVIDER,
                    CMD_VOLUME_MODE,
                    CMD_SET_VOLUME,
                    CMD_POWER_CNFG,
                    };
    sendCommand(cmd, sizeof(cmd)/sizeof(cmd[0]));

    refresh();
}

void Display::on() {
    // Send display on command
    uint8_t cmd[] = {CMD_DISPLAY_ON};
    sendCommand(cmd, 1);
}

void Display::off() {
    // Send display off command
    uint8_t cmd = CMD_DISPLAY_OFF;
    sendCommand(&cmd, 1);
}


void Display::clear() {
    // Initialize framebuffer to base font (static const uint8_t framebufferBase[8][128] in font.hpp)
    for (uint32_t i = 0; i < SCREEN_HEIGHT/PAGE_SIZE; i++) {
        for (uint32_t j = 0; j < SCREEN_WIDTH; j++) {
            framebuffer[i][j] = framebufferBase[i][j];
        }
    }
    needsRefresh = FULL_REFRESH;
}

void Display::refresh() {
    //shutcut as it gets called often
    if (needsRefresh == 0) return;

    for (uint8_t page = 0; page < SCREEN_HEIGHT/PAGE_SIZE; page++) {
        if ((needsRefresh & (1 << page))) {
            uint8_t cmd[] = {(CMD_SET_PAGE_0 | page), CMD_SET_COL_U_0, CMD_SET_COL_L_0};
            sendCommand(cmd, sizeof(cmd)/sizeof(cmd[0]));
            sendData(framebuffer[page], SCREEN_WIDTH);
        }
    }
    needsRefresh = 0;
}

void Display::setInfo(InfoType type, int16_t value) {
    bool inverted = isInverted(type);
    //update progress bar if charge percent
    if (type == CHARGE_PERCENT) {
        uint16_t barWidth = ((uint16_t)value * PROGRESS_BAR_WIDTH) / 100;

        for (uint8_t i = 0; i < PROGRESS_BAR_WIDTH; i++) {
            if (i < barWidth) {
                addToFramebuffer(PROGRESS_BAR_X + i, PROGRESS_BAR_Y, (uint8_t[]){0xFF}, 1, PROGRESS_BAR_HEIGHT, inverted);
            } else {
                addToFramebuffer(PROGRESS_BAR_X + i, PROGRESS_BAR_Y, (uint8_t[]){0x00}, 1, PROGRESS_BAR_HEIGHT, inverted);
            }
        }
    }

    uint8_t digit = 0;
    uint8_t fontSize = InfoTypeData[type][0];
    //if negative, this signals "auto"
    /*
    if (value < 0){
        addToFramebuffer(InfoTypeData[type][3], InfoTypeData[type][1], letters5x7['a'-'a'], 5, 7, inverted);
        addToFramebuffer(InfoTypeData[type][2], InfoTypeData[type][1], letters5x7['t'-'a'], 5, 7, inverted);
    } else 
     */
    if (fontSize == SMALL_N || fontSize == SMALL_L){
        while (digit < 4 && InfoTypeData[type][2+digit] != 0){
            uint8_t digitValue = (value == 0 && fontSize == SMALL_N && digit != 0) ? 10 : value % 10; //show blank for leading 0s if SMALL_N
            addToFramebuffer(InfoTypeData[type][2+digit], InfoTypeData[type][1], nums3x5[digitValue], 3, 5, inverted);
            value /= 10;
            digit++;
        }
    } else if (fontSize == LARGE_N || fontSize == LARGE_L){
        while (digit < 4 && InfoTypeData[type][2+digit] != 0){
            uint8_t digitValue = (value == 0 && fontSize == LARGE_N && digit != 0) ? 10 : value % 10; //show blank for leading 0s if LARGE_N
            addToFramebuffer(InfoTypeData[type][2+digit], InfoTypeData[type][1], nums5x7[digitValue], 5, 7, inverted);
            value /= 10;
            digit++;
        }
    } else if (fontSize == DOUBLE){
        while (digit < 4 && InfoTypeData[type][2+digit] != 0){
            uint8_t digitValue = value % 10;
            addToFramebuffer(InfoTypeData[type][2+digit], InfoTypeData[type][1], nums3x5[digitValue], 3, 5, inverted, true);
            value /= 10;
            digit++;
        }
    }
}


void Display::invertInfo(InfoType type, bool set) {
    //if invert is different, then invert all the required info
    if (isInverted(type) != set){
        //flip flag
        setInverted(type, set);

        //invert pixels around value
        uint8_t left, bottom, right, top;
        getBoundingBox(type, left, bottom, right, top);


        //could definitely optimize using bitwise operations
        for (uint8_t x = left; x <= right; x++) {
            for (uint8_t y = top; y <= bottom; y++) {
                //invert pixel in framebuffer
                uint8_t page = y / PAGE_SIZE;
                uint8_t bit = y % PAGE_SIZE;
                framebuffer[page][x] ^= (1 << bit);
            }
        }
    }
}


void Display::boxInfo(InfoType type, bool set) {
    // Draw box around specified data in framebuffer
    uint8_t left, bottom, right, top;
    getBoundingBox(type, left, bottom, right, top);

    const uint8_t dataToUse[] = {(uint8_t)(set ? 0xFF : 0x00)};

    addToFramebuffer( left-1, bottom, dataToUse, 1, bottom-top+1); //left line
    addToFramebuffer(right+1, bottom, dataToUse, 1, bottom-top+1); //right line

    for (uint8_t x = left-1; x <= right+1; x++) {
        addToFramebuffer(x, bottom+1, dataToUse, 1, 1); //bottom line
        addToFramebuffer(x, top-1, dataToUse, 1, 1); //top line
    }   
}

void Display::setStatus(const char* msg, uint8_t offset, bool fill_spaces) {
    uint8_t cursorx = InfoTypeData[STATUS][2];
    const uint8_t cursory = InfoTypeData[STATUS][1];
    const uint8_t charSize = InfoTypeData[STATUS][5];
    const uint8_t max_cursorx = InfoTypeData[STATUS][3] + cursorx - charSize;
    const uint8_t height = InfoTypeData[STATUS][4];

    //if true, fill the rest with spaces until max_cursorx is reached
    bool spaceFlag = false;

    cursorx += offset*(charSize + 1);

    char cur;
    while (cursorx <= max_cursorx){
        if (spaceFlag){
            cur = ' ';
        } else {
            cur = *msg;
        }
        //verify this is a proper character defined in interfaceTemplate.hpp
        if (cur >= 'A' && cur <= '^') {
            addToFramebuffer(cursorx, cursory, letters5x7[static_cast<uint8_t>(cur-'A')], charSize, height, isInverted(STATUS));
        } else if (cur >= '0' && cur <= '9'){
            //number
            addToFramebuffer(cursorx, cursory, nums5x7[static_cast<uint8_t>(cur-'0')], charSize, height, isInverted(STATUS));
        } else if (cur == ' ') {
            //space, use blank character (last character in nums5x7)
            addToFramebuffer(cursorx, cursory, nums5x7[10], charSize, height, isInverted(STATUS));
        } else {
            //if character is not valid, show as a space
            addToFramebuffer(cursorx, cursory, nums5x7[10], charSize, height, isInverted(STATUS));
        }
        cursorx += charSize + 1;
        msg++;
        if (msg == nullptr || *msg == '\0'){
            if (fill_spaces){
                spaceFlag = true; //if we reach the end of the string, fill the rest with spaces
            } else {
                break;
            }
        }
    }
}

void Display::setStatus(const char* msg) {
    setStatus(msg, 0, true);
}

void Display::setBrightness(uint8_t brightness) {
    // Set backlight brightness using PWM
    HAL_TIM_PWM_Start(BKLT_TIMER, BKLT_PWM_CHANNEL);
    __HAL_TIM_SET_COMPARE(BKLT_TIMER, BKLT_PWM_CHANNEL, brightness);
}

//for testing, sets all info types to the same value to easily check for bounding box and highlight issues
void Display::setAllInfo(uint16_t byte) {
    uint8_t numInfos = sizeof(InfoTypeData)/sizeof(InfoTypeData[0]);
    for (uint8_t type = 0; type < numInfos; type++) {
        setInfo(static_cast<InfoType>(type), byte);
    }
}


//private functions
void Display::addToFramebuffer(uint8_t x, uint8_t y, const uint8_t *data, uint8_t width, uint8_t height, bool inverted, bool doubled) {
    if(doubled){
        // Precomputed: each index i (0-15) maps to its bits doubled
        static constexpr uint8_t doubledBits[16] = {
            0b00000000, // 0000 -> 00000000
            0b00000011, // 0001 -> 00000011
            0b00001100, // 0010 -> 00001100
            0b00001111, // 0011 -> 00001111
            0b00110000, // 0100 -> 00110000
            0b00110011, // 0101 -> 00110011
            0b00111100, // 0110 -> 00111100
            0b00111111, // 0111 -> 00111111
            0b11000000, // 1000 -> 11000000
            0b11000011, // 1001 -> 11000011
            0b11001100, // 1010 -> 11001100
            0b11001111, // 1011 -> 11001111
            0b11110000, // 1100 -> 11110000
            0b11110011, // 1101 -> 11110011
            0b11111100, // 1110 -> 11111100
            0b11111111, // 1111 -> 11111111
        };
        
        //note: these upper and lower refer to the upper and lower parts of the bytes that store the double-sized character, not the display pages
        //the LSB is higher than the MSB
        uint8_t doubleDataUpper[width*2];
        uint8_t doubleDataLower[width*2];
        for (uint8_t i = 0; i < width; i++) {
            doubleDataUpper[2*i] = doubleDataUpper[2*i + 1] = doubledBits[(data[i] >> 4) & 0x0F];
            doubleDataLower[2*i] = doubleDataLower[2*i + 1] = doubledBits[(data[i] & 0x0F)]; // Lower 4 bits for lower page
        }

        //if height*2 is greater than one page, we need to split it into two pages
        if (height*2 > 8) {
            addToFramebuffer(x, y, doubleDataUpper, width*2, height*2-8, inverted); // Upper page
            addToFramebuffer(x, y-(height*2-8), doubleDataLower, width*2, 8, inverted); // Lower page
        } else {
            addToFramebuffer(x, y, doubleDataLower, width*2, height*2, inverted); // Lower page
        }

    } else {
        //TODO: figure out how to add scaling data (ex: large font)
        int8_t numOnLowerPage = (y%PAGE_SIZE + 1);
        int8_t numOnUpperPage = height - numOnLowerPage;
        uint8_t lower_page = y/PAGE_SIZE;
        uint8_t upper_page = lower_page - 1;

        //prevents data from spilling into other areas
        uint8_t dataGuard = 0xFF >> (8-height);
        uint8_t invertMask = inverted ? 0xFF : 0;

        //if data is only on one page
        if (numOnUpperPage <= 0){
            for (uint8_t col = x; col < x + width; col++) {
                //reset the bits in range on lower page
                framebuffer[lower_page][col] &= (0xFF << (numOnLowerPage) | 0xFF >> (8 + numOnUpperPage)); 
                //set the bits in range on lower page
                framebuffer[lower_page][col] |= ((data[col-x]^invertMask) & dataGuard) << (numOnLowerPage-height); 
            }

        //if data is split between two pages
        } else {
            for (uint8_t col = x; col < x + width; col++) {
                //reset the bits in range on lower page
                framebuffer[lower_page][col] &= (0xFF << (numOnLowerPage)); 
                //set the bits in range on lower page
                framebuffer[lower_page][col] |= ((data[col-x]^invertMask) & dataGuard) >> (numOnUpperPage); 

                //reset the bits in range on upper page
                framebuffer[upper_page][col] &= (0xFF >> (numOnUpperPage)); 
                //set the bits in range on upper page
                framebuffer[upper_page][col] |= ((data[col-x]^invertMask) & dataGuard) << (8-numOnUpperPage); 
            }
            needsRefresh |= (1 << upper_page);
        }
        needsRefresh |= (1 << lower_page);
    }
}

//puts the bounding box data into the parameters passed by reference
void Display::getBoundingBox(InfoType type, uint8_t &left, uint8_t &bottom, uint8_t &right, uint8_t &top) {
    //MSG is different
    if (InfoTypeData[type][0] == MSG){
        bottom = InfoTypeData[type][1];
        left = InfoTypeData[type][2];
        right = left + InfoTypeData[type][3] - 1;
        top = bottom - InfoTypeData[type][4] + 1;
        return;
    }
    //determine number of digits to know width of the data
    uint8_t num_digits = 1;
    while (num_digits < 4 && InfoTypeData[type][3+num_digits] != 0) {
        num_digits++;
    }
    left = InfoTypeData[type][2+num_digits];
    bottom = InfoTypeData[type][1];

    if (InfoTypeData[type][0] == SMALL_N || InfoTypeData[type][0] == SMALL_L) {
        right = InfoTypeData[type][2] + 2;
        top = bottom - 4;
    } else if (InfoTypeData[type][0] == LARGE_N || InfoTypeData[type][0] == LARGE_L) {
        right = InfoTypeData[type][2] + 4;
        top = bottom - 6;
    } else if (InfoTypeData[type][0] == DOUBLE) {
        //HANDLE DOUBLE SIZE
    }
}

bool Display::isInverted(InfoType type){
    return infoInvertedMask & (1 << type);
}

void Display::setInverted(InfoType type, bool set){
    if (set){
        infoInvertedMask |= (1 << type);
    } else {
        infoInvertedMask &= ~(1 << type);
    }
}

//COMMUNICATION FUNCTIONS
HAL_StatusTypeDef Display::sendData(const uint8_t* data, size_t length) {
    // Send data via SPI
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(A0_PORT, A0_PIN, GPIO_PIN_SET);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(SPI_HANDLE, (uint8_t*)data, length, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET);
    return status;
}
 
HAL_StatusTypeDef Display::sendCommand(const uint8_t* cmd, size_t length) {
    // Send command via SPI
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(A0_PORT, A0_PIN, GPIO_PIN_RESET);
    HAL_StatusTypeDef status = HAL_SPI_Transmit(SPI_HANDLE, (uint8_t*)cmd, length, HAL_MAX_DELAY);
    HAL_GPIO_WritePin(CS_PORT, CS_PIN, GPIO_PIN_SET);
    return status;
}