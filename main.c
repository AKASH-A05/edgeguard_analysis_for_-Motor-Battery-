/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include <stdio.h>
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define DS18B20_PORT GPIOA
#define DS18B20_PIN  GPIO_PIN_1
#define DS18B20_COUNT 2
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;
UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
#define FFT_SIZE 128

static float fft_real[FFT_SIZE];
static float fft_imag[FFT_SIZE];
static volatile uint8_t tim2_tick = 0;

static uint8_t ds_rom[DS18B20_COUNT][8];
static uint8_t ds_found = 0;

static uint32_t ds_conversion_start = 0;
static uint8_t ds_conversion_pending = 0;
static uint8_t ds_conversion_started = 0;


/* Latest sensor values for ESP32 transmission */
static float latest_frequency = 0.0f;
static float latest_magnitude = 0.0f;
static float latest_temp1 = 0.0f;
static float latest_temp2 = 0.0f;

static uint8_t temp1_valid = 0;
static uint8_t temp2_valid = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
/* USER CODE BEGIN PFP */
static void FFT_128(float *real, float *imag);

static void DS18B20_InitTiming(void);
static void DS18B20_PinOutput(void);
static void DS18B20_PinInput(void);
static void DS18B20_WriteBit(uint8_t bit);
static uint8_t DS18B20_ReadBit(void);
static void DS18B20_WriteByte(uint8_t data);
static uint8_t DS18B20_ReadByte(void);
static uint8_t DS18B20_Reset(void);
static uint8_t DS18B20_ReadTemperature(const uint8_t rom[8],
                                       float *temperature);
