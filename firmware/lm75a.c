#include "lm75a.h"

#define I2C_TIMEOUT                     50000

extern volatile uint32_t msTicks;
volatile uint32_t error_counter;
volatile uint32_t i2c_timeout_counter = 0;

#define _RCC_I2C1_CLK_ENABLE()   do { \
    __IO uint32_t tmpreg; \
    SET_BIT(RCC->APB1ENR, RCC_APB1ENR_I2C1EN);\
    tmpreg = READ_BIT(RCC->APB1ENR, RCC_APB1ENR_I2C1EN);\
    (void)tmpreg; \
} while(0U)

#define _RCC_AFIO_CLK_ENABLE()   do { \
    __IO uint32_t tmpreg; \
    SET_BIT(RCC->APB2ENR, RCC_APB2ENR_AFIOEN);\
    tmpreg = READ_BIT(RCC->APB2ENR, RCC_APB2ENR_AFIOEN);\
    (void)tmpreg; \
} while(0U)
  
#define _RCC_GPIOB_CLK_ENABLE()   do { \
    __IO uint32_t tmpreg; \
    SET_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPBEN);\
    tmpreg = READ_BIT(RCC->APB2ENR, RCC_APB2ENR_IOPBEN);\
    (void)tmpreg; \
} while(0U)

void Delay_US(uint32_t us) {
    uint32_t start = msTicks;
    while ((msTicks - start) < us) { __NOP(); }
}

uint8_t I2C_WaitForFlag(uint32_t flag, uint32_t timeout) {
    uint32_t timeout_cnt = timeout;
    while(!(I2C1->SR1 & flag)) {
        timeout_cnt--;
        if(timeout_cnt == 0) {
            return 0; // Timeout
        }
    }
    return 1;
}

void I2C_Init(void) {
    _RCC_AFIO_CLK_ENABLE();
    _RCC_GPIOB_CLK_ENABLE();
    
    SET_BIT(GPIOB->CRL, GPIO_CRL_CNF7_1 | GPIO_CRL_CNF6_1 | GPIO_CRL_CNF7_0 | GPIO_CRL_CNF6_0 |\
            GPIO_CRL_MODE7_1 | GPIO_CRL_MODE6_1 | GPIO_CRL_MODE7_0 | GPIO_CRL_MODE6_0);
    
    MODIFY_REG(I2C1->CR1, I2C_CR1_SMBUS | I2C_CR1_SMBTYPE | I2C_CR1_ENARP, I2C_MODE_I2C);
    
    _RCC_I2C1_CLK_ENABLE();
    
    CLEAR_BIT(I2C1->CR1, I2C_CR1_PE);
    SET_BIT(I2C1->CR1, I2C_CR1_SWRST);
    CLEAR_BIT(I2C1->CR1, I2C_CR1_SWRST);
    
    MODIFY_REG(I2C1->CR2, I2C_CR2_FREQ, 36);
    MODIFY_REG(I2C1->TRISE, I2C_TRISE_TRISE, 36 + 1);
    MODIFY_REG(I2C1->CCR, (I2C_CCR_FS | I2C_CCR_DUTY | I2C_CCR_CCR), 600);
    
    SET_BIT(I2C1->CR1, I2C_CR1_ACK);
    MODIFY_REG(I2C1->OAR1, 0xFFFF, I2C_OWNADDRESS1_7BIT);
    MODIFY_REG(I2C1->OAR2, I2C_OAR2_ADD2, 0);
    
    SET_BIT(I2C1->CR1, I2C_CR1_PE);
}


uint32_t Get_APB1_FREQ() {
    uint32_t freq = 0;
    uint32_t tmp = (RCC->CFGR & RCC_CFGR_PPRE1) >> 8;
    
    switch(tmp) {
        case 0x04: freq = SystemCoreClock / 2; break;
        case 0x05: freq = SystemCoreClock / 4; break;
        case 0x06: freq = SystemCoreClock / 8; break;
        case 0x07: freq = SystemCoreClock / 16; break;
        default: freq = SystemCoreClock; break;
    }
    return freq;
}

