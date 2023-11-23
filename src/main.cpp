#include <Arduino.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <time.h>
#include <sys/time.h>
#include <ESP32Time.h>
#include <EEPROM.h>
#include <string.h>
#include "time.h"

#define UDP_PORT 4210
#define EGPIB 2
#define EGPIA 14
#define SENSORA 33
#define DELAYCNT 40

#define EEPROM_SIZE 5

const char *WIFISSID = "Jaqueira";
const char *WIFIPASS = "jacajuice";

const char *ntpServer1 = "a.st1.ntp.br";
const char *ntpServer2 = "b.st1.ntp.br";
const char *ntpServer3 = "c.st1.ntp.br";
const long gmtOffset_sec = -10800;
const int daylaightOffset_sec = 0;
struct tm timeinfo;

unsigned long oldTimestap = 0;

enum
{
  DOWN,
  UP
};
enum
{
  LOCKED,
  UNLOCKED
};
enum
{
  MOVE,
  LIMIT
};

/**
 * @brief Enumeração de endereços da EEPROM
 *
 */
enum EEPROM_ADDR
{
  HOUR_ADDR,
  MINUTE_ADDR,
  POSITION_ADDR,
  DELAY_ADDR
};

typedef struct state
{
  uint8_t state, address;

} state_t;

ESP32Time rtc(0);

WiFiUDP UDP;
char packet[255];
const char reply[] = "OK\n";
const uint8_t cmdrply[] = {'D', 'O', 'N', 'E', '\n'};

// const uint8_t header[] = " _____ _____ _____ _____ _____ _____ _____ \n|     |     | __  |_   _|     |   | |  _  |\n|   --|  |  |    -| | | |-   -| | | |     |\n|_____|_____|__|__| |_| |_____|_|___|__|__|\n";
// const uint8_t header[] = "   __ _  ___ ________  __ _ \n ,'_,' \\/ o /_  _/ / |/ .' \\\n/ // o /  ,' / // / || / o /\n|__|_,/_/`_\\/_//_/_/|_/_n_/ \n";
const uint8_t header[] = "|     | __  | __  |     |   __| __  |  _  |   __|\n|-   -|    -|    -|-   -|  |  |    -|     |__   |\n|_____|__|__|__|__|_____|_____|__|__|__|__|_____|";

const uint8_t help_msg[] = {"RAS-Irrigação\nComandos:\n$? Exibe os dados do Sensor A\n"};
char buff[50];

int delaycount = DELAYCNT, bufSize, posstate = MOVE;
bool lockstate = LOCKED, direction = DOWN;

/**
 * Estados graváveis (valor e endereço na EEPROM)
 */
state_t HOUR_ST = {0, HOUR_ADDR}, MINUTE_ST = {0, MINUTE_ADDR}, DELAY_ST = {DELAYCNT, DELAY_ADDR};

/**
 * @brief Grava um estado específico tanto na memória quanto na EEPROM
 *
 * @param stt Estado a ser registrado
 * @param data Dado a ser registrado no estado
 */
void set_state(state_t *stt, uint8_t data)
{
  stt->state = data;
  EEPROM.write(stt->address, stt->state);
  EEPROM.commit();
}

void irrigarTimer(int interval)
{
  digitalWrite(EGPIA, LOW);
  delay(interval);
  digitalWrite(EGPIA, HIGH);
}

/**
 * @brief Dispara um evento de irrigação no horário marcado do dia
 *
 * @param oldTimestap último registro de tempo em que um evento aconteceu
 */
void timeEvent(unsigned long *oldTimestap)
{
  if ((millis() - *oldTimestap) >= 40000)
  {
    *oldTimestap = millis();
    timeinfo = rtc.getTimeStruct();
    if (timeinfo.tm_hour == HOUR_ST.state && timeinfo.tm_min == HOUR_ST.state)
    {
      irrigarTimer(DELAY_ST.state * 100);
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Serial.println();

  WiFi.begin(WIFISSID, WIFIPASS);
  Serial.print("Conectando com ");
  Serial.print(WIFISSID);
  // Loop continuously while WiFi is not connected
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(100);
    Serial.print(".");
  }

  // Connected to WiFi
  Serial.println();
  Serial.print("Connected! IP address: ");
  Serial.println(WiFi.localIP());

  // Tempo para estabilizar conexão
  delay(2000);

  // Conexão com o servidor NTP.br
  configTime(gmtOffset_sec, daylaightOffset_sec, ntpServer1, ntpServer2, ntpServer3);
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Falha em obter tempo por NTP!");
  }
  else
  {
    time_t now;
    rtc.setTime(time(&now));
  }

  // Recuperando estados anteriores
  EEPROM.begin(EEPROM_SIZE);
  // TODO: Criar estados
  // DELAY_ST.state = EEPROM.read(DELAY_ADDR);
  // LOCK_ST.state = EEPROM.read(LOCK_ADDR);
  // POSITION_ST.state = EEPROM.read(POSITION_ADDR);
  // DIRECTION_ST.state = EEPROM.read(DIRECTION_ADDR);

  // Begin listening to UDP port
  UDP.begin(UDP_PORT);
  Serial.print("Listening on UDP port ");
  Serial.println(UDP_PORT);

  /**
   * Configuração dos pinos
   */
  // pinMode(SENSORA, INPUT);
  pinMode(EGPIA, OUTPUT);
  pinMode(EGPIB, OUTPUT);
  digitalWrite(EGPIA, HIGH);
  digitalWrite(EGPIB, HIGH);
}