static uint8_t DS18B20_Search(uint8_t rom[8],
                              uint8_t *last_discrepancy,
                              uint8_t *last_device);
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
  MX_USART2_UART_Init();
  MX_I2C1_Init();
  MX_TIM2_Init();

  if (HAL_TIM_Base_Start_IT(&htim2) != HAL_OK)
  {
      Error_Handler();
  }

  MX_USART1_UART_Init();
  HAL_UART_Transmit(&huart1,
                    (uint8_t *)"STM32 UART test\r\n",
                    17,
                    HAL_MAX_DELAY);
  /* USER CODE BEGIN 2 */

  uint8_t check;
  uint8_t data[14];

  printf("Checking MPU6050...\r\n");

  check = HAL_I2C_IsDeviceReady(&hi2c1, 0xD0, 2, 100);

  if (check == HAL_OK)
  {
      uint8_t wake = 0x00;

      HAL_I2C_Mem_Write(&hi2c1, 0xD0, 0x6B,
                        I2C_MEMADD_SIZE_8BIT,
                        &wake, 1, 100);

      printf("MPU6050 detected and initialized.\r\n");
  }
  else
  {
      printf("MPU6050 not responding.\r\n");
  }

  DS18B20_InitTiming();

  printf("Searching for DS18B20 sensors...\r\n");

  uint8_t last_discrepancy = 0;
  uint8_t last_device = 0;
  uint8_t rom[8] = {0};

  while (ds_found < DS18B20_COUNT && !last_device)
  {
      if (DS18B20_Search(rom, &last_discrepancy, &last_device))
      {
          for (uint8_t j = 0; j < 8; j++)
              ds_rom[ds_found][j] = rom[j];

          printf("DS18B20 sensor %u found\r\n",
                 (unsigned int)(ds_found + 1));

          ds_found++;
      }
      else
      {
          break;
      }
  }

  printf("Total DS18B20 sensors found: %u\r\n",
         (unsigned int)ds_found);

  if (ds_found > 0 && DS18B20_Reset())
  {
      DS18B20_WriteByte(0xCC); /* SKIP ROM: all sensors */
      DS18B20_WriteByte(0x44); /* CONVERT T */

      ds_conversion_start = HAL_GetTick();
      ds_conversion_pending = 1;
      ds_conversion_started = 1;
  }




  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while(1)
  {

	    float sum = 0.0f;
	    uint8_t sample_failed = 0;

	    for (int i = 0; i < FFT_SIZE; i++)
	    {
	        uint8_t data[14];

	        /* Wait for the next TIM2 update */
	        while (tim2_tick == 0)
	        {
	            /* Wait */
	        }

	        __disable_irq();
	        tim2_tick = 0;
	        __enable_irq();

	        /* Read MPU6050 accelerometer data */
	        if (HAL_I2C_Mem_Read(&hi2c1, 0xD0, 0x3B,
	                             I2C_MEMADD_SIZE_8BIT,
	                             data, 14, 100) != HAL_OK)
	        {
	            printf("Sensor read failed!\r\n");
	            sample_failed = 1;
	            break;
	        }

	        int16_t ax = (int16_t)((data[0] << 8) | data[1]);

	        fft_real[i] = (float)ax;
	        fft_imag[i] = 0.0f;
	        sum += fft_real[i];
	    }

	    /* Skip FFT if any sensor read failed */
	    if (sample_failed)
	    {
	        HAL_Delay(100);
	        continue;
	    }

	    /* Remove average offset */
	    float average = sum / FFT_SIZE;

	    for (int i = 0; i < FFT_SIZE; i++)
	    {
	        fft_real[i] -= average;
	    }

	    /* Perform FFT */
	    FFT_128(fft_real, fft_imag);

	    /* Find dominant frequency */
	    float max_magnitude = 0.0f;
	    int peak_bin = 1;

	    for (int i = 1; i < FFT_SIZE / 2; i++)
	    {
	        float magnitude = sqrtf(
	            fft_real[i] * fft_real[i] +
	            fft_imag[i] * fft_imag[i]
	        );

	        if (magnitude > max_magnitude)
	        {
	            max_magnitude = magnitude;
	            peak_bin = i;
	        }
	    }

	    /* 500 Hz sample rate, 128 samples */
	    float frequency = (float)peak_bin * 500.0f / FFT_SIZE;

	    printf("Dominant vibration frequency: %.2f Hz\r\n", frequency);
	    printf("FFT peak magnitude: %.2f\r\n", max_magnitude);
	    latest_frequency = frequency;
	    latest_magnitude = max_magnitude;




	    /* Read temperatures after conversion completes. */
	    if (ds_found > 0 &&
	        ds_conversion_pending &&
	        (HAL_GetTick() - ds_conversion_start >= 750U))
	    {
	        for (uint8_t i = 0; i < ds_found; i++)
	        {
	            float temperature;


	            if (DS18B20_ReadTemperature(ds_rom[i], &temperature))
	            {
	                printf("Temperature %u: %.2f C\r\n",
	                       (unsigned int)(i + 1), temperature);

	                if (i == 0)
	                {
	                    latest_temp1 = temperature;
	                    temp1_valid = 1;
	                }
	                else if (i == 1)
	                {
	                    latest_temp2 = temperature;
	                    temp2_valid = 1;
	                }
	            }

	            else
	            {
	                printf("Temperature %u read failed\r\n",
	                       (unsigned int)(i + 1));
	            }
	        }

	        ds_conversion_pending = 0;
	    }

	    /* Start the next conversion once the previous one was read. */
	    if (ds_found > 0 && !ds_conversion_pending)
	    {
	        if (DS18B20_Reset())
	        {
	            DS18B20_WriteByte(0xCC);
	            DS18B20_WriteByte(0x44);

	            ds_conversion_start = HAL_GetTick();
	            ds_conversion_pending = 1;
	        }
	    }


	    /* Send one complete sensor packet to the ESP32 */
	    if (temp1_valid && temp2_valid)
	    {
	        char uart_packet[160];

	        int packet_len = snprintf(
	            uart_packet,
	            sizeof(uart_packet),
	            "{\"frequency_hz\":%.2f,"
	            "\"fft_magnitude\":%.2f,"
	            "\"temp1_c\":%.2f,"
	            "\"temp2_c\":%.2f}\r\n",
	            latest_frequency,
	            latest_magnitude,
	            latest_temp1,
	            latest_temp2
	        );

	        if (packet_len > 0 &&
	            packet_len < (int)sizeof(uart_packet))
	        {
	            HAL_UART_Transmit(
	                &huart1,
	                (uint8_t *)uart_packet,
	                (uint16_t)packet_len,
	                100
	            );
	        }
	    }

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE3);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 16;
  RCC_OscInitStruct.PLL.PLLN = 336;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
  RCC_OscInitStruct.PLL.PLLQ = 2;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 83;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 1999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

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
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
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
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_1|LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : PA1 */
  GPIO_InitStruct.Pin = GPIO_PIN_1;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* ---------- DS18B20 1-Wire driver on PA1 ---------- */

