#pragma once



const int Width = 128; // ?????????? ?????????? ? ??????? ??????
const int Height = 64; // ?????????? ?????????? ? ??????? ??????
char Buffer[Width * Height / 8]; // ????? ?????? ??????

void SetAltFunc(GPIO_TypeDef* Port, int Channel, int AF)
{
  Port->MODER &= ~(3 << (2 * Channel)); // ????? ??????
  Port->MODER |= 2 << (2 * Channel); // ????????? ????. ??????
  if (Channel < 8) // ????? ???????? ??????? ?? ?????? ????????
  {
      Port->AFR[0] &= ~(15 << 4 * Channel); // ????? ????. ???????
      Port->AFR[0] |= AF << (4 * Channel); // ????????? ????. ???????
  }
  else
  {
      Port->AFR[1] &= ~(15 << 4 * (Channel - 8)); // ????? ????. ???????
      Port->AFR[1] |= AF << (4 * (Channel - 8)); // ????????? ????. ???????
  }
}

void SetADC() {
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN; // ??? ????????????
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN; // ???? A ????????????
  GPIOC->MODER &= ~(GPIO_MODER_MODER3 | GPIO_MODER_MODER2); // ????? ?????? ??? PC3 ? PC2
  GPIOC->MODER |= GPIO_MODER_MODER3_0 | GPIO_MODER_MODER3_1; // ?????????? ???? PC3
  GPIOC->MODER |= GPIO_MODER_MODER2_0 | GPIO_MODER_MODER2_1; // ?????????? ???? PC2
  ADC1->CR2 = ADC_CR2_ADON; // ??? ???????

}

enum PORT { A, B, C, D, E, F, G, H, I }; // ???????????? ????????? ??????
void SetEXTI(PORT Port, int Channel, bool Rise, bool Fall)
{
  SYSCFG->EXTICR[Channel / 4] &= ~(15 << (4 * (Channel % 4))); // ???????? ????
  SYSCFG->EXTICR[Channel / 4] |= Port << (4 * (Channel % 4)); // ??????? ????

  EXTI->IMR |= 1 << Channel; // ?????????? ???????

  if (Rise) EXTI->RTSR |= 1 << Channel; // ?????? ????????? ??????????
  else EXTI->RTSR &= ~(1 << Channel); // ?? ?????? ????????? ??????????

  if (Fall) EXTI->FTSR |= 1 << Channel; // ?????? ??????? ??????????
  else EXTI->FTSR &= ~(1 << Channel); // ?? ?????? ??????? ??????????
}

void ActivateUSARTs() {
  RCC->APB2ENR |= RCC_APB2ENR_USART6EN; // UART 6 ???????????? (APB2=....?)

  // ??? ????? LabBoard 1.1
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN; // ???? A ????????????
  SetAltFunc(GPIOC, 6, 8); // ????????? ????. ?????? PC6 ??? TX() (??. ???. 2)
  SetAltFunc(GPIOC, 7, 8); // ????????? ????. ?????? PC7 ??? RX() (??. ???. 2)
  //--

  USART6->BRR = (84000000) / 9600; // ???????? ?????????? ?? 11520 //???????????????

  USART6->CR1 = USART_CR1_RE | USART_CR1_TE; // ????? ? ???????? ???????
  USART6->CR1 |= USART_CR1_RXNEIE | USART_CR1_UE; // ?????????? ?? ????? ? ?????? ??????????

  NVIC_SetPriority(USART6_IRQn, 0); // ?????? ????????? ??????????
  NVIC_EnableIRQ(USART6_IRQn); // ?????????? ???????

}

//inline namespace. Global vars for RX buffer
namespace {
  const int DATA_Size = 128; // ?????? ???????????? ??????
  char DATA_Buffer[DATA_Size]; // ??????????? ?????
  int DATA_Head = 0; // ??????? ??? ????????? ??????
  int DATA_Tail = 0; // ??????? ??? ?????????? ??????
}

extern "C" void USART6_IRQHandler() // ??????? ????????? ??????????
{
  int test = USART6->SR; // ?????? SR ????? ?????????? ??????????
  DATA_Buffer[DATA_Head++] = USART6->DR; // ?????? ?????? ? ?????? ????. ???????
  if (DATA_Head >= DATA_Size) DATA_Head = 0; // ???????? ? ?????? ???? ????????? ??????
}

int UART6_Recv(char* Data, int Size, bool WaitAll = false) // ??????? ?????? ????
{
  int size; // ?????? ???????? ??????
  for (size = 0; size < Size; size++) // ???? ?????? ?????? ? ?????? ??????????? ???????
  {
      if (WaitAll) while (DATA_Tail == DATA_Head) {} // ????? ??????? ??????
      else if (DATA_Tail == DATA_Head) break; // ?????? ?????? ???, ??????? ?? ?????
      Data[size] = DATA_Buffer[DATA_Tail++]; // ?????? ???? ? ?????? ????. ???????
      if (DATA_Tail >= DATA_Size) DATA_Tail = 0; // ?????????? ???????, ???? ???????
  }
  return size; // ??????? ?????? ?????????? ??????
}

