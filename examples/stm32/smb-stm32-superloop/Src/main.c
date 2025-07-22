/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
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
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "simple_modbus.h"
#include "simple_modbus_rtu.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define MEASURE_TIME
#define INVALID       (42)
#define ADDRESS_START (1000)
#define ADDRESS_END   (1199)
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

TIM_HandleTypeDef htim14;
TIM_HandleTypeDef htim16;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
static uint16_t regs[200] = {0};
static uint8_t byte = 0;
volatile bool timer_elapsed = false;
volatile bool did_receive_frame = false;
static bool is_new_duration = false;
#ifdef MEASURE_TIME
static uint32_t duration_us = 0;
#endif
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM14_Init(void);
static void MX_TIM16_Init(void);
/* USER CODE BEGIN PFP */
#ifdef __GNUC__
#define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
#define PUTCHAR_PROTOTYPE int fputc(int ch, FILE* f)
#endif

PUTCHAR_PROTOTYPE
{
    HAL_UART_Transmit(&huart2, (uint8_t*)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

// CALLBACK FUNCTIONS FOR SIMPLE-MODBUS RTU
static void frame_received(void)
{
    BSP_LED_Toggle(LED_GREEN);

    if (did_receive_frame)
    {
        // Here we would have a timing problem.
        __BKPT();
    }
    else
    {
        // Notify main loop that we received a frame.
        did_receive_frame = true;
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
        if (i == length)
        {
#ifdef MEASURE_TIME
            if (is_new_duration)
            {
                __BKPT();
            }
            else
            {
                HAL_TIM_Base_Stop(&htim16);
                duration_us = __HAL_TIM_GET_COUNTER(&htim16);
            }
#endif
            is_new_duration = true;
        }
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

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
    /* USER CODE BEGIN 1 */

    /* USER CODE END 1 */

    /* MCU Configuration--------------------------------------------------------*/

    /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
    HAL_Init();

    /* USER CODE BEGIN Init */

    /* USER CODE END Init */

    /* Configure the system clock */
    SystemClock_Config();

    /* USER CODE BEGIN SysInit */

    /* USER CODE END SysInit */

    /* Initialize all configured peripherals */
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    MX_USART2_UART_Init();
    MX_TIM14_Init();
    MX_TIM16_Init();
    /* USER CODE BEGIN 2 */

    // Configure simple-modbus
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
    if (0 == smb_rtu_config(0x01, 115200, &rtu_if))
    {
        printf("Successfully confgured smb rtu\r\n");
        HAL_Delay(5);  // more than 3.5 char times
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
    /* USER CODE END 2 */

    /* Initialize leds */
    BSP_LED_Init(LED_GREEN);

    /* Initialize USER push-button, will be used to trigger an interrupt each time it's pressed.*/
    BSP_PB_Init(BUTTON_USER, BUTTON_MODE_EXTI);

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
#ifdef MEASURE_TIME
    uint32_t rx_cnt = 0;
#endif
    int16_t poll_ret = 0;
    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */

        // Alternative: use LL to check if there is a byte in the receive buffer without a timeout,
        // which slows down the main loop.
        HAL_StatusTypeDef status = HAL_UART_Receive(&huart1, &byte, 1, 1);
        if (HAL_OK == status)
        {
#ifdef MEASURE_TIME
            // Optional, just for timing analysis
            if (rx_cnt == 0)
            {
                __HAL_TIM_SET_COUNTER(&htim16, 0);
                HAL_TIM_Base_Start(&htim16);
            }
            rx_cnt++;
#endif

            // Give byte to simple-modbus
            smb_rtu_receive(byte);
        }
        else if (HAL_TIMEOUT == status)
        {
            // No byte available...
        }
        else
        {
            // Unexpected, handle more gracefully
            __BKPT();
        }

        if (timer_elapsed)
        {
            timer_elapsed = false;
            smb_rtu_timer_timeout();
        }

        // Only poll if a frame was received or if
        // the poll function needs to be called again
        if (poll_ret == -EAGAIN || did_receive_frame)
        {
            did_receive_frame = false;
            poll_ret = smb_server_poll();
        }
        else if (poll_ret == 0)
        {
            // everything is fine
        }
        else
        {
            printf("Error in smb_server_poll(): %d\r\n", poll_ret);
            HAL_Delay(1000);
        }

#ifdef MEASURE_TIME
        if (is_new_duration)
        {
            printf("Duration: %lu us\r\n", duration_us);
            is_new_duration = false;
            rx_cnt = 0;
        }
#endif
    }
    /* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_FLASH_SET_LATENCY(FLASH_LATENCY_1);

    /** Initializes the RCC Oscillators according to the specified parameters
     * in the RCC_OscInitTypeDef structure.
     */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    /** Initializes the CPU, AHB and APB buses clocks
     */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief TIM14 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM14_Init(void)
{
    /* USER CODE BEGIN TIM14_Init 0 */

    /* USER CODE END TIM14_Init 0 */

    TIM_OC_InitTypeDef sConfigOC = {0};

    /* USER CODE BEGIN TIM14_Init 1 */

    /* USER CODE END TIM14_Init 1 */
    htim14.Instance = TIM14;
    htim14.Init.Prescaler = 48;
    htim14.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim14.Init.Period = 65535;
    htim14.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim14.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_Base_Init(&htim14) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_TIM_OC_Init(&htim14) != HAL_OK)
    {
        Error_Handler();
    }
    sConfigOC.OCMode = TIM_OCMODE_TIMING;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_OC_ConfigChannel(&htim14, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN TIM14_Init 2 */

    /* USER CODE END TIM14_Init 2 */
}

/**
 * @brief TIM16 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM16_Init(void)
{
    /* USER CODE BEGIN TIM16_Init 0 */

    /* USER CODE END TIM16_Init 0 */

    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

    /* USER CODE BEGIN TIM16_Init 1 */

    /* USER CODE END TIM16_Init 1 */
    htim16.Instance = TIM16;
    htim16.Init.Prescaler = 48;
    htim16.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim16.Init.Period = 65535;
    htim16.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim16.Init.RepetitionCounter = 0;
    htim16.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim16) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_TIM_OC_Init(&htim16) != HAL_OK)
    {
        Error_Handler();
    }
    sConfigOC.OCMode = TIM_OCMODE_TIMING;
    sConfigOC.Pulse = 0;
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
    sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
    if (HAL_TIM_OC_ConfigChannel(&htim16, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }
    sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
    sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
    sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
    sBreakDeadTimeConfig.DeadTime = 0;
    sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
    sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
    sBreakDeadTimeConfig.BreakFilter = 0;
    sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
    if (HAL_TIMEx_ConfigBreakDeadTime(&htim16, &sBreakDeadTimeConfig) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN TIM16_Init 2 */

    /* USER CODE END TIM16_Init 2 */
}

/**
 * @brief USART1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART1_UART_Init(void)
{
    /* USER CODE BEGIN USART1_Init 0 */

    /* USER CODE END USART1_Init 0 */

    /* USER CODE BEGIN USART1_Init 1 */

    /* USER CODE END USART1_Init 1 */
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 115200;
    huart1.Init.WordLength = UART_WORDLENGTH_9B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_EVEN;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
    {
        Error_Handler();
    }
    if (HAL_UARTEx_EnableFifoMode(&huart1) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN USART1_Init 2 */

    /* USER CODE END USART1_Init 2 */
}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void)
{
    /* USER CODE BEGIN USART2_Init 0 */

    /* USER CODE END USART2_Init 0 */

    /* USER CODE BEGIN USART2_Init 1 */

    /* USER CODE END USART2_Init 1 */
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX_RX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
    huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
    huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
    if (HAL_UART_Init(&huart2) != HAL_OK)
    {
        Error_Handler();
    }
    /* USER CODE BEGIN USART2_Init 2 */

    /* USER CODE END USART2_Init 2 */
}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
    /* USER CODE BEGIN MX_GPIO_Init_1 */
    /* USER CODE END MX_GPIO_Init_1 */

    /* GPIO Ports Clock Enable */
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOF_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    /* USER CODE BEGIN MX_GPIO_Init_2 */
    /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_TIM_OC_DelayElapsedCallback(TIM_HandleTypeDef* htim)
{
    /* USER CODE BEGIN Callback 0 */

    /* USER CODE END Callback 0 */
    if (htim->Instance == TIM14)
    {
        HAL_TIM_OC_Stop(&htim14, TIM_CHANNEL_1);
        if (timer_elapsed)
        {
            __BKPT();
        }
        timer_elapsed = true;
    }
    /* USER CODE BEGIN Callback 1 */

    /* USER CODE END Callback 1 */
}

/* USER CODE END 4 */

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
    /* USER CODE BEGIN Error_Handler_Debug */
    /* User can add his own implementation to report the HAL error return state */
    __disable_irq();
    while (1)
    {
    }
    /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t* file, uint32_t line)
{
    /* USER CODE BEGIN 6 */
    /* User can add his own implementation to report the file name and line number,
       ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
    /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