static void DS18B20_InitTiming(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    DWT->CYCCNT = 0;
}

static void DS18B20_DelayUs(uint32_t us)
{
    uint32_t start = DWT->CYCCNT;
    uint32_t cycles = (SystemCoreClock / 1000000U) * us;

    while ((uint32_t)(DWT->CYCCNT - start) < cycles) {}
}

static void DS18B20_PinOutput(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = DS18B20_PIN;
    gpio.Mode = GPIO_MODE_OUTPUT_OD;
    gpio.Pull = GPIO_NOPULL;
    gpio.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DS18B20_PORT, &gpio);
}

static void DS18B20_PinInput(void)
{
    GPIO_InitTypeDef gpio = {0};

    gpio.Pin = DS18B20_PIN;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DS18B20_PORT, &gpio);
}

static void DS18B20_WriteBit(uint8_t bit)
{
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    DS18B20_PinOutput();
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);

    if (bit)
    {
        DS18B20_DelayUs(6);
        HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
        DS18B20_DelayUs(64);
    }
    else
    {
        DS18B20_DelayUs(60);
        HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
        DS18B20_DelayUs(10);
    }

    if (!primask) __enable_irq();
}

static uint8_t DS18B20_ReadBit(void)
{
    uint8_t bit;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    DS18B20_PinOutput();
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
    DS18B20_DelayUs(3);

    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
    DS18B20_PinInput();
    DS18B20_DelayUs(10);

    bit = (HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN)
           == GPIO_PIN_SET) ? 1U : 0U;

    DS18B20_DelayUs(53);

    if (!primask) __enable_irq();
    return bit;
}

static void DS18B20_WriteByte(uint8_t data)
{
    for (uint8_t i = 0; i < 8; i++)
    {
        DS18B20_WriteBit(data & 1U);
        data >>= 1;
    }
}

static uint8_t DS18B20_ReadByte(void)
{
    uint8_t data = 0;

    for (uint8_t i = 0; i < 8; i++)
    {
        if (DS18B20_ReadBit())
            data |= (1U << i);
    }

    return data;
}

static uint8_t DS18B20_Reset(void)
{
    uint8_t presence;
    uint32_t primask = __get_PRIMASK();
    __disable_irq();

    DS18B20_PinOutput();
    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_RESET);
    DS18B20_DelayUs(480);

    HAL_GPIO_WritePin(DS18B20_PORT, DS18B20_PIN, GPIO_PIN_SET);
    DS18B20_PinInput();
    DS18B20_DelayUs(70);

    presence = (HAL_GPIO_ReadPin(DS18B20_PORT, DS18B20_PIN)
                == GPIO_PIN_RESET) ? 1U : 0U;

    DS18B20_DelayUs(410);

    if (!primask) __enable_irq();
    return presence;
}

static uint8_t DS18B20_CRC8(const uint8_t *data, uint8_t len)
{
    uint8_t crc = 0;

    for (uint8_t i = 0; i < len; i++)
    {
        uint8_t inbyte = data[i];

        for (uint8_t j = 0; j < 8; j++)
        {
            uint8_t mix = (crc ^ inbyte) & 1U;
            crc >>= 1;

            if (mix) crc ^= 0x8CU;

            inbyte >>= 1;
        }
    }

    return crc;
}

