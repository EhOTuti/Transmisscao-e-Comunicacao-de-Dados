#include <WiFi.h>
// flag e tipos
#define FLAG 0xE6
#define T_DATA 0xF0
#define T_IMG 0x8C
#define T_ACK 0xF8
#define T_END 0xDE

// padrão usado no CRC
#define PADRAO 0b110110011
#define LOW_PAD (PADRAO & 0xFF)

//Tempo do TimeOut (em milissegundos)
#define TIMEOUT 1500

// pinos usados nos módulos RF
#define TX 17
#define TX_VCC 25
#define RX 16
#define RX_VCC 33

// TRANSMISSOR

//variáveis globais o quadro de informações e mensagens a ser enviadas
uint8_t frame[21];
uint8_t indx;
uint8_t seqAtual = 0;

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
    Serial2.begin(3200, SERIAL_8N1, RX, TX);
}

//Leitura de apenas um Byte
uint8_t lerByte(){
    while(!Serial2.available());
    return Serial2.read();
}

int lerByteComTimeout(unsigned long inicio, unsigned long timeoutMax) {
    while (!Serial2.available()) {
        if (millis() - inicio > timeoutMax) return -1;
    }
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
    digitalWrite(RX_VCC, HIGH);
    delay(200);

    while(Serial2.available()) Serial2.read();

    unsigned long inicio = millis();
    
    while(millis() - inicio < TIMEOUT){ 
        if(!Serial2.available()) {
            delay(1);
            continue;
        }
        
        int flag = Serial2.read();
        if(flag == FLAG) {                        
            int type = lerByteComTimeout(inicio, TIMEOUT);
            int seq  = lerByteComTimeout(inicio, TIMEOUT);
            int len  = lerByteComTimeout(inicio, TIMEOUT);
            int fcs = lerByteComTimeout(inicio, TIMEOUT);
            
            if (type == -1 || seq == -1 || len == -1) return false;
            
            if(type == T_ACK && seq == seqEsp){
                digitalWrite(RX_VCC, LOW);
               return true;
            }
        }
    }
    digitalWrite(RX_VCC, LOW);
    return false; 
}

void loop() {
    if(Serial.available()){
      String msgUser = Serial.readStringUntil('\n');
      msgUser.trim();

      if(msgUser.length() == 0) return;

      uint8_t seqAtual = 0;
      
      uint8_t msgLen = msgUser.length();

      if(msgUser.startsWith("TXT: ")){
        uint8_t indxAtual = 5;
        uint8_t buff[16];

        while(indxAtual < msgUser.length()){
            uint8_t restoMsg = msgUser.length() - indxAtual;
            uint8_t tamMsg = 0;
            uint8_t tipo;
            if(restoMsg > 16){
                tamMsg = 16;
                tipo = T_DATA;
            }
            else{
                tamMsg = restoMsg;
                tipo = T_END;
            }
            for(uint8_t i = 0; i < tamMsg; i++){
                buff[i] = msgUser[i + indxAtual];
            }
            digitalWrite(TX_VCC, HIGH);
            delay(200);

            criaFrame(tipo, seqAtual, tamMsg, buff);
            for(int i = 0; i < 10; i++) Serial2.write(0xAA);
            Serial2.write(frame, indx);
            Serial2.flush();

            digitalWrite(TX_VCC, LOW);
            while(Serial2.available()) Serial2.read();
            Serial.print("Dados enviados: "); Serial.print(" ("); Serial.print(millis()); Serial.println("ms)");
            delay(300);

            if(esperaACK(seqAtual) == true){
                Serial.println("ACK recebido!");
                digitalWrite(TX_VCC, LOW);
                seqAtual++;
                indxAtual += tamMsg;
                delay(500);
            }
            else{
                Serial.println("Tempo expirado, Retransmitindo!");
                digitalWrite(RX_VCC, LOW);

            }
        }
      }
      else if(msgUser.startsWith("IMG: ")){
        uint8_t indxAtual = 5;
        uint8_t buff[16];

        while(indxAtual < msgUser.length()){
            uint8_t restoMsg = msgUser.length() - indxAtual;
            uint8_t tamMsg = 0;
            uint8_t tipo;
            if(restoMsg > 16){
                tamMsg = 16;
                tipo = T_IMG;
            }
            else{
                tamMsg = restoMsg;
                tipo = T_END;
            }
            for(uint8_t i = 0; i < tamMsg; i++){
                buff[i] = msgUser[i + indxAtual];
            }
            digitalWrite(TX_VCC, HIGH);
            delay(200);

            criaFrame(tipo, seqAtual, tamMsg, buff);
            for(int i = 0; i < 10; i++) Serial2.write(0xAA);
            Serial2.write(frame, indx);
            Serial2.flush();

            digitalWrite(TX_VCC, LOW);
            while(Serial2.available()) Serial2.read();
            Serial.print("Dados enviados: "); Serial.print(" ("); Serial.print(millis()); Serial.println("ms)");
            delay(300);

            if(esperaACK(seqAtual) == true){
                Serial.println("ACK recebido!");
                digitalWrite(TX_VCC, LOW);
                seqAtual++;
                indxAtual += tamMsg;
                delay(500);
            }
            else{
                Serial.println("Tempo expirado, Retransmitindo!");
                digitalWrite(RX_VCC, LOW);

            }
        }

      
      }
            else{
        Serial.println("Mensagem inválida!");
      }
    }
}
