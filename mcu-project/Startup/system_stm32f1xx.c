#include "stm32f1xx.h" 

#ifndef HSE_VALUE 
#define HSE_VALUE 8000000 
#endif 

#ifndef HSI_VALUE 
#define HSI_VALUE 8000000 
#endif 

#define SYSCLK_HZ 72000000 

uint32_t SystemCoreClock = SYSCLK_HZ; 

const uint8_t AHBPrescTable[16] = { 
    0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9
};
const uint8_t APBPrescTable[8] = {0, 0, 0, 0, 1, 2, 3, 4}; 

void SystemInit(void) 
{
    /* Bat prefetch, cho Flash 2 chu ky. */
    FLASH->ACR = FLASH_ACR_PRFTBE | FLASH_ACR_LATENCY_2; 

    /* Bat HSE va cho dao dong on dinh. */
    RCC->CR |= RCC_CR_HSEON; 
    while ((RCC->CR & RCC_CR_HSERDY) == 0) { 
    }

    RCC->CFGR &= ~(RCC_CFGR_SW | RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | 
                   RCC_CFGR_PPRE2 | RCC_CFGR_PLLSRC | 
                   RCC_CFGR_PLLXTPRE | RCC_CFGR_PLLMULL); 
    /* HSE 8 MHz x 9 = 72 MHz; APB1 = 36 MHz, APB2 = 72 MHz. */
    RCC->CFGR |= RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV2 | 
                 RCC_CFGR_PPRE2_DIV1 | RCC_CFGR_PLLSRC | 
                 RCC_CFGR_PLLMULL9; 

    /* Bat PLL va cho clock on dinh. */
    RCC->CR |= RCC_CR_PLLON; 
    while ((RCC->CR & RCC_CR_PLLRDY) == 0) { 
    }

    /* Chuyen clock he thong sang PLL. */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL; 
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) { 
    }

    SystemCoreClock = SYSCLK_HZ; 
}

/* Tinh lai clock CPU tu cau hinh RCC hien tai. */
void SystemCoreClockUpdate(void) 
{
    uint32_t system_clock; 

    switch (RCC->CFGR & RCC_CFGR_SWS) { 
    case RCC_CFGR_SWS_HSE: 
        system_clock = HSE_VALUE; 
        break; 

    case RCC_CFGR_SWS_PLL: { 
        uint32_t pll_multiplier =
            ((RCC->CFGR & RCC_CFGR_PLLMULL) >> RCC_CFGR_PLLMULL_Pos) + 2;

        if ((RCC->CFGR & RCC_CFGR_PLLSRC) != 0) { 
            uint32_t pll_input = HSE_VALUE; 

            if ((RCC->CFGR & RCC_CFGR_PLLXTPRE) != 0) { 
                pll_input /= 2;
            }
            system_clock = pll_input * pll_multiplier;
        } else { 
            system_clock = (HSI_VALUE / 2) * pll_multiplier;
        }
        break; 
    }

    case RCC_CFGR_SWS_HSI: 
    default: 
        system_clock = HSI_VALUE; 
        break; 
    }

    SystemCoreClock = system_clock >>
        AHBPrescTable[(RCC->CFGR & RCC_CFGR_HPRE) >> RCC_CFGR_HPRE_Pos];
}

/* Startup goi truoc main; du an C khong can ham khoi tao tinh. */
void __libc_init_array(void) 
{
}