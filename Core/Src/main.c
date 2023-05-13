/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2023 STMicroelectronics.
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
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "fonts.h"
#include "test.h"

#include "usbd_cdc_if.h"

#include "math.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
#define effectsNum 8
#define sampleRate 47743
#define usbBufferLen 256
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
#define EXECUTE_EFFECT(effect)\
		struct effect ## Config effect ## Params;\
		paramsLen = sizeof(effect ## Params);\
		memcpy(&effect ## Params, effectsData + i, paramsLen);\
		d = effect(d, effect ## Params);

#define LOAD_EFFECT(effect)\
		struct effect ## Config effect ## Params;\
		paramsLen = sizeof(effect ## Params);\
		memcpy(&effect ## Params, effectsData + i, paramsLen);

#define SAVE_EFFECT(effect)\
		memcpy(effectsData + i, &effect ## Params, paramsLen);
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

I2C_HandleTypeDef hi2c1;
I2C_HandleTypeDef hi2c2;

I2S_HandleTypeDef hi2s1;
I2S_HandleTypeDef hi2s2;
DMA_HandleTypeDef hdma_spi1_rx;
DMA_HandleTypeDef hdma_spi2_tx;

/* USER CODE BEGIN PV */
int8_t bufSize = 64;
int32_t inData[64];
int32_t outData[64];
int32_t process[32];
int32_t output[32];

int8_t inReady = 0;
int8_t outReady = 0;
int8_t execute = 1;

const float HALF_PI = M_PI / 2;
const float ONE_AND_HALF_PI = M_PI + HALF_PI;
const float TWO_PI = 2 * M_PI;

uint8_t effectsData[usbBufferLen];
uint8_t effectsRawData[usbBufferLen] = {0xff, 0x0, 0xff};
uint8_t usbDataBuffer[usbBufferLen];
uint32_t usbBytesReady = 0;

struct distConfig {
	float gain;
};

struct softdistConfig {
	float gain;
};

struct vibratoConfig {
	float *previousInputsPtr;
	int previousInputsLen;
	float frequency;
	float strength;
};

struct chorusConfig {
	float *previousInputsPtr;
	int previousInputsLen;
	float frequency;
	float strength;
};

struct echoConfig {
	float *previousOutputsPtr;
	int previousOutputsLen;
	float delay;
	float attenuation;
};

struct lowpassConfig {
	float *lastOutputPtr;
	float filterConst;
};

struct highpassConfig {
	float *lastSamplesPtr;
	float filterConst;
};

struct tremoloConfig {
	float frequency;
	float strength;
};

struct rotaryConfig {
	float *previousInputsPtr;
	int previousInputsLen;
	float frequency;
	float vibratoStrength;
	float tremoloStrength;
};

struct reverbConfig {
	struct echoConfig filtersParams[6];
	float dry_wet;
	float reverbTime;
};

struct gainConfig {
	float gain;
};

struct noiseSupprConfig {
	int *counter;
	float threshold;
};

struct fuzzConfig {
	uint8_t dummy;
};

const float dt = 1 / (float) sampleRate;
unsigned long generalInputN = 0;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_I2S1_Init(void);
static void MX_I2S2_Init(void);
static void MX_I2C2_Init(void);
static void MX_I2C1_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void crash(int code){
	if(code % 2 == 1){
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_3, 1);
	}
	if((code >> 1) % 2 == 1){
		HAL_GPIO_WritePin(GPIOE, GPIO_PIN_4, 1);
	}
	while(1);
}

void checkForNull(void* ptr){
	if(ptr == NULL){
		crash(3);
	}
}

float getReverbAttenuation(int samples, float time){
	float t = (float) samples / sampleRate;
	return 1 / (exp(t / time * log(1000)));
}

void loadEffects(){
	unsigned int i = 0;
	uint8_t effectId;
	unsigned int paramsLen;
	int channel = 1;
	memcpy(effectsData, effectsRawData, usbBufferLen);

	while(1){
		effectId = *(effectsData + i);
		i++;
		if(effectId == 0xff){
			if(channel == 1){
				channel = 2;
				continue;
			}else{
				break;
			}
		}

		if(effectId == 0){
			paramsLen = 0;

		}else if(effectId == 1){
			LOAD_EFFECT(dist)

		}else if(effectId == 2){
			LOAD_EFFECT(softdist)

		}else if(effectId == 3){
			LOAD_EFFECT(vibrato)
			vibratoParams.frequency = TWO_PI * vibratoParams.frequency / sampleRate;
			vibratoParams.strength = vibratoParams.strength / vibratoParams.frequency;
			vibratoParams.previousInputsLen = ceil(2 * vibratoParams.strength) + 3;
			vibratoParams.previousInputsPtr = (float*) calloc(vibratoParams.previousInputsLen, sizeof(float));
			checkForNull(vibratoParams.previousInputsPtr);
			SAVE_EFFECT(vibrato)

		}else if(effectId == 4){
			LOAD_EFFECT(chorus)
			chorusParams.frequency = TWO_PI * chorusParams.frequency / sampleRate;
			chorusParams.strength = chorusParams.strength / chorusParams.frequency;
			chorusParams.previousInputsLen = ceil(2 * chorusParams.strength) + 3;
			chorusParams.previousInputsPtr = (float*) calloc(chorusParams.previousInputsLen, sizeof(float));
			checkForNull(chorusParams.previousInputsPtr);
			SAVE_EFFECT(chorus)

		}else if(effectId == 5){
			LOAD_EFFECT(echo)
			echoParams.previousOutputsLen = ceil(echoParams.delay * sampleRate);
			echoParams.previousOutputsPtr = (float*) calloc(echoParams.previousOutputsLen, sizeof(float));
			checkForNull(echoParams.previousOutputsPtr);
			SAVE_EFFECT(echo)

		}else if(effectId == 6){
			LOAD_EFFECT(lowpass)
			lowpassParams.filterConst = dt / (dt + 1 / (TWO_PI * lowpassParams.filterConst));
			lowpassParams.lastOutputPtr = (float*) calloc(1, sizeof(float));
			checkForNull(lowpassParams.lastOutputPtr);
			SAVE_EFFECT(lowpass)

		}else if(effectId == 7){
			LOAD_EFFECT(highpass)
			highpassParams.filterConst = dt / (dt + 1 / (TWO_PI * highpassParams.filterConst));
			highpassParams.lastSamplesPtr = (float*) calloc(2, sizeof(float));
			checkForNull(highpassParams.lastSamplesPtr);
			SAVE_EFFECT(highpass)

		}else if(effectId == 8){
			LOAD_EFFECT(tremolo)
			tremoloParams.frequency = TWO_PI * tremoloParams.frequency / sampleRate;
			SAVE_EFFECT(tremolo)

		}else if(effectId == 9){
			LOAD_EFFECT(rotary)
			rotaryParams.frequency = TWO_PI * rotaryParams.frequency / sampleRate;
			rotaryParams.vibratoStrength = rotaryParams.vibratoStrength / rotaryParams.frequency;
			rotaryParams.previousInputsLen = ceil(2 * rotaryParams.vibratoStrength) + 3;
			rotaryParams.previousInputsPtr = (float*) calloc(rotaryParams.previousInputsLen, sizeof(float));
			checkForNull(rotaryParams.previousInputsPtr);
			SAVE_EFFECT(rotary)

		}else if(effectId == 10){
			LOAD_EFFECT(reverb)
			reverbParams.filtersParams[0].attenuation = getReverbAttenuation(9601, reverbParams.reverbTime);
			reverbParams.filtersParams[0].previousOutputsLen = 9601;
			reverbParams.filtersParams[1].attenuation = getReverbAttenuation(9999, reverbParams.reverbTime);
			reverbParams.filtersParams[1].previousOutputsLen = 9999;
			reverbParams.filtersParams[2].attenuation = getReverbAttenuation(10799, reverbParams.reverbTime);
			reverbParams.filtersParams[2].previousOutputsLen = 10799;
			reverbParams.filtersParams[3].attenuation = getReverbAttenuation(11599, reverbParams.reverbTime);
			reverbParams.filtersParams[3].previousOutputsLen = 11599;
			reverbParams.filtersParams[4].attenuation = getReverbAttenuation(11599, reverbParams.reverbTime);
			reverbParams.filtersParams[4].previousOutputsLen = 7001;
			reverbParams.filtersParams[5].attenuation = getReverbAttenuation(11599, reverbParams.reverbTime);
			reverbParams.filtersParams[5].previousOutputsLen = 2333;
			for(int i = 0; i < 6; i++){
				reverbParams.filtersParams[i].previousOutputsPtr = (float*) calloc(reverbParams.filtersParams[i].previousOutputsLen, sizeof(float));
				checkForNull(reverbParams.filtersParams[i].previousOutputsPtr);
			}
			SAVE_EFFECT(reverb)

		}else if(effectId == 11){
			LOAD_EFFECT(gain)

		}else if(effectId == 12){
			LOAD_EFFECT(noiseSuppr)
			noiseSupprParams.counter = (int*) calloc(1, sizeof(int));
			checkForNull(noiseSupprParams.counter);
			SAVE_EFFECT(noiseSuppr)

		}else if(effectId == 13){
			LOAD_EFFECT(fuzz)

		}else{
			crash(1);
		}
		i += paramsLen;
	}
}

void unloadEffects(){
	int i = 0;
	int effectId;
	int paramsLen;
	int channel = 1;

	while(1){
		effectId = *(effectsData + i);
		i++;
		if(effectId == 0xff){
			if(channel == 1){
				channel = 2;
				continue;
			}else{
				break;
			}
		}

		if(effectId == 0){
			paramsLen = 0;

		}else if(effectId == 1){
			LOAD_EFFECT(dist)

		}else if(effectId == 2){
			LOAD_EFFECT(softdist)

		}else if(effectId == 3){
			LOAD_EFFECT(vibrato)
			free(vibratoParams.previousInputsPtr);

		}else if(effectId == 4){
			LOAD_EFFECT(chorus)
			free(chorusParams.previousInputsPtr);

		}else if(effectId == 5){
			LOAD_EFFECT(echo)
			free(echoParams.previousOutputsPtr);

		}else if(effectId == 6){
			LOAD_EFFECT(lowpass)
			free(lowpassParams.lastOutputPtr);

		}else if(effectId == 7){
			LOAD_EFFECT(highpass)
			free(highpassParams.lastSamplesPtr);

		}else if(effectId == 8){
			LOAD_EFFECT(tremolo)

		}else if(effectId == 9){
			LOAD_EFFECT(rotary)
			free(rotaryParams.previousInputsPtr);

		}else if(effectId == 10){
			LOAD_EFFECT(reverb)
			for(int i = 0; i < 6; i++){
				free(reverbParams.filtersParams[i].previousOutputsPtr);
			}

		}else if(effectId == 11){
			LOAD_EFFECT(gain)

		}else if(effectId == 12){
			LOAD_EFFECT(noiseSuppr)
			free(noiseSupprParams.counter);

		}else if(effectId == 13){
			LOAD_EFFECT(fuzz)

		}else{
			crash(1);
		}
		i += paramsLen;
	}
}

void HAL_I2S_RxHalfCpltCallback(I2S_HandleTypeDef * hi2s1){
	  for(int i = 0; i < 32; i++){
		  process[i] = inData[i];
	  }
	inReady = 1;
}
void HAL_I2S_TxHalfCpltCallback(I2S_HandleTypeDef * hi2s2){
	  for(int i = 0; i < 32; i++){
		  outData[i] = output[i];
	  }
	outReady = 1;
}
void HAL_I2S_RxCpltCallback(I2S_HandleTypeDef * hi2s1){
	  for(int i = 32; i < 64; i++){
		  process[i-32] = inData[i];
	  }
	inReady = 1;
}
void HAL_I2S_TxCpltCallback(I2S_HandleTypeDef * hi2s2){
	  for(int i = 32; i < 64; i++){
		  outData[i] = output[i-32];
	  }
	outReady = 1;
}

void usbReceiveHandle(){
	if(usbBytesReady >= 256){
		if(usbBytesReady > 256){
			crash(2);
		}
		usbBytesReady = 0;

		if(usbDataBuffer[0] == 0xFE){
			for(int i = 0; i < usbBufferLen; i += 64){
				CDC_Transmit_HS(effectsRawData + i, 64);
				HAL_Delay(5);
			}
		}else{
			unloadEffects();
			memcpy(effectsRawData, usbDataBuffer, usbBufferLen);
			for(int i = 0; i < usbBufferLen; i += 32){
				HAL_I2C_Mem_Write(&hi2c1, 0xA0, i, 2, effectsRawData + i, 32, HAL_MAX_DELAY);
				HAL_Delay(5);
			}
			loadEffects();
		}
	}
}

float loadSample(int i){
	int32_t temp = process[i] << 8;
	float d = (float) temp;
	d = d / 2147483392;
	return d;
}

void saveSample(float d, int i){
	d = d * 2147483392;
	int32_t temp = (int)d;
	temp = temp >> 8;
	output[i] = temp;
}

float dist(float d, struct distConfig params){
	d = d * params.gain;
	if(d > 1){
		d = 1;
	}else if(d < -1){
		d = -1;
	}
	return d;
}

float softdist(float d, struct softdistConfig params){
	d = d * params.gain;
	return atanf(d);
}

float getPreviousSample(float prevSamps[], int prevSampsLen, int currInp, int offset){
	offset = -offset;
	int index;
	if(offset <= currInp){
		index = currInp - offset;
	}else{
		index = prevSampsLen + currInp - offset;
	}

	return prevSamps[index];
}

float interpolate(float prevSamps[], int prevSampsLen, int currInp, float offset){
	int i = floor(offset);
	float r0, r1, r2, r3;
	r0 = offset - 2 - i;
	r1 = r0 + 1;
	r2 = r0 + 2;
	r3 = r0 + 3;

	float i0, i1, i2, i3;

	i0 = getPreviousSample(prevSamps, prevSampsLen, currInp, i);
	i1 = getPreviousSample(prevSamps, prevSampsLen, currInp, i - 1);
	i2 = getPreviousSample(prevSamps, prevSampsLen, currInp, i - 2);
	i3 = getPreviousSample(prevSamps, prevSampsLen, currInp, i - 3);

	return  r1 * r2 * (r3 * i0 - r0 * i3) / 6 + r0 * r3 * (r1 * i2 - r2 * i1) / 2;
}

float vibrato(float d, struct vibratoConfig params){
	int currInp = generalInputN % params.previousInputsLen;
	params.previousInputsPtr[currInp] = d;
	return interpolate(params.previousInputsPtr, params.previousInputsLen, currInp, - params.strength * (sinf(params.frequency * generalInputN) + 1));
}

float chorus(float d, struct chorusConfig params){
	int currInp = generalInputN % params.previousInputsLen;
	params.previousInputsPtr[currInp] = d;
	float vibr = interpolate(params.previousInputsPtr, params.previousInputsLen, currInp, - params.strength * (sinf(params.frequency * generalInputN) + 1));
	float clean = getPreviousSample(params.previousInputsPtr, params.previousInputsLen, currInp, - ((params.previousInputsLen - 3) / 3));
	return (vibr + clean) / 2;
}

float echo(float d, struct echoConfig params){
	int currSampN = generalInputN % params.previousOutputsLen;
	float prevSamp = getPreviousSample(params.previousOutputsPtr, params.previousOutputsLen, currSampN, -params.previousOutputsLen + 1);
	float currSamp = d + params.attenuation * prevSamp;
	params.previousOutputsPtr[currSampN] = currSamp;
	return currSamp;
}

float lowpass(float d, struct lowpassConfig params){
	d = *params.lastOutputPtr + params.filterConst * (d - (*params.lastOutputPtr));
	*params.lastOutputPtr = d;
	return d;
}

float highpass(float d, struct highpassConfig params){
	float temp = params.filterConst * (params.lastSamplesPtr[0] + d - params.lastSamplesPtr[1]);
	params.lastSamplesPtr[1] = d;
	params.lastSamplesPtr[0] = temp;
	return temp;
}

float tremolo(float d, struct tremoloConfig params){
	return d * (1 + params.strength * sinf(params.frequency * generalInputN)) / (1 + params.strength);
}

float rotary(float d, struct rotaryConfig params){
	int currInp = generalInputN % params.previousInputsLen;
	params.previousInputsPtr[currInp] = d;
	float effectSin = sinf(params.frequency * generalInputN);
	float vibr = interpolate(params.previousInputsPtr, params.previousInputsLen, currInp, - params.vibratoStrength * (effectSin + 1));
	return vibr * (1 + params.tremoloStrength * effectSin) / (1 + params.tremoloStrength);
}

float allPass(float d, struct echoConfig params){
	int currSampN = generalInputN % params.previousOutputsLen;
	float prevSamp = getPreviousSample(params.previousOutputsPtr, params.previousOutputsLen, currSampN, -params.previousOutputsLen + 1);
	float currSamp = d + params.attenuation * prevSamp;
	params.previousOutputsPtr[currSampN] = currSamp;
	return currSamp * (1 - params.attenuation * params.attenuation) - params.attenuation * d;
}

float reverb(float d, struct reverbConfig params){
	float comb1 = echo(d, params.filtersParams[0]);
	float comb2 = echo(d, params.filtersParams[1]);
	float comb3 = echo(d, params.filtersParams[2]);
	float comb4 = echo(d, params.filtersParams[3]);
	float sum = comb1 + comb2 + comb3 + comb4;

	return allPass(allPass(sum, params.filtersParams[4]), params.filtersParams[5]) * params.dry_wet + d * (1 - params.dry_wet);
}

float gain(float d, struct gainConfig params){
	return d * params.gain;
}

float noiseSuppr(float d, struct noiseSupprConfig params){
	if(fabs(d) > params.threshold){
		*params.counter = 4000;
	}
	if(*params.counter > 0){
		*params.counter = *params.counter - 1;
		return d;
	}else{
		return 0;
	}
}

float fuzz(float d, struct fuzzConfig params){
	if(d == 0){
		return 0;
	}
	if(d > 0){
		return 1;
	}else{
		return -1;
	}
}

void applyEffects(float ch1, float ch2, int codecBufferInd){
	int i = 0;
	int effectId;
	int channel = 1;
	float d = ch1;
	int paramsLen;

	while(1){
		effectId = *(effectsData + i);
		i++;
		if(effectId == 0xff){
			if(channel == 1){
				ch1 = d;
				d = ch2;
				channel = 2;
				continue;
			}else{
				ch2 = d;
				break;
			}
		}

		if(effectId == 0){
			paramsLen = 0;
			d = 0;

		}else if(effectId == 1){
			EXECUTE_EFFECT(dist)

		}else if(effectId == 2){
			EXECUTE_EFFECT(softdist)

		}else if(effectId == 3){
			EXECUTE_EFFECT(vibrato)

		}else if(effectId == 4){
			EXECUTE_EFFECT(chorus)

		}else if(effectId == 5){
			EXECUTE_EFFECT(echo)

		}else if(effectId == 6){
			EXECUTE_EFFECT(lowpass)

		}else if(effectId == 7){
			EXECUTE_EFFECT(highpass)

		}else if(effectId == 8){
			EXECUTE_EFFECT(tremolo)

		}else if(effectId == 9){
			EXECUTE_EFFECT(rotary)

		}else if(effectId == 10){
			EXECUTE_EFFECT(reverb)

		}else if(effectId == 11){
			EXECUTE_EFFECT(gain)

		}else if(effectId == 12){
			EXECUTE_EFFECT(noiseSuppr)

		}else if(effectId == 13){
			EXECUTE_EFFECT(fuzz)

		}else{
			crash(1);
		}
		i += paramsLen;
	}

	if(ch1 > 1){
		ch1 = 1;
	}else if(ch1 < -1){
		ch1 = -1;
	}
	if(ch2 > 1){
		ch2 = 1;
	}else if(ch2 < -1){
		ch2 = -1;
	}

	saveSample(ch1, codecBufferInd);
	saveSample(ch2, codecBufferInd + 1);
}

void mainLoop(){
	usbReceiveHandle();
	//sound processing
	if(inReady && execute){ //executes when new data is fully loaded into half of the input buffer
		for(int i = 0; i<32; i += 2){
			float ch1 = loadSample(i);
			float ch2 = loadSample(i + 1);

			applyEffects(ch1, ch2, i);
			generalInputN++;
		}
		inReady = 0;
	}
}

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
  MX_DMA_Init();
  MX_I2S1_Init();
  MX_I2S2_Init();
  MX_I2C2_Init();
  MX_I2C1_Init();
  MX_USB_DEVICE_Init();
  /* USER CODE BEGIN 2 */
	MX_GPIO_Init();
	SSD1306_Init (); // initialize the display

	SSD1306_GotoXY (0,20); // goto 10, 10
	SSD1306_Puts ("PLAYING :)", &Font_11x18, 1); //
	SSD1306_UpdateScreen(); // update screen
	MX_I2S1_Init();
	MX_I2S2_Init();
	/* USER CODE BEGIN 2 */
	HAL_GPIO_WritePin(GPIOE, GPIO_PIN_2, 1);

	HAL_I2S_Receive_DMA(&hi2s1, &inData, 64);
	HAL_I2S_Transmit_DMA(&hi2s2, &outData, 64);

	for(int i = 0; i < usbBufferLen; i += 32){
		HAL_I2C_Mem_Read(&hi2c1, 0xA1, i, 2, effectsRawData + i, 32, HAL_MAX_DELAY);
		HAL_Delay(5);
	}

	loadEffects();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
	while (1)
	{
		mainLoop();
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

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI48|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSI48State = RCC_HSI48_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 44;
  RCC_OscInitStruct.PLL.PLLP = 1;
  RCC_OscInitStruct.PLL.PLLQ = 3;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_3;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
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
  hi2c1.Init.Timing = 0x60404E72;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x00D049FB;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief I2S1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2S1_Init(void)
{

  /* USER CODE BEGIN I2S1_Init 0 */

  /* USER CODE END I2S1_Init 0 */

  /* USER CODE BEGIN I2S1_Init 1 */

  /* USER CODE END I2S1_Init 1 */
  hi2s1.Instance = SPI1;
  hi2s1.Init.Mode = I2S_MODE_MASTER_RX;
  hi2s1.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s1.Init.DataFormat = I2S_DATAFORMAT_24B;
  hi2s1.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
  hi2s1.Init.AudioFreq = I2S_AUDIOFREQ_48K;
  hi2s1.Init.CPOL = I2S_CPOL_LOW;
  hi2s1.Init.FirstBit = I2S_FIRSTBIT_MSB;
  hi2s1.Init.WSInversion = I2S_WS_INVERSION_DISABLE;
  hi2s1.Init.Data24BitAlignment = I2S_DATA_24BIT_ALIGNMENT_RIGHT;
  hi2s1.Init.MasterKeepIOState = I2S_MASTER_KEEP_IO_STATE_DISABLE;
  if (HAL_I2S_Init(&hi2s1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S1_Init 2 */

  /* USER CODE END I2S1_Init 2 */

}

/**
  * @brief I2S2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2S2_Init(void)
{

  /* USER CODE BEGIN I2S2_Init 0 */

  /* USER CODE END I2S2_Init 0 */

  /* USER CODE BEGIN I2S2_Init 1 */

  /* USER CODE END I2S2_Init 1 */
  hi2s2.Instance = SPI2;
  hi2s2.Init.Mode = I2S_MODE_MASTER_TX;
  hi2s2.Init.Standard = I2S_STANDARD_PHILIPS;
  hi2s2.Init.DataFormat = I2S_DATAFORMAT_24B;
  hi2s2.Init.MCLKOutput = I2S_MCLKOUTPUT_ENABLE;
  hi2s2.Init.AudioFreq = I2S_AUDIOFREQ_48K;
  hi2s2.Init.CPOL = I2S_CPOL_LOW;
  hi2s2.Init.FirstBit = I2S_FIRSTBIT_MSB;
  hi2s2.Init.WSInversion = I2S_WS_INVERSION_DISABLE;
  hi2s2.Init.Data24BitAlignment = I2S_DATA_24BIT_ALIGNMENT_RIGHT;
  hi2s2.Init.MasterKeepIOState = I2S_MASTER_KEEP_IO_STATE_DISABLE;
  if (HAL_I2S_Init(&hi2s2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2S2_Init 2 */

  /* USER CODE END I2S2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA1_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA1_Stream0_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream0_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream0_IRQn);
  /* DMA1_Stream1_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA1_Stream1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA1_Stream1_IRQn);

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
  __HAL_RCC_GPIOE_CLK_ENABLE();
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOE, LED1_Pin|GPIO_PIN_3|GPIO_PIN_4, GPIO_PIN_RESET);

  /*Configure GPIO pins : LED1_Pin PE3 PE4 */
  GPIO_InitStruct.Pin = LED1_Pin|GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

  /*Configure GPIO pins : PF2 PF3 PF4 */
  GPIO_InitStruct.Pin = GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOF, &GPIO_InitStruct);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
int _write(int file, char *ptr, int len)

{   int DataIdx;

    for(DataIdx = 0; DataIdx < len; DataIdx++)

    { ITM_SendChar( *ptr++); }

     return len;

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

#ifdef  USE_FULL_ASSERT
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
