#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <stdint.h>
#include <array>
#define SLAVE

class GameMaster;

void SetAltFunc(GPIO_TypeDef* Port, int Channel, int AF)
{
  Port->MODER &= ~(3 << (2 * Channel)); // Сброс режима
  Port->MODER |= 2 << (2 * Channel); // Установка альт. Режима
  if (Channel < 8) // Выбор регистра зависит от номера контакта
  {
      Port->AFR[0] &= ~(15 << 4 * Channel); // Сброс альт. функции
      Port->AFR[0] |= AF << (4 * Channel); // Установка альт. функции
  }
  else
  {
      Port->AFR[1] &= ~(15 << 4 * (Channel - 8)); // Сброс альт. функции
      Port->AFR[1] |= AF << (4 * (Channel - 8)); // Установка альт. функции
  }
}

void SetADC() {
  RCC->APB2ENR |= RCC_APB2ENR_ADC1EN; // АЦП задействован
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN; // Порт A задействован
  GPIOC->MODER &= ~(GPIO_MODER_MODER3 | GPIO_MODER_MODER2); // Сброс режима для PC3 и PC2
  GPIOC->MODER |= GPIO_MODER_MODER3_0 | GPIO_MODER_MODER3_1; // Аналоговый вход PC3
  GPIOC->MODER |= GPIO_MODER_MODER2_0 | GPIO_MODER_MODER2_1; // Аналоговый вход PC2
  ADC1->CR2 = ADC_CR2_ADON; // АЦП активен

}

enum PORT { A, B, C, D, E, F, G, H, I }; // Перечисление доступных портов
void SetEXTI(PORT Port, int Channel, bool Rise, bool Fall)
{
  SYSCFG->EXTICR[Channel / 4] &= ~(15 << (4 * (Channel % 4))); // Сбросить порт
  SYSCFG->EXTICR[Channel / 4] |= Port << (4 * (Channel % 4)); // Выбрать порт

  EXTI->IMR |= 1 << Channel; // Прерывание выбрано

  if (Rise) EXTI->RTSR |= 1 << Channel; // Ловить повышение напряжения
  else EXTI->RTSR &= ~(1 << Channel); // Не ловить повышение напряжения

  if (Fall) EXTI->FTSR |= 1 << Channel; // Ловить падение напряжения
  else EXTI->FTSR &= ~(1 << Channel); // Не ловить падение напряжения
}

void ActivateUSARTs() {
  RCC->APB2ENR |= RCC_APB2ENR_USART6EN; // UART 6 задействован (APB2=....?)

  // Для платы LabBoard 1.1
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN; // Порт A задействован
  SetAltFunc(GPIOC, 6, 8); // Установка альт. режима PC6 для TX() (см. лаб. 2)
  SetAltFunc(GPIOC, 7, 8); // Установка альт. режима PC7 для RX() (см. лаб. 2)
  //--

  USART6->BRR = (84000000) / 9600; // Делитель установлен на 11520 //???????????????

  USART6->CR1 = USART_CR1_RE | USART_CR1_TE; // Приём и передача активны
  USART6->CR1 |= USART_CR1_RXNEIE | USART_CR1_UE; // Прерывание на приём и запуск устройства

  NVIC_SetPriority(USART6_IRQn, 0); // Высший приоритет прерывания
  NVIC_EnableIRQ(USART6_IRQn); // Прерывание активно

}

//inline namespace. Global vars for RX buffer
namespace {
  const int DATA_Size = 128; // Размер принимающего буфера
  char DATA_Buffer[DATA_Size]; // Принимающий буфер
  int DATA_Head = 0; // Позиция для следующий записи
  int DATA_Tail = 0; // Позиция для следующего чтения
}

extern "C" void USART6_IRQHandler() // Функция обработки прерывания
{
  int test = USART6->SR; // Читаем SR чтобы обработать прерывание
  DATA_Buffer[DATA_Head++] = USART6->DR; // Читаем данные и задаём след. позицию
  if (DATA_Head >= DATA_Size) DATA_Head = 0; // Начинаем с начала если превысили размер
}

