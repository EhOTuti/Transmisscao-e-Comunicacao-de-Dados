#include <WiFi.h>
// flag e tipos
#define FLAG 0xE6
#define T_DATA 0xF0
#define T_ACK 0xF8
#define T_END 0xDE

// padrão usado no CRC
#define PADRAO 0b110110011
#define LOW_PAD (PADRAO & 0xFF)

//Tempo do TimeOut (em milissegundos)
#define TIMEOUT 14000

// pinos usados nos módulos RF
#define TX 17
#define TX_VCC 25
#define RX 16
#define RX_VCC 33

// TRANSMISSOR

//variáveis globais o quadro de informações e mensagens a ser enviadas
uint8_t frame[21];
uint8_t indx;
const char* msg[] = {
  "Ola, Mundo!",
  "Hello, World!",
  "aborgue",
  "siri cascudo",
  "canguru",
  "zovão do bob"
  };
const uint8_t totalMsg = (sizeof(msg) / sizeof(msg[0]));


void setup() {

    // Controle do módulo RF
    pinMode(TX_VCC, OUTPUT);
    pinMode(RX_VCC, OUTPUT);
    digitalWrite(TX_VCC, HIGH);
    digitalWrite(RX_VCC, LOW);
    delay(100);

    //Desliga o módulo WiFi para evitar interferências
    WiFi.mode(WIFI_OFF);

    Serial.begin(115200);
    Serial2.begin(300, SERIAL_8N1, RX, TX);
}

//Leitura de apenas um Byte
uint8_t lerByte(){
    while(!Serial2.available());
    return Serial2.read();
}

//Cálculo da Redundância com CRC
uint8_t CRC(uint8_t *data, uint8_t len){
    uint8_t fcs = 0;
    for(uint8_t i = 1; i < len; i++){
        fcs ^= data[i];
        for(uint8_t j = 0; j < 8; j++){
            if(fcs & 0x80){
                fcs = (fcs << 1) ^ LOW_PAD;
            }
            else{
                fcs <<= 1;
            }
        }
    }
    return fcs;
}

//Monta o Quadro de informações
void criaFrame(uint8_t type, uint8_t seq, uint8_t len, uint8_t *data){
  indx = 0;
  frame[indx++] = FLAG;
  frame[indx++] = type;
  frame[indx++] = seq;
  frame[indx++] = len;
  memcpy(&frame[indx], data, len);
  indx += len;
  frame[indx++] = CRC(frame, indx);
}

// Espera o ACK do receptor
bool esperaACK(uint8_t seqEsp){
    unsigned long inicio = millis();
    while(millis() - inicio < TIMEOUT){ 
        if(!Serial2.available()) continue;
        uint8_t flag = lerByte();
        if(flag == FLAG) {
        Serial.println("-> FLAG!");                            
        uint8_t type = lerByte();
        uint8_t seq  = lerByte();
        if(type == T_ACK && seq == seqEsp) return true;
        return false;
        }
    }
    return false; 
}

void loop() {
    // Loop que envia todas as mensagens
    for(uint8_t i = 0; i < totalMsg;){
      digitalWrite(TX_VCC, HIGH);
      delay(100);
      criaFrame(T_DATA, i, strlen(msg[i]), (uint8_t*)msg[i]);
      for(int j = 0; j < 10; j++) Serial2.write(0xAA);
      Serial2.write(frame, indx);
      Serial2.flush();

      digitalWrite(TX_VCC, LOW);
      while(Serial2.available()) Serial2.read();
      Serial.print("Dados enviados: ("); Serial.print(millis()); Serial.println("ms)");
      delay(800);
      digitalWrite(RX_VCC, HIGH);
      delay(100);
      if(esperaACK(i) == true){
        Serial.println("ACK Recebido!");
        digitalWrite(RX_VCC, LOW);
        i++;

      }
      else{
        Serial.println("Tempo Expirado ou dados corrompidos, Retransmitindo!");
        digitalWrite(RX_VCC, LOW);
      }
    }
}