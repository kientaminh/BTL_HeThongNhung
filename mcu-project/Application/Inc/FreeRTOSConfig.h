#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

// STM32F103C8: HCLK 72 MHz, NVIC co 4 bit uu tien.
#define configCPU_CLOCK_HZ                    72000000UL
#define configTICK_RATE_HZ                    1000U
#define configPRIO_BITS                       4
#define configKERNEL_INTERRUPT_PRIORITY      (15U << 4)
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5U << 4)
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5U

#define configUSE_PREEMPTION                  1
#define configMAX_PRIORITIES                  3
#define configMINIMAL_STACK_SIZE             128U
#define configMAX_TASK_NAME_LEN              12
#define configUSE_16_BIT_TICKS                0
#define configUSE_IDLE_HOOK                   0
#define configUSE_TICK_HOOK                   0
#define configUSE_TIMERS                      0
#define configUSE_MUTEXES                     0
#define configCHECK_FOR_STACK_OVERFLOW        2
#define configCHECK_HANDLER_INSTALLATION      1

#define configASSERT(condition)                                      \
    do                                                               \
    {                                                                \
        if (!(condition))                                             \
        {                                                            \
            __asm volatile ("cpsid i");                              \
            for (;;)                                                 \
            {                                                        \
            }                                                        \
        }                                                            \
    } while (0)

// Khong can heap: TCB va stack cua recorder/idle duoc cap tinh.
#define configSUPPORT_STATIC_ALLOCATION      1
#define configKERNEL_PROVIDED_STATIC_MEMORY  1
#define configSUPPORT_DYNAMIC_ALLOCATION     0

#define INCLUDE_vTaskDelay                   1
#define INCLUDE_xTaskGetCurrentTaskHandle    1

// Vector trong startup_stm32f103xb.s dung cac ten handler CMSIS.
#define vPortSVCHandler                      SVC_Handler
#define xPortPendSVHandler                   PendSV_Handler
#define xPortSysTickHandler                  SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