void I2C_Reset(void) {
    CLEAR_BIT(I2C1->CR1, I2C_CR1_PE);
    SET_BIT(I2C1->CR1, I2C_CR1_SWRST);
    CLEAR_BIT(I2C1->CR1, I2C_CR1_SWRST);
    MODIFY_REG(I2C1->CR2, I2C_CR2_FREQ, 36);
    MODIFY_REG(I2C1->TRISE, I2C_TRISE_TRISE, 36 + 1);
    MODIFY_REG(I2C1->CCR, (I2C_CCR_FS | I2C_CCR_DUTY | I2C_CCR_CCR), 120);
    
    SET_BIT(I2C1->CR1, I2C_CR1_ACK);
    MODIFY_REG(I2C1->OAR1, 0xFFFF, I2C_OWNADDRESS1_7BIT);
    
    SET_BIT(I2C1->CR1, I2C_CR1_PE);
}

uint8_t I2C_WriteData(uint8_t addr, uint8_t *buf, uint16_t bytes_count) {
    uint16_t i;
    uint32_t timeout;
    
    CLEAR_BIT(I2C1->CR1, I2C_CR1_POS);
    SET_BIT(I2C1->CR1, I2C_CR1_ACK);
    SET_BIT(I2C1->CR1, I2C_CR1_START);
    
    timeout = I2C_TIMEOUT;
    while(!READ_BIT(I2C1->SR1, I2C_SR1_SB) && timeout--);
    if(timeout == 0) {
        SET_BIT(I2C1->CR1, I2C_CR1_STOP);
        error_counter++;
        return 0;
    }
    (void) I2C1->SR1;
    
    I2C1->DR = SLAVE_OWN_ADDRESS | I2C_REQUEST_WRITE;
    
    timeout = I2C_TIMEOUT;
    while(!READ_BIT(I2C1->SR1, I2C_SR1_ADDR) && timeout--);
    if(timeout == 0) {
        SET_BIT(I2C1->CR1, I2C_CR1_STOP);
        error_counter++;
        return 0;
    }
    (void) I2C1->SR1;
    (void) I2C1->SR2;
    
    I2C1->DR = addr;
    timeout = I2C_TIMEOUT;
    while(!READ_BIT(I2C1->SR1, I2C_SR1_TXE) && timeout--);
    if(timeout == 0) {
        SET_BIT(I2C1->CR1, I2C_CR1_STOP);
        error_counter++;
        return 0;
    }
    
    for(i = 0; i < bytes_count; i++) {
        I2C1->DR = buf[i];
        timeout = I2C_TIMEOUT;
        while(!READ_BIT(I2C1->SR1, I2C_SR1_TXE) && timeout--);
        if(timeout == 0) {
            SET_BIT(I2C1->CR1, I2C_CR1_STOP);
            error_counter++;
            return 0;
        }
    }
    SET_BIT(I2C1->CR1, I2C_CR1_STOP);
    return 1;
}

uint8_t I2C_ReadData(uint8_t addr, uint8_t *buf, uint16_t bytes_count) {
    uint16_t i;
    uint32_t timeout;
    
    CLEAR_BIT(I2C1->CR1, I2C_CR1_POS);
    SET_BIT(I2C1->CR1, I2C_CR1_ACK);
    SET_BIT(I2C1->CR1, I2C_CR1_START);
    
    timeout = I2C_TIMEOUT;
    while(!READ_BIT(I2C1->SR1, I2C_SR1_SB) && timeout--);
    if(timeout == 0) {
        SET_BIT(I2C1->CR1, I2C_CR1_STOP);
        error_counter++;
        return 0;
    }
    (void) I2C1->SR1;
    
    I2C1->DR = SLAVE_OWN_ADDRESS | I2C_REQUEST_WRITE;
    timeout = I2C_TIMEOUT;
    while(!READ_BIT(I2C1->SR1, I2C_SR1_ADDR) && timeout--);
    if(timeout == 0) {
        SET_BIT(I2C1->CR1, I2C_CR1_STOP);
        error_counter++;
        return 0;
    }
    (void) I2C1->SR1;
    (void) I2C1->SR2;
    
    I2C1->DR = LM75B_Temp;
    timeout = I2C_TIMEOUT;
    while(!READ_BIT(I2C1->SR1, I2C_SR1_TXE) && timeout--);
    if(timeout == 0) {
        SET_BIT(I2C1->CR1, I2C_CR1_STOP);
        error_counter++;
        return 0;
    }
    
    SET_BIT(I2C1->CR1, I2C_CR1_START);
    timeout = I2C_TIMEOUT;
    while(!READ_BIT(I2C1->SR1, I2C_SR1_SB) && timeout--);
    if(timeout == 0) {
        SET_BIT(I2C1->CR1, I2C_CR1_STOP);
        error_counter++;
        return 0;
    }
    (void) I2C1->SR1;
    
    I2C1->DR = SLAVE_OWN_ADDRESS | I2C_REQUEST_READ;
    timeout = I2C_TIMEOUT;
    while(!READ_BIT(I2C1->SR1, I2C_SR1_ADDR) && timeout--);
    if(timeout == 0) {
        SET_BIT(I2C1->CR1, I2C_CR1_STOP);
        error_counter++;
        return 0;
    }
    (void) I2C1->SR1;
    (void) I2C1->SR2;
    
    for(i = 0; i < bytes_count; i++) {
        if(i < (bytes_count-1)) {
            timeout = I2C_TIMEOUT;
            while(!READ_BIT(I2C1->SR1, I2C_SR1_RXNE) && timeout--);
            if(timeout == 0) {
                SET_BIT(I2C1->CR1, I2C_CR1_STOP);
                error_counter++;
                return 0;
            }
            buf[i] = READ_BIT(I2C1->DR, I2C_DR_DR);
        } else {
            CLEAR_BIT(I2C1->CR1, I2C_CR1_ACK);
            SET_BIT(I2C1->CR1, I2C_CR1_STOP);
            timeout = I2C_TIMEOUT;
            while(!READ_BIT(I2C1->SR1, I2C_SR1_RXNE) && timeout--);
            if(timeout == 0) {
                error_counter++;
                return 0;
            }
            buf[i] = READ_BIT(I2C1->DR, I2C_DR_DR);
        }
    }
    return 1;
}

