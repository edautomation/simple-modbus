/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * File Name          : app_freertos.c
 * Description        : FreeRTOS applicative file
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_freertos.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>

#include "simple_modbus.h"
#include "simple_modbus_rtu.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define ADDRESS_START (1000)
#define ADDRESS_END   (1199)

#define FLAG_TIMEOUT     0x01
#define FLAG_FRAME_READY 0x02
#define FLAG_AGAIN       0x04
#define FLAG_ERROR       (1 << 31)

#define ALL_FLAGS (FLAG_TIMEOUT | FLAG_FRAME_READY | FLAG_AGAIN)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */

extern TIM_HandleTypeDef htim14;
extern UART_HandleTypeDef huart1;
osMutexId_t rtuMutexHandle;
const osMutexAttr_t rtuMutex_attributes = {
    .name = "rtuMutex"};
static uint16_t regs[200] = {0};
/* USER CODE END Variables */
/* Definitions for modbusTask */
osThreadId_t modbusTaskHandle;
const osThreadAttr_t modbusTask_attributes = {
    .name = "modbusTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 256 * 4};
/* Definitions for rxTask */
osThreadId_t rxTaskHandle;
const osThreadAttr_t rxTask_attributes = {
    .name = "rxTask",
    .priority = (osPriority_t)osPriorityNormal,
    .stack_size = 256 * 4};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

static void frame_received(void)
{
    BSP_LED_Toggle(LED_GREEN);
    uint32_t flags = osThreadFlagsSet(modbusTaskHandle, FLAG_FRAME_READY);
    if (flags & FLAG_ERROR)
    {
        __BKPT();
    }
}
static void start_timer(uint16_t duration_us)
{
    HAL_NVIC_DisableIRQ(TIM14_IRQn);
    HAL_TIM_OC_Stop(&htim14, TIM_CHANNEL_1);
    __HAL_TIM_SET_COUNTER(&htim14, 0);
    __HAL_TIM_SET_COMPARE(&htim14, TIM_CHANNEL_1, duration_us);
    HAL_NVIC_EnableIRQ(TIM14_IRQn);
    HAL_TIM_OC_Start_IT(&htim14, TIM_CHANNEL_1);
}
static int16_t write_bytes(const uint8_t* bytes, uint16_t length)
{
    static const uint32_t TIMEOUT_MS = 1;

    HAL_StatusTypeDef status = HAL_OK;
    int16_t i = 0;
    while (HAL_OK == status && i < length)
    {
        status = HAL_UART_Transmit(&huart1, &bytes[i], 1, TIMEOUT_MS);
        i++;
    }
    if (HAL_TIMEOUT != status && HAL_OK != status)
    {
        return -status;
    }
    else
    {
        return i;
    }
}
// CALLBACK FUNCTIONS FOR SIMPLE-MODBUS SERVER
static int16_t read_regs(uint16_t* const buffer, uint16_t n_regs, uint16_t start_addr)
{
    int16_t result = n_regs - 1;

    bool is_address_in_range = (start_addr >= ADDRESS_START) && (start_addr <= ADDRESS_END);
    bool are_regs_in_range = (start_addr + n_regs) <= (ADDRESS_END + 1);
    if (is_address_in_range && are_regs_in_range)
    {
        uint16_t offset = start_addr - ADDRESS_START;
        for (uint16_t i = 0; i < n_regs; i++)
        {
            buffer[i] = regs[i + offset];
        }
        result = n_regs;
    }

    return result;
}
static int16_t write_regs(const uint16_t* const buffer, uint16_t n_regs, uint16_t start_addr)
{
    int16_t result = n_regs - 1;

    bool is_address_in_range = (start_addr >= ADDRESS_START) && (start_addr <= ADDRESS_END);
    bool are_regs_in_range = (start_addr + n_regs) <= (ADDRESS_END + 1);
    if (is_address_in_range && are_regs_in_range)
    {
        uint16_t offset = (start_addr - ADDRESS_START);
        for (uint16_t i = 0; i < n_regs; i++)
        {
            regs[i + offset] = buffer[i];
        }
        result = n_regs;
    }

    return result;
}

/* USER CODE END FunctionPrototypes */

/**
 * @brief  FreeRTOS initialization
 * @param  None
 * @retval None
 */
void MX_FREERTOS_Init(void)
{
    /* USER CODE BEGIN Init */

    printf("Hello, simple-modbus!\r\n");

    /* USER CODE END Init */

    /* USER CODE BEGIN RTOS_MUTEX */
    rtuMutexHandle = osMutexNew(&rtuMutex_attributes);
    if (NULL == rtuMutexHandle)
    {
        __BKPT();
    }
    /* USER CODE END RTOS_MUTEX */

    /* USER CODE BEGIN RTOS_SEMAPHORES */
    /* add semaphores, ... */
    /* USER CODE END RTOS_SEMAPHORES */

    /* USER CODE BEGIN RTOS_TIMERS */
    /* start timers, add new ones, ... */
    /* USER CODE END RTOS_TIMERS */

    /* USER CODE BEGIN RTOS_QUEUES */

    /* USER CODE END RTOS_QUEUES */
    /* creation of modbusTask */
    modbusTaskHandle = osThreadNew(ModbusTask, NULL, &modbusTask_attributes);

    /* creation of rxTask */
    rxTaskHandle = osThreadNew(RxTask, NULL, &rxTask_attributes);

    /* USER CODE BEGIN RTOS_THREADS */

    /* USER CODE END RTOS_THREADS */

    /* USER CODE BEGIN RTOS_EVENTS */
    /* add events, ... */
    /* USER CODE END RTOS_EVENTS */
}
/* USER CODE BEGIN Header_ModbusTask */
/**
 * @brief Function implementing the modbusTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_ModbusTask */
void ModbusTask(void* argument)
{
    /* USER CODE BEGIN modbusTask */
    // Configure simple modbus
    struct smb_rtu_if_t rtu_if = {
        .frame_received = frame_received,
        .start_counter = start_timer,
        .write = write_bytes,
    };
    struct smb_transport_if_t transport = {
        .read_frame = smb_rtu_read_pdu,
        .write_frame = smb_rtu_write_pdu,
    };
    struct smb_server_if_t callbacks = {
        .read_input_regs = read_regs,
        .read_holding_regs = read_regs,
        .write_regs = write_regs,
    };

    osStatus_t sts = osMutexAcquire(rtuMutexHandle, 100);
    if (sts != osOK)
    {
        __BKPT();
    }

    // Initialize simple modbus
    if (0 == smb_rtu_config(0x01, 115200, &rtu_if))
    {
        printf("Successfully confgured smb rtu\r\n");
        osDelay(5);  // more than 3.5 char times
    }
    else
    {
        printf("Error configuring smb rtu!\r\n");
        __BKPT();
    }
    if (0 == smb_server_config(0x01, &transport, &callbacks))
    {
        printf("Successfully configured smb server\r\n");
    }
    else
    {
        printf("Error configuring smb server!\r\n");
        __BKPT();
    }
    osMutexRelease(rtuMutexHandle);

    while (1)
    {
        uint32_t flags = osThreadFlagsWait(ALL_FLAGS, osFlagsWaitAny, osWaitForever);
        if (flags & ~ALL_FLAGS)
        {
            __BKPT();
        }
        else
        {
            osStatus_t sts = osMutexAcquire(rtuMutexHandle, 100);
            if (sts != osOK)
            {
                __BKPT();
            }
            else
            {
                if (flags & FLAG_TIMEOUT)
                {
                    smb_rtu_timer_timeout();
                }
                if (flags & (FLAG_AGAIN | FLAG_FRAME_READY))
                {
                    int16_t ret = smb_server_poll();
                    if (-EAGAIN == ret)
                    {
                        sts = osThreadFlagsSet(modbusTaskHandle, FLAG_AGAIN);
                        if (sts != osOK)
                        {
                            __BKPT();
                        }
                    }
                    else if (0 != ret)
                    {
                        __BKPT();
                    }
                }
                osMutexRelease(rtuMutexHandle);
            }
        }
    }
    /* USER CODE END modbusTask */
}

/* USER CODE BEGIN Header_RxTask */
/**
 * @brief Function implementing the rxTask thread.
 * @param argument: Not used
 * @retval None
 */
/* USER CODE END Header_RxTask */
void RxTask(void* argument)
{
    /* USER CODE BEGIN rxTask */
    /* Infinite loop */
    for (;;)
    {
        uint8_t byte = 0;
        HAL_StatusTypeDef sts = HAL_UART_Receive(&huart1, &byte, 1, osWaitForever);
        if (sts != HAL_OK)
        {
            __BKPT();
        }
        else
        {
            osStatus_t sts = osMutexAcquire(rtuMutexHandle, 100);
            if (sts != osOK)
            {
                __BKPT();
            }
            else
            {
                smb_rtu_receive(byte);
                osMutexRelease(rtuMutexHandle);
            }
        }
    }
    /* USER CODE END rxTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef* htim)
{
    /* USER CODE BEGIN Callback 0 */

    /* USER CODE END Callback 0 */
    if (htim->Instance == TIM14)
    {
        HAL_TIM_OC_Stop(&htim14, TIM_CHANNEL_1);
        uint32_t flags = osThreadFlagsSet(modbusTaskHandle, FLAG_TIMEOUT);
        if (flags & FLAG_ERROR)
        {
            __BKPT();
        }
    }
    /* USER CODE BEGIN Callback 1 */

    /* USER CODE END Callback 1 */
}
void BSP_PB_Callback(Button_TypeDef Button)
{
    // Placeholder, nothing implemented yet

    BSP_PB_Init(BUTTON_USER, BUTTON_MODE_GPIO);
}
/* USER CODE END Application */