int UART6_Recv(char* Data, int Size, bool WaitAll = false) // Функция приёма байт
{
  int size; // Размер принятых данных
  for (size = 0; size < Size; size++) // Цикл приёма данных с учётом допустимого размера
  {
      if (WaitAll) while (DATA_Tail == DATA_Head) {} // Ждать прихода данных
      else if (DATA_Tail == DATA_Head) break; // Данных больше нет, выходим из цикла
      Data[size] = DATA_Buffer[DATA_Tail++]; // Читаем байт и задаём след. позицию
      if (DATA_Tail >= DATA_Size) DATA_Tail = 0; // Превышение размера, идём сначала
  }
  return size; // Вернуть размер полученных данных
}

int UART6_GetString(char* Data, int Size) // Функция приёма строки
{
  int size;
  int tmpTail = DATA_Tail;

  // Размер принятых данных
  int del = 0;
  for (size = 0; size < (Size - 1); size++) // Цикл приёма данных с учётом допустимого размера
  {
      while (DATA_Tail == DATA_Head) {

          del++;
          if (del == 20000000) {
              DATA_Tail = tmpTail;
              return 0;
          }
      }// Ждать прихода данных
      Data[size] = DATA_Buffer[DATA_Tail++]; // Читаем байт и задаём след. позицию
      if (DATA_Tail >= DATA_Size) DATA_Tail = 0; // Превышение размера, идём сначала
      if (Data[size] == '\n') { size++; break; } // Обнаружить новую строку
  }
  Data[size] = '\0'; // Установить конец строки
  return size; // Вернуть размер полученных данных
}

void UART6_Send(char* Data, int Size) // Функция передачи байт
{
  while (Size--) // Цикл передачи данных
  {                      // ???!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
      while (!(USART6->SR & USART_SR_TXE)) {} // Ждать возможности передавать
      USART6->DR = *Data++; // Передать данные и задать след. позицию
  }                    // ??????!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
  while (!(USART6->SR & USART_SR_TC)) {} // Ждать завершения передачи
}


void setI2C_2() {
  RCC->APB1ENR |= RCC_APB1ENR_I2C2EN; // I2C 2 задействован
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN; // Порт H задействован
  GPIOH->OTYPER |= GPIO_OTYPER_OT_4 | GPIO_OTYPER_OT_5; // Открытый сток для PH4 и PH5
  GPIOH->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR4_0 | GPIO_OSPEEDER_OSPEEDR4_1; // Мах. скорость PH4 
  GPIOH->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR5_0 | GPIO_OSPEEDER_OSPEEDR5_1; // Мах. скорость PH5
  GPIOH->PUPDR |= GPIO_PUPDR_PUPDR4_0 | GPIO_PUPDR_PUPDR5_0; // Подтяжка 3.3V для PH4 и PH5
  SetAltFunc(GPIOH, 4, 4); // Установка альт. режима AF4 для SCL(PH4) (см. лаб. 2)
  SetAltFunc(GPIOH, 5, 4); // Установка альт. режима AF4 для SDA(PH5) (см. лаб. 2)
  //****
  I2C2->CR2 = (I2C_CR2_FREQ & 0x2A);
  I2C2->CCR = I2C_CCR_FS | (I2C_CCR_CCR & 0x006C);
  I2C2->TRISE = (I2C_TRISE_TRISE & 0x14);
  //**
  I2C2->CR1 = I2C_CR1_PE;
  while (I2C3->SR2 & I2C_SR2_BUSY) {}

}

void setI2C_1() {
  RCC->APB1ENR |= RCC_APB1ENR_I2C1EN; // I2C 1 задействован
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN; // Порт H задействован
  GPIOH->OTYPER |= GPIO_OTYPER_OT_7 | GPIO_OTYPER_OT_8; // Открытый сток для PH7 и PH8
  GPIOH->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR7_0 | GPIO_OSPEEDER_OSPEEDR7_1; // Мах. скорость PH7 
  GPIOH->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR8_0 | GPIO_OSPEEDER_OSPEEDR8_1; // Мах. скорость PH8
  GPIOH->PUPDR |= GPIO_PUPDR_PUPDR7_0 | GPIO_PUPDR_PUPDR8_0; // Подтяжка 3.3V для PH7 и PH8
  SetAltFunc(GPIOH, 7, 4); // Установка альт. режима AF4 для SCL(PH7) (см. лаб. 2)
  SetAltFunc(GPIOH, 8, 4); // Установка альт. режима AF4 для SDA(PH8) (см. лаб. 2)

}