int UART6_GetString(char* Data, int Size) // ??????? ?????? ??????
{
  int size;
  int tmpTail = DATA_Tail;

  // ?????? ???????? ??????
  int del = 0;
  for (size = 0; size < (Size - 1); size++) // ???? ?????? ?????? ? ?????? ??????????? ???????
  {
      while (DATA_Tail == DATA_Head) {

          del++;
          if (del == 20000000) {
              DATA_Tail = tmpTail;
              return 0;
          }
      }// ????? ??????? ??????
      Data[size] = DATA_Buffer[DATA_Tail++]; // ?????? ???? ? ?????? ????. ???????
      if (DATA_Tail >= DATA_Size) DATA_Tail = 0; // ?????????? ???????, ???? ???????
      if (Data[size] == '\n') { size++; break; } // ?????????? ????? ??????
  }
  Data[size] = '\0'; // ?????????? ????? ??????
  return size; // ??????? ?????? ?????????? ??????
}

void UART6_Send(char* Data, int Size) // ??????? ???????? ????
{
  while (Size--) // ???? ???????? ??????
  {                      // ???!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      while (!(USART6->SR & USART_SR_TXE)) {} // ????? ??????????? ??????????
      USART6->DR = *Data++; // ???????? ?????? ? ?????? ????. ???????
  }                    // ??????!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  while (!(USART6->SR & USART_SR_TC)) {} // ????? ?????????? ????????
}


void setI2C_2() {
  RCC->APB1ENR |= RCC_APB1ENR_I2C2EN; // I2C 2 ????????????
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN; // ???? H ????????????
  GPIOH->OTYPER |= GPIO_OTYPER_OT_4 | GPIO_OTYPER_OT_5; // ???????? ???? ??? PH4 ? PH5
  GPIOH->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR4_0 | GPIO_OSPEEDER_OSPEEDR4_1; // ???. ???????? PH4 
  GPIOH->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR5_0 | GPIO_OSPEEDER_OSPEEDR5_1; // ???. ???????? PH5
  GPIOH->PUPDR |= GPIO_PUPDR_PUPDR4_0 | GPIO_PUPDR_PUPDR5_0; // ???????? 3.3V ??? PH4 ? PH5
  SetAltFunc(GPIOH, 4, 4); // ????????? ????. ?????? AF4 ??? SCL(PH4) (??. ???. 2)
  SetAltFunc(GPIOH, 5, 4); // ????????? ????. ?????? AF4 ??? SDA(PH5) (??. ???. 2)
  //****
  I2C2->CR2 = (I2C_CR2_FREQ & 0x2A);
  I2C2->CCR = I2C_CCR_FS | (I2C_CCR_CCR & 0x006C);
  I2C2->TRISE = (I2C_TRISE_TRISE & 0x14);
  //**
  I2C2->CR1 = I2C_CR1_PE;
  while (I2C3->SR2 & I2C_SR2_BUSY) {}

}

void setI2C_1() {
  RCC->APB1ENR |= RCC_APB1ENR_I2C1EN; // I2C 1 ????????????
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN; // ???? H ????????????
  GPIOH->OTYPER |= GPIO_OTYPER_OT_7 | GPIO_OTYPER_OT_8; // ???????? ???? ??? PH7 ? PH8
  GPIOH->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR7_0 | GPIO_OSPEEDER_OSPEEDR7_1; // ???. ???????? PH7 
  GPIOH->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR8_0 | GPIO_OSPEEDER_OSPEEDR8_1; // ???. ???????? PH8
  GPIOH->PUPDR |= GPIO_PUPDR_PUPDR7_0 | GPIO_PUPDR_PUPDR8_0; // ???????? 3.3V ??? PH7 ? PH8
  SetAltFunc(GPIOH, 7, 4); // ????????? ????. ?????? AF4 ??? SCL(PH7) (??. ???. 2)
  SetAltFunc(GPIOH, 8, 4); // ????????? ????. ?????? AF4 ??? SDA(PH8) (??. ???. 2)

}


