#include "stm32f4xx.h"
#include <stdio.h>
#include <string.h>
#include <cstdlib>
#include <map>
#include <array>
#include <stdint.h>
#include "initPeriph.h"
#define MASTER

class GameMaster;


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


void periphInit();
class ConnectMaster{
public:
  bool checkCorrect(char *arr, int size){
    int t = arr[0] - '0';
    int i =1;
    while(i < size && arr[i] != '\0' ){
      i++;
    }
    printf("size = %d\n", t);
    if (i == t) return 1;
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
private:
  std::array<char,16> handshake {"6hello\n"}; // Буфер временных данных
  std::array<char,16> test{"3OK\n"};
  std::array<char,16> rx;
  std::array<char,16> gethandshake;
  bool MasterHandShake() {
    

    while (1) {
        int n;
        
        printf("->try to connect- %s\n",handshake.data() );
       // checkCorrect(handshake.data(), handshake.size());
        UART6_Send(handshake.data(), handshake.size());

        for (int i = 0; i < 20000000; i++);
        int size = UART6_GetString(rx.data(), (rx).size());
        if (checkCorrect(rx.data(), (rx).size())) {
            printf("<-%s", rx.data());
            
            if (strcmp(rx.data(), "2OK\n") == 0) {
                
                printf("->%s", test.data());
                return true;
            }
            
        }
    }
  }

  bool SlaveHandShake() {
  
  
    while (1) {
        int n;
        
         printf("->wait connection\n");
      //  UART6_Send(test, strlen(handshake));

        for (int i = 0; i < 20000000; i++);
        int size = UART6_GetString(gethandshake.data(), gethandshake.size());
        if (size > 1) {
            printf("<-%s", rx.data());

            if (strcmp(rx.data(), "hello\r\n") == 0) {
                
                printf("->%s", test.data());
                UART6_Send(test.data(), test.size());
                return true;
            }
        }
    }
  }

};
int main()
{
  periphInit();

  ConnectMaster CM;
  GM.DrawField();
  UpdateScreen();

#ifdef MASTER
  CM.MasterConnect();
#else
  CM.SlaveConnect();
#endif  
  //GM.currState = GameStates::SelectCoords;
  while (1)
  {

     printf("curr state %d\n", GM.currState);

      GM.DrawField();
      GM.MakeMove();

      /*
      Connection, // первый коннект плат
      SelectCoords, // выбор координат для атаки (ход)
      TransieveCoord, // отпарвка выбранных координат
      GetAnsver, // получить ответ - попал не попал
      UpdateField, // отрисовать попал / непопал  
      GetEnemyChoose, // получить ответ врага (его попытку атаки - координату)
      SendAnsver
      */

    UpdateScreen();
      
  }
}

void periphInit(){
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
}