void I2C1_Write(int Address, char Reg, char* Data, int Size) // регистр и данные
{
  I2C1->CR1 |= I2C_CR1_START; // Занимаем линию связи для данных
  while (!(I2C1->SR1 & I2C_SR1_SB)) {} // Ждём занятия линии
  I2C1->DR = (Address << 1) & ~I2C_OAR1_ADD0; // Шлём адрес для передачи данных
  while (!(I2C1->SR1 & I2C_SR1_ADDR)) {} // Ждём успешной связи с устройством
  I2C1->SR2; // Читаем SR2 для его отчистки
  while (!(I2C1->SR1 & I2C_SR1_TXE)) {} // Ждем готовности передать байт
  I2C1->DR = Reg; // Передаём байт регистра
  while (Size--) // Цикл передачи байт
  {
      while (!(I2C1->SR1 & I2C_SR1_TXE)) {} // Ждем готовности передать байт
      I2C1->DR = *Data++; // Передаём байт данных
  }
  while (!(I2C1->SR1 & I2C_SR1_BTF)) {} // Ждём окончания передачи данных
  I2C1->CR1 |= I2C_CR1_STOP; // Освобождаем линию связи
  while (I2C1->CR1 & I2C_CR1_STOP) {} // Ждём освобождения линии
}


void I2C_Write(int Address, char Reg, char* Data, int Size) // регистр и данные
{
  I2C2->CR1 |= I2C_CR1_START; // Занимаем линию связи для данных
  while (!(I2C2->SR1 & I2C_SR1_SB)) {} // Ждём занятия линии
  I2C2->DR = (Address << 1) & ~I2C_OAR1_ADD0; // Шлём адрес для передачи данных
  while (!(I2C2->SR1 & I2C_SR1_ADDR)) {} // Ждём успешной связи с устройством
  I2C2->SR2; // Читаем SR2 для его отчистки
  while (!(I2C2->SR1 & I2C_SR1_TXE)) {} // Ждем готовности передать байт
  I2C2->DR = Reg; // Передаём байт регистра
  while (Size--) // Цикл передачи байт
  {
      while (!(I2C2->SR1 & I2C_SR1_TXE)) {} // Ждем готовности передать байт
      I2C2->DR = *Data++; // Передаём байт данных
  }
  while (!(I2C2->SR1 & I2C_SR1_BTF)) {} // Ждём окончания передачи данных
  I2C2->CR1 |= I2C_CR1_STOP; // Освобождаем линию связи
  while (I2C2->CR1 & I2C_CR1_STOP) {} // Ждём освобождения линии
}


const int Addr = 0x3C; // Глобальная переменная с адресом устройства
const int AddrAS5600 = 0x36; // Глобальная переменная с адресом устройства
void as5600COMMAND(char Value) { I2C_Write(AddrAS5600, 0x00, &Value, 1); }
void Command(char Value) { I2C_Write(Addr, 0x00, &Value, 1); } // Запись в 0x00 регистр