/* Search ROM: discovers each sensor's unique 64-bit address. */
static uint8_t DS18B20_Search(uint8_t rom[8],
                              uint8_t *last_discrepancy,
                              uint8_t *last_device)
{
    uint8_t id_bit, cmp_bit, direction;
    uint8_t last_zero = 0;
    uint8_t rom_byte = 0, rom_mask = 1;

    if (*last_device || !DS18B20_Reset())
        return 0;

    DS18B20_WriteByte(0xF0); /* SEARCH ROM */

    for (uint8_t bit_number = 1; bit_number <= 64; bit_number++)
    {
        id_bit = DS18B20_ReadBit();
        cmp_bit = DS18B20_ReadBit();

        if (id_bit && cmp_bit)
            return 0;

        if (id_bit != cmp_bit)
        {
            direction = id_bit;
        }
        else
        {
            if (bit_number < *last_discrepancy)
                direction = (rom[rom_byte] & rom_mask) ? 1U : 0U;
            else
                direction = (bit_number == *last_discrepancy) ? 1U : 0U;

            if (direction == 0U)
                last_zero = bit_number;
        }

        if (direction)
            rom[rom_byte] |= rom_mask;
        else
            rom[rom_byte] &= (uint8_t)~rom_mask;

        DS18B20_WriteBit(direction);

        rom_mask <<= 1;

        if (rom_mask == 0)
        {
            rom_byte++;
            rom_mask = 1;
        }
    }

    *last_discrepancy = last_zero;

    if (last_zero == 0)
        *last_device = 1;

    return (DS18B20_CRC8(rom, 7) == rom[7]);
}

static uint8_t DS18B20_ReadTemperature(const uint8_t rom[8],
                                       float *temperature)
{
    uint8_t scratchpad[9];

    if (!DS18B20_Reset())
        return 0;

    DS18B20_WriteByte(0x55); /* MATCH ROM */

    for (uint8_t i = 0; i < 8; i++)
        DS18B20_WriteByte(rom[i]);

    DS18B20_WriteByte(0xBE); /* READ SCRATCHPAD */

    for (uint8_t i = 0; i < 9; i++)
        scratchpad[i] = DS18B20_ReadByte();

    if (DS18B20_CRC8(scratchpad, 8) != scratchpad[8])
        return 0;

    int16_t raw = (int16_t)(((uint16_t)scratchpad[1] << 8)
                           | scratchpad[0]);

    *temperature = (float)raw / 16.0f;

    return 1;
}




int __io_putchar(int ch)
{
    HAL_UART_Transmit(&huart2,
                      (uint8_t *)&ch,
                      1,
                      HAL_MAX_DELAY);
    return ch;
}


static void FFT_128(float *real, float *imag)
{
    unsigned int i, j, k, m;
    unsigned int n = FFT_SIZE;

    /* Bit-reversal permutation */
    j = 0;
    for (i = 1; i < n; i++)
    {
        unsigned int bit = n >> 1;

        while (j & bit)
        {
            j ^= bit;
            bit >>= 1;
        }

        j ^= bit;

        if (i < j)
        {
            float temp = real[i];
            real[i] = real[j];
            real[j] = temp;

            temp = imag[i];
            imag[i] = imag[j];
            imag[j] = temp;
        }
    }

    /* Iterative radix-2 FFT */
    for (m = 2; m <= n; m <<= 1)
    {
        float angle = -6.28318530718f / (float)m;
        float wlen_real = cosf(angle);
        float wlen_imag = sinf(angle);

        for (k = 0; k < n; k += m)
        {
            float wr = 1.0f;
            float wi = 0.0f;

            for (j = 0; j < m / 2; j++)
            {
                unsigned int even = k + j;
                unsigned int odd  = even + m / 2;

                float tr = wr * real[odd] - wi * imag[odd];
                float ti = wr * imag[odd] + wi * real[odd];

                real[odd] = real[even] - tr;
                imag[odd] = imag[even] - ti;

                real[even] += tr;
                imag[even] += ti;

                float next_wr = wr * wlen_real - wi * wlen_imag;
                wi = wr * wlen_imag + wi * wlen_real;
                wr = next_wr;
            }
        }
    }
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM2)
    {
        tim2_tick = 1;
    }
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
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
