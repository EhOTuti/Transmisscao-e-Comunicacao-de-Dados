#include <LiquidCrystal_I2C.h>
#include <Wire.h>
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

// pinos usados nos módulos RF
#define TX 17
#define TX_VCC 25
#define RX 16
#define RX_VCC 33
// RECEPTOR

// controlador do LCD
LiquidCrystal_I2C lcd (0x27, 16, 2);

//variáveis globais o quadro de informações
uint8_t frame[21];
uint8_t indx;

void setup() {

  // Controle do módulo RF
    pinMode(TX_VCC, OUTPUT);
    pinMode(RX_VCC, OUTPUT);
    digitalWrite(TX_VCC, LOW);
    digitalWrite(RX_VCC, HIGH);
    delay(100);

    //Desliga o módulo WiFi para evitar interferências
    WiFi.mode(WIFI_OFF);

    Serial.begin(115200);
    Serial2.begin(3200, SERIAL_8N1, RX, TX);
    delay(1000);

    //Inicializando o LCD
    Wire.begin();
    lcd.init();
    lcd.backlight();
    lcd.print("Aguardando...");
  
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
void criaFrame(uint8_t type, uint8_t seq, uint8_t len, uint8_t *data, uint8_t fcs){
  indx = 0;
  frame[indx++] = FLAG;
  frame[indx++] = type;
  frame[indx++] = seq;
  frame[indx++] = len;
  memcpy(&frame[indx], data, len);
  indx += len;
  frame[indx++] = fcs;
  return;
}

//Envia o Quadro ACK como resposta
void enviaACK(uint8_t seq){
  delay(500);
  indx = 0;
  frame[indx++] = FLAG;
  frame[indx++] = T_ACK;
  frame[indx++] = seq;
  frame[indx++] = 0;
  frame[indx++] = CRC(frame, indx);
  for(int i = 0; i < 10; i++) Serial2.write(0xAA);
  Serial2.write(frame, indx);
  Serial2.flush();
  Serial.println("ACK enviado!");
  delay(500);
  return;
}

// verifica se o quadro de informações lido está sem erro
bool verificaFrame(uint8_t type, uint8_t seq, uint8_t len, uint8_t *data, uint8_t fcs){
    criaFrame(type, seq, len, data, fcs);
    uint8_t novoFcs = CRC(frame, (len + 5));
    if(novoFcs != 0x00){
        return false;
    }
    return true;
}

//extrai todos os bytes contidos em dados
void extrData(uint8_t *data, uint8_t len){
  for(uint8_t i = 0; i < len; i++){
    data[i] = lerByte();
  }
}

void encerraMensagem(){
  delay(1500);
  lcd.clear();
  lcd.print("Encerrando...");
  Serial.println("Mensagem recebida com sucesso!");
  return;
}

void loop() {
    digitalWrite(RX_VCC, HIGH);
    delay(200);
    uint8_t flag = 0, type  = 0, tempTipo;
    
    //Lê os bytes até encontrar o início do quadro
    while(true){
        flag = lerByte();
        if(flag != FLAG) continue;
        type = lerByte();
        if(type == T_DATA || type == T_ACK || type == T_END || type == T_IMG) break;
    }
    uint8_t seq = lerByte();
    uint8_t len = lerByte();

    //Verificação que evita overflow do buffer
    if(len  > 16 || (type != T_DATA && type != T_ACK && type != T_END && type != T_IMG)){
      Serial.println("Frame inválido!");
      while(Serial2.available()) Serial2.read();
      return;
    }
    uint8_t data[16];
    extrData(data, len);
    uint8_t fcs = lerByte();
    if(verificaFrame(type, seq, len, data, fcs) == true){
      if(type == T_DATA){
      tempTipo = type;
      digitalWrite(RX_VCC, LOW);
      digitalWrite(TX_VCC, HIGH);
      delay(200);
      enviaACK(seq);
      Serial.print("Dados Recebidos: ");
      lcd.clear();
      for(uint8_t i = 0; i < len; i++){
        Serial.print((char)data[i]);
        lcd.print((char)data[i]);
      }
      Serial.print("("); Serial.print(millis()); Serial.println(" ms)");
      digitalWrite(TX_VCC, LOW);
      }

      else if(type == T_IMG){
        tempTipo = type;
        digitalWrite(RX_VCC, LOW);
        digitalWrite(TX_VCC, HIGH);
        delay(200);
        enviaACK(seq);
        Serial.print("Imagem sendo recebida, frame: "); Serial.println(seq);
        lcd.clear();
        lcd.print("Frame: "); lcd.print(seq);
        Serial.print("("); Serial.print(millis()); Serial.println(" ms)");
        digitalWrite(TX_VCC, LOW);
      }

      else if(type == T_END){
      if(tempTipo == T_DATA){
        digitalWrite(RX_VCC, LOW);
        digitalWrite(TX_VCC, HIGH);
        delay(200);
        enviaACK(seq);
        Serial.print("Dados Recebidos: ");
        lcd.clear();
        for(uint8_t i = 0; i < len; i++){
          Serial.print((char)data[i]);
          lcd.print((char)data[i]);
        }
        Serial.print("("); Serial.print(millis()); Serial.println(" ms)");
        digitalWrite(TX_VCC, LOW);
      
        encerraMensagem();
      }

      else {
        digitalWrite(RX_VCC, LOW);
        digitalWrite(TX_VCC, HIGH);
        delay(200);
        enviaACK(seq);
        Serial.print("Imagem sendo recebida, frame: "); Serial.println(seq);
        lcd.clear();
        lcd.print("Frame: "); lcd.print(seq);
        Serial.print("("); Serial.print(millis()); Serial.println(" ms)");
        digitalWrite(TX_VCC, LOW);
      
        encerraMensagem();
        }
      

      }

      
    }
    else{
      Serial.println("Dados corrompidos!");
      digitalWrite(TX_VCC, LOW);
    }

}