void turnOnSSD1309() {

  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN; // Порт E задействован
  GPIOE->MODER &= ~GPIO_MODER_MODER7; // Сброс режима для PE7
  GPIOE->MODER |= GPIO_MODER_MODER7_0; // Установка режима на выход PE7
  GPIOE->BSRRL = 1 << 7; // Установить значение HIGH (3.3V) для PE7

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





const int Width = 128; // Глобальная переменная с шириной экрана
const int Height = 64; // Глобальная переменная с высотой экрана
char Buffer[Width * Height / 8]; // Буфер данных экрана
void Clear() // Функция очистки буфера экрана
{
  for (int a = 0; a < sizeof(Buffer); a++) Buffer[a] = 0;
}
void DrawPixel(int X, int Y) // Функция установки пикселя
{
  if (X >= Width || Y >= Height) return;
  Buffer[X + (Y / 8) * Width] |= 1 << (Y % 8);
}
void UpdateScreen() // Функция прорисовки буфера экрана на дисплей
{
  for (char a = 0; a < 8; a++)
  {
      Command(0xB0 + a);
      Command(0x00);
      Command(0x10);
      I2C_Write(Addr, 0x40, &Buffer[Width * a], Width);
  }
}

void PrintNum(int x0, int y0, int n) {


}

void DrawCirlce(int X0, int Y0, int R)
{
  int x = 0;
  int y = R;
  int delta = 1 - 2 * R;
  int error = 0;
  while (y >= 0)
  {
      DrawPixel(X0 + x, Y0 + y);
      DrawPixel(X0 + x, Y0 - y);
      DrawPixel(X0 - x, Y0 + y);
      DrawPixel(X0 - x, Y0 - y);
      error = 2 * (delta + y) - 1;
      if (delta < 0 && error <= 0)
      {
          ++x;
          delta += 2 * x + 1;
          continue;
      }
      error = 2 * (delta - x) - 1;
      if (delta > 0 && error > 0)
      {
          --y;
          delta += 1 - 2 * y;
          continue;
      }
      ++x;
      delta += 2 * (x - y);
      --y;
  }
}
void DrawOctoPixel(int x0, int y0) {
  for (int i = 0; i <= 7; i++) {
      for (int j = 0; j <= 7; j++) {
          DrawPixel(x0 * 8 + i, y0 * 8 + j);
      }
  }

}

void DrawOctoPixelGray(int x0, int y0) {
  for (int i = 0; i <= 7; i += 2) {
      for (int j = 0; j <= 7; j += 2) {
          DrawPixel(x0 * 8 + i, y0 * 8 + j);
      }
  }

}

// read from ADC 
int AnalogRead(int N) // Функция принимает номер канала для преобразования
{
  ADC1->SQR3 = N; // Выбран полученный из аргумента канал
  for (int a = 0; a < 100; a++) { asm("NOP"); } // Ожидать больше 100 тактов
  ADC1->CR2 |= ADC_CR2_SWSTART; // Начать преобразование
  while (!(ADC1->SR & ADC_SR_EOC)) { asm("NOP"); } // Ждать установки бита конца операции
  return ADC1->DR; // Вернуть результат преобразования
}

void Connect() {

}

// POT1 = A13 POT2 = A12

enum GameStates {
  Connection, // первый коннект плат
  SelectCoords, // выбор координат для атаки (ход)
  TransieveCoord, // отпарвка выбранных координат
  GetAnsver, // получить ответ - попал не попал
  UpdateField, // отрисовать попал / непопал  
  GetEnemyChoose, // получить ответ врага (его попытку атаки - координату)
  SendAnsver, // ответить ему - попал/не попал, потом снова SelectCoords
  Aiming
};

class GameMaster {

private:
  std::array<char, 16> rx;
  char coord[2];
  char answer[2];
  char hitCoord[4];
  bool my_move = false;
  bool setCoord = false;
public:
  GameStates currState;
  char m_ships[8][8] = { { ' ', ' ', ' ', ' ', ' ',' ',' ',' ' },
                        { '*', ' ', ' ', ' ', ' ',' ',' ',' ' },
                        { '*', ' ', ' ', ' ', ' ',' ',' ',' ' },
                        { '*', ' ', ' ', ' ', ' ','*',' ',' ' },
                        { ' ', ' ', ' ', ' ', ' ','!',' ',' ' },
                        { ' ', '*', '*', '*', ' ','!',' ',' ' },
                        { ' ', ' ', ' ', ' ', ' ',' ',' ',' ' },
                        { ' ', ' ', ' ', ' ', ' ','*','*','*' } };
  char e_ships[8][8] = { { ' ', ' ', ' ', ' ', ' ',' ',' ',' ' },
                        { ' ', ' ', ' ', ' ', ' ',' ',' ',' ' },
                        { ' ', ' ', ' ', ' ', ' ',' ',' ',' ' },
                        { ' ', ' ', ' ', ' ', ' ',' ',' ',' ' },
                        { ' ', ' ', ' ', ' ', ' ',' ',' ',' ' },
                        { ' ', ' ', ' ', ' ', ' ',' ',' ',' ' },
                        { ' ', ' ', ' ', ' ', ' ',' ',' ',' ' },
                        { ' ', ' ', ' ', ' ', ' ',' ',' ',' ' } };
public:

  void HalfClear() {

      for (int x = 0; x < 16; x++) {
          for (int y = 0; y < 64; y++) {
              Buffer[y * 16 + x] = 0;
          }
      }
  }

  void setCoords() {
      if (currState == GameStates::Aiming) currState = GameStates::TransieveCoord;
  }

  void TransieveCoord(int x, int y) {
      currState = GameStates::GetAnsver;
  }


  void DrawLine() {
      for (int i = 0; i < 64; i += 2) {
          DrawPixel(64, i);
      }
  }

  void MakeMove() {
    short x, y;
    short tmp_x = 0, tmp_y = 0;
    if (currState == GameStates::SelectCoords) {
      currState = GameStates::Aiming;
      
      while(currState != GameStates::TransieveCoord){ // статус отправка координат ставится в методе setCoords который вызывается по прерыванию EXTI4 по нажатию кнопки PE4
        x = AnalogRead(12) / 512;// 0x008
        y = AnalogRead(13) / 512;
        if (tmp_x != x || tmp_y != y) DrawAim(x, y);
        tmp_x = x;
        tmp_y = y;
        UpdateScreen();
      }
      
      printf("%s\n", "attack!");
        
      coord[0] =x;
      coord[1] =y;
      printf("x = %d y = %d\n", x,y);
      UART6_Send(coord, strlen(coord));
      currState = GameStates::GetAnsver;
    }
    if(currState == GameStates::GetAnsver){
      if(getAnswer() ){
       UpdateEnemyField(x,y,'*');
      }
      else  UpdateEnemyField(x,y,'!');
      DrawField();
      currState = GameStates::GetEnemyChoose; 
    }
    if(currState == GameStates::GetEnemyChoose){
      if(getHit(tmp_x, tmp_y)){
       
        answer[0] ='1';
        printf("->answer = %c\n", answer[0]);
        UpdateMyField(tmp_x, tmp_y, '!');
      }else answer[0] = '0';
      
      printf("->answer = %c\n", answer[0]);

      // отправляем подтверждение/опровержение попадания 
        UART6_Send(answer, strlen(answer));
      currState = GameStates::SelectCoords;
      // DrawField();
      //AttackXY(x, y);
    }
  }
  void sendAnswer(){
    
  }
  bool getAnswer(){
    if(currState == GameStates::GetAnsver){ // ждем результата стрельбы
      // !!!!!!!!!!! заглушка, убрать когда вторую плату сделаю
   //   return 1;
      // !!!!!!!!!!!
      while (1) {
        int n;
        
        printf("->wait shoot result\n");
        for (int i = 0; i < 20000000; i++);
        int size = UART6_GetString(answer, sizeof(answer));
        if (size > 1) {
            printf("<-%s", answer);

            if (strcmp(answer, "1\r\n") == 0) {
              printf("<-popal\n"); 
              return true;
            }
            else {
               printf("<-ne popal\n"); 
              return false;
            }
        }
      }
      currState = GameStates::GetEnemyChoose;
    }
    
    else {
       printf("state != getAns\n"); 
      return false;
    }
  }
  
  bool getHit(short& x, short& y) {
    if(currState == GameStates::GetEnemyChoose) { // ждем выбор соперника
        
      // !!!!!!!!!!! заглушка, убрать когда вторую плату сделаю
    //  x = rand()%7+0;
    //  y = rand()%7+0; 
    //  if(m_ships[x][y] == '*') return 1;
    // else return 0;
      // !!!!!!!!!!!
      while (1) {
        int n;
        
        printf("->wait shoot result\n");
        for (int i = 0; i < 20000000; i++);
        int size = UART6_GetString(hitCoord, sizeof(hitCoord));
        if (size > 1) {
            printf("<-%s", hitCoord);
            x = hitCoord[0] - '0';
            y = hitCoord[1] - '0';
            if(m_ships[x][y] == '*') return 1;
            else return 0;
        }
      }
      currState = GameStates::SelectCoords;
    }
    
    else {
       printf("state != getAns\n"); 
      return false;
    }
   }


  void AttackXY(short x, short y) {
      // todo - get isHit if popal from getHit()
      bool isHit = getHit(x, y);

      if (isHit) DrawOctoPixel(x, y);
      else DrawOctoPixelGray(x, y);

  }

  void DrawAim(short x, short y) {
      Clear();
      DrawField();
      for (int i = 0; i < 64; i++) {
          DrawPixel(64 + 8 * x + 3, i);
      }
      for (int i = 64; i < 128; i++) {
          DrawPixel(i, y * 8 + 3);
      }

  }
  void DrawShips() {
      for (int i = 0; i < 8; i++) {
          for (int j = 0; j < 8; j++) {
              if (m_ships[j][i] == '*') {
                  DrawOctoPixel(j, i);
              }
              if (m_ships[j][i] == '!') {
                  DrawOctoPixelGray(j, i);
              }
          }
      }

      for (int i = 0; i < 8; i++) {
          for (int j = 8; j < 16; j++) {
              if (e_ships[j-8][i] == '*') {
                  DrawOctoPixel(j, i);
              }
              if (e_ships[j-8][i-8] == '!') {
                  DrawOctoPixelGray(j, i);
              }
          }
      }
  }
  void DrawField() {
      Clear();
      DrawShips();

      DrawLine();

      UpdateScreen();
  }

  void UpdateEnemyField(int x, int y, char ch) {
      e_ships[x][y] = ch;
  }

  void UpdateMyField(int x, int y, char ch) {
      m_ships[x][y] = ch;
  }

  bool attackShips(int x, int y) {
      if (m_ships[x - 1][y - 1] == '*') {
          m_ships[x - 1][y - 1] = '!';
          return 1;
      }
  }

};
GameMaster GM;

extern "C" void EXTI4_IRQHandler() // Название функции для EXTI4
{
  EXTI->PR = 1 << 4; // Снять бит активности прерывания (прерывание 4 обработано)
  if (GPIOE->IDR & (1 << 4)) // Прочесть значение входящего сигнала в PE4
  {
      GM.setCoords();
  }

}
void setField(char* field, uint8_t W, uint8_t H) {

}

bool MasterHandShake() {
  char handshake[32] = "hello\r\n"; // Буфер временных данных
  char test[32] = "OK\r\n";
  char rc[32] = "hello\r\n";
  char rx[32];

  while (1) {
      int n;
      
       printf("->try to connect\n");
      UART6_Send(handshake, strlen(handshake));

      for (int i = 0; i < 20000000; i++);
      int size = UART6_GetString(rx, sizeof(rx));
      if (size > 1) {
          printf("<-%s", rx);

          if (strcmp(rx, "OK\r\n") == 0) {
              
              printf("->%s", test);
              return true;
          }
      }
  }
}




bool Attack(int x, int y) {
  x -= 1;
  y -= 1;
  char send[32];
  char test[32];
  uint8_t i = 0;

  while (i < 4) {
      send[i++] = x & 1 + '0';
      x >>= 1;
  }
  while (i < 8) {
      send[i++] = y & 1 + '0';
      y >>= 1;
  }
  send[i++] = '\r';
  send[i++] = '\n';
  send[i] = '\0';
  printf("->%s", send);

  UART6_Send(send, strlen(send));
  for (int i = 0; i < 20000000; i++);


  while (1) {
      int size = UART6_GetString(test, sizeof(test));
      if (size > 1) {
          if (strcmp(test, "1\r\n")) return 1;
          else return 0;
      }
  }

}

bool Recieve() {
  char test[32];
  char send[32];

  while (1) {
      int size = UART6_GetString(test, sizeof(test));
      printf("<-%s", test);
      if (size > 1) {
          if (strlen(test) > 0)
          {
              uint8_t x = 0, y = 0;
              uint8_t i = 0;

              while (i < 4) {
                  x = test[i++] - '0';
                  x <<= 1;
              }
              while (i < 8) {
                  y = test[i++] - '0';
                  y <<= 1;
              }

              if (GM.m_ships[x][y] == '*') {
                  GM.m_ships[x][y] = '!';

                  send[0] = '1';
                  send[1] = '\r';
                  send[2] = '\n';
                  send[3] = '\0';
                  printf("->%s", send);

                  UART6_Send(send, strlen(send));
                  for (int i = 0; i < 20000000; i++);
                  return 1;
              }
              else {
                  send[0] = '0';
                  send[1] = '\r';
                  send[2] = '\n';
                  send[3] = '\0';
                  printf("->%s", send);

                  UART6_Send(send, strlen(send));
                  return 0;

              }
          }
      }
  }

}


enum States {
  TryToConnect,
  Connected,
  MakeMove,
  SendMove,
  GetAnswer,
  GetAttack
};

bool SlaveHandShake() {
  char handshake[32] = "hello\r\n"; // Буфер временных данных
  char test[32] = "OK\r\n";
  char rc[32] = "hello\r\n";
  char rx[32] = "";

  while (1) {
      int n;
      
       printf("->wait connection\n");
    //  UART6_Send(test, strlen(handshake));

      for (int i = 0; i < 20000000; i++);
      int size = UART6_GetString(rx, sizeof(rx));
      if (size > 1) {
          printf("<-%s\n", rx);

          if (strcmp(rx, "hello\r\n") == 0) {
              
              printf("->%s", test);
              UART6_Send(test, strlen(test));
              return true;
          }
          else rx = "";
      }
  }
}


void MasterConnect(){
  if(MasterHandShake()){
    printf("->connected\n");
    GM.currState = GameStates::SelectCoords;

  }
}
void SlaveConnect(){
  if(SlaveHandShake()){
    printf("->connected\n");
    GM.currState = GameStates::GetEnemyChoose;

  }
}
int main()
{
  ActivateUSARTs();
  
  RCC->AHB1ENR |= RCC_AHB1ENR_GPIOHEN | RCC_AHB1ENR_GPIOEEN;
  GPIOH->MODER &= ~GPIO_MODER_MODER14;
  GPIOH->MODER |= GPIO_MODER_MODER14_0;
  GPIOE->MODER &= ~(GPIO_MODER_MODER0 | GPIO_MODER_MODER1 | GPIO_MODER_MODER2 | 
  GPIO_MODER_MODER3);
  GPIOH->BSRRL = GPIO_BSRR_BS_14;

  // прерывания 
  GPIOE->MODER &= ~GPIO_MODER_MODER4; // Сброс режима (режим входа) PE4
  RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN; // Задействован SYSCFG
  SetEXTI(PORT::E, 4, true, true); // Прерывание PE4 при появлении и исчезновении сигнала
  NVIC_SetPriority(EXTI4_IRQn, 0); // Высший приоритет прерывания
  NVIC_EnableIRQ(EXTI4_IRQn); // Прерывание активировано



  SetADC();
  setI2C_2();


  turnOnSSD1309();
  Clear();

  GM.DrawField();
  UpdateScreen();



  // UART6_Send(handshake, strlen(handshake)); // Получаем входящие данные
  int del = 0;
 
  int x = 1, y = 1;
#ifdef MASTER
  MasterConnect();
#else
  SlaveConnect();
#endif  
  //GM.currState = GameStates::SelectCoords;
  while (1)
  {

      printf("curr state %d\n", GM.currState);
      GM.DrawField();
      GM.MakeMove();

     //GM.getHit();
     // GM.DrawField();
      
      
      
    

      /*
      Connection, // первый коннект плат
      SelectCoords, // выбор координат для атаки (ход)
      TransieveCoord, // отпарвка выбранных координат
      GetAnsver, // получить ответ - попал не попал
      UpdateField, // отрисовать попал / непопал  
      GetEnemyChoose, // получить ответ врага (его попытку атаки - координату)
      SendAnsver
      */

      //   Recieve();
      //    if(Attack(x++%8, y++%8)){
      //      GM.UpdateEnemyField(x,y, '*');
      //    }
      //    else GM.UpdateEnemyField(x,y, '!');
      //     

    UpdateScreen();
      
  }
}