/**
 * @brief Envia um pacote UDP
 *
 * @param data Carga do pacote a ser enviado
 * @param size Tamanho em bytes da carga
 */
void sendPacket(const uint8_t *data, int size)
{
  UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
  UDP.write(data, size);
  UDP.endPacket();
}

/**
 * @brief Envia um pacote UDP
 *
 * @param data Carga do pacote a ser enviado
 */
void sendPacket(uint8_t data)
{
  UDP.beginPacket(UDP.remoteIP(), UDP.remotePort());
  UDP.write(data);
  UDP.endPacket();
}

void ligaBomba(int pinobomba)
{
  digitalWrite(pinobomba, LOW);
}

void desligaBomba(int pinobomba)
{
  digitalWrite(pinobomba, HIGH);
}

void loop()
{
  int packetSize = UDP.parsePacket();
  if (packetSize)
  {
    Serial.println("Pacote recebido!");
    int len = UDP.read(packet, 255);
    if (len > 0)
    {
      packet[len] = '\0';
      strtok(packet, "\n");
      sendPacket((const uint8_t *)reply, 3);
    }

    Serial.println(packet);

    /**
     * Switch de comandos
     */
    switch (packet[0])
    {
      // TODO: Criar comandos
    // Modo de liga/desliga a bomba
    case 'B':
      switch (packet[1])
      {
      case '0':
        desligaBomba(EGPIA);
        break;
      case '1':
        ligaBomba(EGPIA);
        break;
      default:
        break;
      }
      break;
    // Configuração de timer de parada
    case 'S':
      switch (packet[1])
      {
      case '0':
        set_state(&DELAY_ST, 30);
        break;
      case '1':
        set_state(&DELAY_ST, 35);
        break;
      case '2':
        set_state(&DELAY_ST, 40);
        break;
      case '3':
        set_state(&DELAY_ST, 45);
        break;
      case '4':
        set_state(&DELAY_ST, 50);
        break;
      case '5':
        set_state(&DELAY_ST, 55);
        break;
      case '6':
        set_state(&DELAY_ST, 60);
        break;
      case 'S':
        if (len > 4)
        {
          char num[] = {
              packet[2],
              packet[3],
              '\0'};
          set_state(&DELAY_ST, atoi(num));
        }
        break;

      default:
        break;
      }
      irrigarTimer(DELAY_ST.state * 100);
      break;

    case 'H':
      if (len > 3)
      {
        char num[] = {
            packet[2],
            packet[3],
            '\0'};
        set_state(&HOUR_ST, atoi(num));
      }
      break;
    case 'M':
      if (len > 3)
      {
        char num[] = {
            packet[2],
            packet[3],
            '\0'};
        set_state(&MINUTE_ST, atoi(num));
      }
      break;

    // Mensagem de ajuda
    case '?':
      // TODO: Configurar mensagens de ajuda
      sendPacket(header, sizeof(header) - 1);
      sendPacket(help_msg, sizeof(help_msg) - 1);
      // bufSize = sprintf(buff, "Tempo atual: %d\n", delaycount);
      bufSize = sprintf(buff, "Tempo atual: %d\n", DELAY_ST.state);
      sendPacket((const uint8_t *)buff, bufSize);
      bufSize = sprintf(buff, "RTC:%s\n", rtc.getTime("%H %M").c_str());
      sendPacket((const uint8_t *)buff, bufSize);
      break;

    // Comandos de controle
    case '$':
      // Configurar Retorno de estados
      switch (packet[1])
      {
      case '?':
        memset(buff, 0, 50);
        bufSize = sprintf(buff, "SENSOR A: %d\n", analogRead(SENSORA));
        sendPacket((const uint8_t *)buff, bufSize);
      default:
        break;
      }
      break;
    default:
      break;
    }
  }

  if (analogRead(SENSORA) > 1500)
  {
    irrigarTimer(2000);
  }

  timeEvent(&oldTimestap);
}
