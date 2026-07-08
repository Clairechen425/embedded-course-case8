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
#include "tim.h"
#include "usb_device.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "gesture.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define SERVO_LR_CENTER  750   /* 左右舵机中位 CCR，调这个值到物理居中 */
#define SERVO_UD_CENTER  750   /* 上下舵机中位 CCR，调这个值到物理居中 */
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
static uint32_t led_tick = 0;

/* ── 舵机摆风状态 ── */
static uint8_t  servo_lr_on = 0;       /* 左右摆风开关 */
static int32_t  servo_lr_ccr = SERVO_LR_CENTER;    /* 当前 CCR (90°居中) */
static int8_t   servo_lr_dir = 1;      /* 1=右扫, -1=左扫 */

static uint8_t  servo_ud_on = 0;       /* 上下摆风开关 */
static int32_t  servo_ud_ccr = SERVO_UD_CENTER;    /* 当前 CCR (90°居中) */
static int8_t   servo_ud_dir = -1;      /* 启动时先向 CCD 减小方向扫 */

static uint32_t servo_tick_lr = 0;
static uint32_t servo_tick_ud = 0;
static uint8_t  system_on = 0;            /* 开机标志，palm置1，fist清0 */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

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
  MX_TIM3_Init();
  MX_USB_DEVICE_Init();
  MX_TIM4_Init();
  /* USER CODE BEGIN 2 */
  /* ── PB3/PB4 GPIO: L298N IN1/IN2 ── */
  __HAL_RCC_GPIOB_CLK_ENABLE();
  GPIO_InitTypeDef g = {0};
  g.Mode = GPIO_MODE_OUTPUT_PP;
  g.Pull = GPIO_NOPULL;
  g.Speed = GPIO_SPEED_FREQ_LOW;
  g.Pin = GPIO_PIN_3 | GPIO_PIN_4;
  HAL_GPIO_Init(GPIOB, &g);
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_3, GPIO_PIN_SET);   /* IN1=H, 正转 */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_4, GPIO_PIN_RESET); /* IN2=L */

  /* ── 启动 TIM3 PWM (PA6 → L298N ENA) ── */
  HAL_TIM_PWM_Start(&htim3, TIM_CHANNEL_1);

  /* ── 启动 TIM4 PWM (PB6 CH1 左右 + PB7 CH2 上下) ── */
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_1);
  HAL_TIM_PWM_Start(&htim4, TIM_CHANNEL_2);
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, SERVO_LR_CENTER);
  __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, SERVO_UD_CENTER);
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    /* ── 左右舵机扫描（预判式折返，无边界停顿）── */
    if (servo_lr_on && (HAL_GetTick() - servo_tick_lr >= 10)) {
        servo_tick_lr = HAL_GetTick();
        int32_t next = (int32_t)servo_lr_ccr + servo_lr_dir * 2;
        if (next > 700) {
            servo_lr_ccr = 698;      // 强制弹离上边界
            servo_lr_dir = -1;
        } else if (next < 200) {
            servo_lr_ccr = 204;       // 强制弹离下边界
            servo_lr_dir = 1;
        } else {
            servo_lr_ccr = (uint16_t)next;
        }
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, servo_lr_ccr);
    }

    /* ── 上下舵机扫描 ── */
    if (servo_ud_on && (HAL_GetTick() - servo_tick_ud >= 10)) {
        servo_tick_ud = HAL_GetTick();
        servo_ud_ccr += servo_ud_dir * 2;
        if (servo_ud_ccr > 800)  { servo_ud_ccr = 1600 - servo_ud_ccr; servo_ud_dir = -1; }
        if (servo_ud_ccr < 350)  { servo_ud_ccr = 700  - servo_ud_ccr; servo_ud_dir = 1;  }
        __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, servo_ud_ccr);
    }

    /* ── 手势控制 ── */
    if (g_gesture_ready) {
        g_gesture_ready = 0;
        int32_t gid = g_gesture.class_id;

        /* 电源指令总是有效，其他需先开机 */
        if (gid != 20 && gid != 24 && gid != 14 && !system_on)
            goto gesture_done;

        switch (gid) {
            /* ── 电源 ── */
            case 20: case 24:   /* palm/stop → 开机 */
                system_on = 1;
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 300);
                servo_lr_ccr = 420;
                __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 420);
                break;
            case 14:            /* fist → 全关 */
                system_on = 0;
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 0);
                servo_lr_on = 0; servo_lr_ccr = SERVO_LR_CENTER;
                servo_ud_on = 0; servo_ud_ccr = SERVO_UD_CENTER;
                __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, SERVO_LR_CENTER);
                __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, SERVO_UD_CENTER);
                break;

            /* ── 风扇（需开机）── */
            case 19: __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 300); break;
            case 21: case 22: case 28: case 29:
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 500); break;
            case 26: case 27: case 5:  case 30:
                __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 700); break;
            case 15: __HAL_TIM_SET_COMPARE(&htim3, TIM_CHANNEL_1, 900); break;

            /* ── 左右摆风（需开机）── */
            case 18:  /* ok → 开始扫风 */
                servo_lr_dir = 1;    /* 从420向右(800)扫，不碰左边界 */
                servo_lr_on = 1;  break;
            case 6:   /* timeout → 停扫，留在就位 */
                servo_lr_on = 0;
                servo_lr_ccr = 420;
                __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_1, 420);
                break;

            /* ── 上下摆风（需开机）── */
            case 16: servo_ud_on = 1; break;
            case 13:
                servo_ud_on = 0;
                servo_ud_ccr = SERVO_UD_CENTER;
                __HAL_TIM_SET_COMPARE(&htim4, TIM_CHANNEL_2, SERVO_UD_CENTER);
                break;
        }
        gesture_done:;

        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
        led_tick = HAL_GetTick();
    }
    if (HAL_GetTick() - led_tick > 100) {
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
    }
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
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL6;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_1) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_USB;
  PeriphClkInit.UsbClockSelection = RCC_USBCLKSOURCE_PLL;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

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