void I2C1_Write(int Address, char Reg, char* Data, int Size) // ??????? ? ??????
{
  I2C1->CR1 |= I2C_CR1_START; // ???????? ????? ????? ??? ??????
  while (!(I2C1->SR1 & I2C_SR1_SB)) {} // ???? ??????? ?????
  I2C1->DR = (Address << 1) & ~I2C_OAR1_ADD0; // ???? ????? ??? ???????? ??????
  while (!(I2C1->SR1 & I2C_SR1_ADDR)) {} // ???? ???????? ????? ? ???????????
  I2C1->SR2; // ?????? SR2 ??? ??? ????????
  while (!(I2C1->SR1 & I2C_SR1_TXE)) {} // ???? ?????????? ???????? ????
  I2C1->DR = Reg; // ???????? ???? ????????
  while (Size--) // ???? ???????? ????
  {
      while (!(I2C1->SR1 & I2C_SR1_TXE)) {} // ???? ?????????? ???????? ????
      I2C1->DR = *Data++; // ???????? ???? ??????
  }
  while (!(I2C1->SR1 & I2C_SR1_BTF)) {} // ???? ????????? ???????? ??????
  I2C1->CR1 |= I2C_CR1_STOP; // ??????????? ????? ?????
  while (I2C1->CR1 & I2C_CR1_STOP) {} // ???? ???????????? ?????
}


void I2C_Write(int Address, char Reg, char* Data, int Size) // ??????? ? ??????
{
  I2C2->CR1 |= I2C_CR1_START; // ???????? ????? ????? ??? ??????
  while (!(I2C2->SR1 & I2C_SR1_SB)) {} // ???? ??????? ?????
  I2C2->DR = (Address << 1) & ~I2C_OAR1_ADD0; // ???? ????? ??? ???????? ??????
  while (!(I2C2->SR1 & I2C_SR1_ADDR)) {} // ???? ???????? ????? ? ???????????
  I2C2->SR2; // ?????? SR2 ??? ??? ????????
  while (!(I2C2->SR1 & I2C_SR1_TXE)) {} // ???? ?????????? ???????? ????
  I2C2->DR = Reg; // ???????? ???? ????????
  while (Size--) // ???? ???????? ????
  {
      while (!(I2C2->SR1 & I2C_SR1_TXE)) {} // ???? ?????????? ???????? ????
      I2C2->DR = *Data++; // ???????? ???? ??????
  }
  while (!(I2C2->SR1 & I2C_SR1_BTF)) {} // ???? ????????? ???????? ??????
  I2C2->CR1 |= I2C_CR1_STOP; // ??????????? ????? ?????
  while (I2C2->CR1 & I2C_CR1_STOP) {} // ???? ???????????? ?????
}


const int Addr = 0x3C; // ?????????? ?????????? ? ??????? ??????????
const int AddrAS5600 = 0x36; // ?????????? ?????????? ? ??????? ??????????
void as5600COMMAND(char Value) { I2C_Write(AddrAS5600, 0x00, &Value, 1); }
void Command(char Value) { I2C_Write(Addr, 0x00, &Value, 1); } // ?????? ? 0x00 ???????

void turnOnSSD1309() {

  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN; // ???? E ????????????
  GPIOE->MODER &= ~GPIO_MODER_MODER7; // ????? ?????? ??? PE7
  GPIOE->MODER |= GPIO_MODER_MODER7_0; // ????????? ?????? ?? ????? PE7
  GPIOE->BSRRL = 1 << 7; // ?????????? ???????? HIGH (3.3V) ??? PE7

  for (int a = 0; a < 1000000; a++) {};
  Command(0xAE); // Display off
  Command(0x20); // Set Memory Addressing Mode 
  Command(0x10); // Page Addressing Mode (RESET)
  Command(0xB0); // Set Page Start Address for Page Addressing Mode,0-7
  Command(0xC8); // Set COM Output Scan Direction
  Command(0x00); //---set low column address
  Command(0x10); //---set high column address
  Command(0x40); //--set start line address
  Command(0x81); //--set contrast control register
  Command(0xFF); // Orientation
  Command(0xA1); //--set segment re-map (0 to 127)
  Command(0xA6); //--set normal display
  Command(0xA8); //--set multiplex ratio (1 to 64)
  Command(0x3F); //
  Command(0xA4); // Output follows RAM content
  Command(0xD3); //-set display offset
  Command(0x00); //-not offset
  Command(0xD5); //--set display clock divide ratio/oscillator frequency
  Command(0xF0); //--set divide ratio
  Command(0xD9); //--set pre-charge period
  Command(0x22); //
  Command(0xDA); //--set com pins hardware configuration
  Command(0x12); //
  Command(0xDB); //--set vcomh
  Command(0x20); // 0.77 * Vcc
  Command(0x8D); //--set DC-DC enable
  Command(0x14); //
  Command(0xAF); //--turn on SSD1309 panel
}

// read from ADC 
int AnalogRead(int N) // ??????? ????????? ????? ?????? ??? ??????????????
{
  ADC1->SQR3 = N; // ?????? ?????????? ?? ????????? ?????
  for (int a = 0; a < 100; a++) { asm("NOP"); } // ??????? ?????? 100 ??????
  ADC1->CR2 |= ADC_CR2_SWSTART; // ?????? ??????????????
  while (!(ADC1->SR & ADC_SR_EOC)) { asm("NOP"); } // ????? ????????? ???? ????? ????????
  return ADC1->DR; // ??????? ????????? ??????????????
}

