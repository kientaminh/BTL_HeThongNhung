#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

// STM32F103C8: HCLK 72 MHz, NVIC co 4 bit uu tien.
#define configCPU_CLOCK_HZ                    72000000
#define configTICK_RATE_HZ                    1000
#define configPRIO_BITS                       4
#define configKERNEL_INTERRUPT_PRIORITY      (15 << 4)
#define configMAX_SYSCALL_INTERRUPT_PRIORITY (5 << 4)
#define configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY 5

#define configUSE_PREEMPTION                  1
#define configMAX_PRIORITIES                  3
#define configMINIMAL_STACK_SIZE             128
#define configMAX_TASK_NAME_LEN              12
#define configUSE_16_BIT_TICKS                0
#define configUSE_IDLE_HOOK                   1    // vApplicationIdleHook() trong main.c: __WFI()
// Khi khong task nao can chay >= 2 tick: tam dung SysTick va ngu mot mach toi ngat ke tiep
// (port.c vPortSuppressTicksAndSleep). SysTick 24 bit o 72 MHz: moi lan ngu toi da ~233 ms
#define configUSE_TICKLESS_IDLE               1
// Chi so 0: ngat DMA ADC -> record; chi so 1: ngat nut EXTI1 -> record (BUTTON_NOTIFY_INDEX)
#define configTASK_NOTIFICATION_ARRAY_ENTRIES 2
#define configUSE_TICK_HOOK                   0
#define configUSE_TIMERS                      0
#define configUSE_MUTEXES                     0
#define configCHECK_FOR_STACK_OVERFLOW        2
#define configCHECK_HANDLER_INSTALLATION      1

// Loi khong phuc hoi: tat ngat, nhap nhay LED, treo (dinh nghia trong Src/main.c)
void app_fatal(void);

#define configASSERT(condition) \
    do                          \
    {                           \
        if (!(condition))       \
        {                       \
            app_fatal();        \
        }                       \
    } while (0)

// Khong can heap: TCB va stack cua recorder/idle duoc cap tinh.
#define configSUPPORT_STATIC_ALLOCATION      1
#define configKERNEL_PROVIDED_STATIC_MEMORY  1
#define configSUPPORT_DYNAMIC_ALLOCATION     0

#define INCLUDE_vTaskDelay                   1
#define INCLUDE_xTaskGetCurrentTaskHandle    1
// Do stack con trong cua task recorder (in "stack_free" sau moi lan ghi)
#define INCLUDE_uxTaskGetStackHighWaterMark  1

// Vector trong startup_stm32f103xb.s dung cac ten handler CMSIS.
#define vPortSVCHandler                      SVC_Handler
#define xPortPendSVHandler                   PendSV_Handler
#define xPortSysTickHandler                  SysTick_Handler

#endif /* FREERTOS_CONFIG_H */