uint8_t I2C_IsDeviceReady(uint8_t devAddr) {
    uint32_t timeout = I2C_TIMEOUT;
    uint8_t ready = 0;
    
    I2C1->SR1 &= ~(I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_OVR);
    
    SET_BIT(I2C1->CR1, I2C_CR1_START);
    
    while(!READ_BIT(I2C1->SR1, I2C_SR1_SB) && timeout--);
    if(timeout == 0) {
        SET_BIT(I2C1->CR1, I2C_CR1_STOP);
        return 0;
    }
    
    I2C1->DR = (devAddr << 1) | I2C_REQUEST_WRITE;
    
    timeout = I2C_TIMEOUT / 10;
    while(!READ_BIT(I2C1->SR1, I2C_SR1_ADDR) && !READ_BIT(I2C1->SR1, I2C_SR1_AF) && timeout--);
    
    if(READ_BIT(I2C1->SR1, I2C_SR1_ADDR)) {
        ready = 1;
        (void)I2C1->SR2;
    }
    
    SET_BIT(I2C1->CR1, I2C_CR1_STOP);
    I2C1->SR1 &= ~(I2C_SR1_AF | I2C_SR1_BERR | I2C_SR1_ARLO);
    
    return ready;
}

uint8_t CheckAnyDevice(void) {
    uint8_t addr;
    static uint8_t reset_pending = 0;
    
    if(error_counter > 20) {
        I2C_Reset();
        for(volatile int i = 0; i < 10000; i++);
    }
    
    for(addr = LM75A_ADDR_START; addr <= LM75A_ADDR_END; addr++) {
        if(I2C_IsDeviceReady(addr)) {
            error_counter = 0; 
            return addr;
        }
    }
		error_counter++;
    return 0;
}

uint8_t LM75A_ReadTemperature(uint8_t addr, float *temp) {
    uint8_t read_data[2] = {0, 0};
    uint8_t write_data[1] = {0x00};
    int16_t received_data;
    
    if(addr < LM75A_ADDR_START || addr > LM75A_ADDR_END) {
        return 0;
    }
    
    if(!I2C_IsDeviceReady(addr)) {
        return 0;
    }
    
    if(!I2C_WriteData(addr, write_data, 1)) {
        return 0;
    }
    Delay_US(1);
    
    if(!I2C_ReadData(addr, read_data, 2)) {
        return 0;
    }
    
    received_data = (read_data[0] << 8) | read_data[1];
    
    if((received_data == 0xFFFF) || (received_data == 0x0000)) {
        return 0;
    }
    
    if(received_data & 0x8000) {
        *temp = (float)((int16_t)received_data) / 256.0f;
    } else {
        *temp = (float)received_data / 256.0f;
    }
    
    if(*temp < -55.0f || *temp > 125.0f) {
        return 0;
    }
    
    return 1;
}

uint8_t LM75A_IsConnected(uint8_t addr) {
    if(addr < LM75A_ADDR_START || addr > LM75A_ADDR_END) {
        return 0;
    }
    return I2C_IsDeviceReady(addr);
}