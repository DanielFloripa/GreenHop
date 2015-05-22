/*********************************************/
/** CODIGO PARA NODO LOCALIZADO NA ENTRADA DO RACK
/** VERSAO especifica, de acordo com OS SENSORES
/* Modelo padrao para todos os sensores V3
/**********************************************/

#include "XBee.h"
#include "Temps.h"
#include "EmonLib.h"
#include <Wire.h>
#include <BMP085.h>

/*********************************************/
/***** DEFINICOES DE PINOS E CONSTANTES ******/
/*********************************************/

/** Pinos dos sensores **/
#define SCT_PIN       A0 
#define MQ2_PIN       A1
#define LM35_PIN      A2
//#define FREE          A3
//#define BMP_SCL_SDA   A4 A5 //reservado ao BMP180/085
#define PIR_PIN       8 
#define DHT11PIN      7 //pino d7 ligado ao DHT11
#define DS18B20_Pin   6 //pino do sensor temperatura DS18B20
#define INCEND_PIN    5
//#define FREE        4
//#define FREE        3
//#define FREE        2

/* Pinos dos LEDs informativos */
#define ERROR_RED    13 // GP LED
#define SEND_GREEN   12 // Led de envio bem sucedido no Xbee
#define SENS_ERR_BLU 11 // Led indicador de erro no DHT11+BMP
#define INC_LED      10// Led  INCENDIO+GAS
#define PIR_LED      9 // Led  PIR

/* Outros valores importantes */
#define DELAY         3000
#define TIMEOUT       100
#define CALIBRA_PIR   6000  //60 segundos
#define QTD_DADOS     15 // Quantidade de dados distintos enviados
#define ERROR_TIME    50 // tempo (em ms) para piscar led
#define ERR1_MAX      9 //
#define ERR2_MAX      5  // ERR* definem o maximo de erro que pode ocorrer em ERR_MAX_TIME (em s) @TODO
#define ERR3_MAX      5  //
#define ERR_MAX_TIME  60000 // tempo maximo (em s) de acumulo de erros de envio de pacotes
#define CELSIUS_BASE  0.4887585532746823069403714565

#define DEBUG_MODE 0  /** 0:Nodo em operacao, 1:Imprime na serial **/ 
#define NO_SENSORS 0  /** 0:Em operacao 1:Sem sensores **/
                      
/*************************************************************/
/********   DEFINICAO DOS SENSORES PRESENTES       ***********/
/******** Use 1 para 'presente' e '0' para ausente ***********/
/*************************************************************/
#define IS_DHT11       1
#define IS_TEMP2       1
#define IS_T2_BMP      1
#define IS_T2_DS       0
#define IS_T2_LM       0 
#define IS_SCT000      0
#define IS_MQ2         0
#define IS_INC_IR      0
#define IS_PRES_IR     0 
#define IS_NOISE       0
#define IS_LIGHT       0
//#define IS_              


// Definicao do vetor de dados do pacote a ser enviado
// Enviaremos #QT_DATA floats de 4 bytes cada
uint8_t payload[QTD_DADOS*4] = { /* A carga maxima eh 63 bytes = 15*4 (+ 3 de sobra) */
  0, 0, 0, 0, // 0 temperature
  0, 0, 0, 0, // 1 humidity
  0, 0, 0, 0, // 2 dewPoint
  0, 0, 0, 0, // 3 temperature2
  0, 0, 0, 0, // 4 MQ2
  0, 0, 0, 0, // 5 PIR
  0, 0, 0, 0, // 6 INCEND
  0, 0, 0, 0, // 7 readPressure
  0, 0, 0, 0, // 8 readSealevelPressure
  0, 0, 0, 0, // 9 readAltitude
  0, 0, 0, 0, // 10 readAltitude Sealevel
  0, 0, 0, 0, // 11 Consumo
  0, 0, 0, 0, // 12 ruido
  0, 0, 0, 0, // 13 luz
  0, 0, 0, 0};// 14 count
                     
XBee xbee = XBee(); /* Instanciando Xbee */
Temps temps;

#if IS_T2_BMP == 1
  BMP085 bmp;
#endif

#if IS_SCT000 == 1
  EnergyMonitor emon1;
#endif

unsigned long tempoAnterior = 0;
int contador = 0, error3 = 0, error2 = 0, error1 = 0;
float media = 0.00f;
float data_f[QTD_DADOS] = 
{0.00f,0.00f,0.00f,0.00f,0.00f,0.00f,0.00f,
0.00f,0.00f,0.00f,0.00f,0.00f,0.00f,0.00f,0.00f};
  
 /* union que converte float para byte */
union u_tag {
  uint8_t b[4];
  float fval;
} u;

/** Endereco SH + SL do XBee recebedor (Coordenador) **/
XBeeAddress64 addr64 = XBeeAddress64(0x0013a200, 0x40ae9a7f);//40ae9a7f
ZBTxRequest zbTx = ZBTxRequest(addr64, payload, sizeof(payload));
ZBTxStatusResponse txStatus = ZBTxStatusResponse();

void(* resetFunc) (void) = 0; //declare reset function @ address 0 // asm volatile ("  jmp 0");

/**************************************************************************/
/***************       SETUP     ******************************************/
/**************************************************************************/
void setup(){
  #if IS_SCT000 == 1
    emon1.current(SCT_PIN, 111.1); /** Corrente: input pin, calibration. [111.1->Burden:18ohm // 60.6->burden:33ohm] **/
  #endif
  
  pinMode(SEND_GREEN, OUTPUT);
  pinMode(SENS_ERR_BLU, OUTPUT);
  pinMode(PIR_LED, OUTPUT);
  pinMode(INC_LED, OUTPUT);
  pinMode(PIR_PIN, INPUT);
  pinMode(INCEND_PIN, INPUT);
  Serial.begin(9600);
  xbee.setSerial(Serial);
  
  flashLed(13, 10, 50);
  flashLed(12, 10, 50);
  flashLed(11, 10, 50);
  
  #if IS_PRES_IR == 1
    calibraPIR();
  #endif
  
  #if IS_T2_BMP == 1
    if(!bmp.begin(3)) // 3:Ultra High Resolution
      /** Pisca led azul em caso de erro do BMP **/
      flashLed(SENS_ERR_BLU, 5, ERROR_TIME);
  #endif
}

/**************************************************************************/
/************************ LOOP ********************************************/
/**************************************************************************/
void loop(){
  
  contador++;   /** Contador para realizar a media dos valores lidos **/
  
  #if IS_DHT11 == 1
    /** ret: OK=0; ERROR_CHECKSUM=1, ERROR_TIMEOUT=2 **/
    int ret = temps.readDHT(DHT11PIN);
    /** Pisca ret vezes o led em caso de erro no DHT **/
    //if(ret > 0)
    //  flashLed(SENS_ERR_BLU, ret, ERROR_TIME);
    data_f[0] += temps.temperature;
    data_f[1] += temps.humidity;  
    data_f[2] += temps.dewPoint(temps.temperature, temps.humidity);
  #endif
  #if DEBUG_MODE == 1 
    Serial.print("\nRetorno DHT: "); Serial.println(ret);
  #endif
  
  /* pega temp2 do DS/BMP/LM   */
  #if IS_T2_DS == 1
    data_f[3] += temps.readDS(DS18B20_Pin); 
  #endif 
  #if IS_T2_LM == 1  
    data_f[3] += analogRead(LM35_PIN)*CELSIUS_BASE; 
  #endif 
  
  #if IS_MQ2 == 1 //  para gas
    data_f[4] += map(analogRead(MQ2_PIN), 0, 1023, 0, 100); 
  #endif 
  
  #if IS_PRES_IR == 1 // para presenca
    data_f[5] += digitalPinTest(PIR_PIN, INC_LED); 
  #endif  
  
  #if IS_INC_IR == 1 // Incendio por IR
    data_f[6] += digitalPinTest(INCEND_PIN, PIR_LED); 
  #endif 
  
  #if IS_T2_BMP == 1 
    data_f[3] += bmp.readTemperature(); 
    data_f[7] += bmp.readPressure();
    data_f[8] += bmp.readSealevelPressure();
    data_f[9] += bmp.readAltitude();
    data_f[10]+= bmp.readAltitude(101500);
  #endif
  
  #if IS_SCT000 == 1
  double Watts = emon1.calcIrms(1480)*220;
    #if DEBUG_MODE == 1 
      Serial.print("Potencia:");
      Serial.print(Watts);   // Apparent power
      Serial.print("W \t Corrente: ");
      Serial.println(Watts/220); // Corrente RMS   Mean Square
    #endif
    data_f[11] += Watts; /* consumo */
  #endif   
    
  #if IS_NOISE == 1
    data_f[12] += -12; /* ruido */
  #endif
  
  /* Configure DEBUG_MODE para enviar dados sem sensores */
  #if NO_SENSORS == 1
    for (int i = 0; i < QTD_DADOS-2; i++) /* (-2) pois ultimos lugares nao sao sensores*/
      data_f[i] += i;
  #endif    
  
  /** condicao que evita a espera ocupada do delay(); e permite calcular as medias **/
  /** subtrai o tempo anterior do atual, e so envia apos o tempo dado em ms (DELAY)*/
  unsigned long tempoAtual = millis();
  if((tempoAtual - tempoAnterior) > DELAY) { 
    tempoAnterior = millis();  
    data_f[14] = contador; // armazena apenas o ultimo valor do contador
    //Serial.print(bmp.readTemperature());Serial.print(" -> ");  Serial.print(bmp.readPressure());Serial.print(" -> ");  Serial.print(bmp.readSealevelPressure());Serial.print(" -> ");  Serial.print(bmp.readAltitude());Serial.print(" -> ");  Serial.println(bmp.readAltitude(101500));*/
    
    #if ID_MQ2 == 1
    if (data_f[4] > 70) /* se gas passa de xx%, entao alarme*/
      flashLed(INC_LED, 3, 50);
    #endif
    
    /** Tira a media ao transformar floats em bytes e carrega no payload **/
    switchToPayload();
    
    /*****************************************/
    /***  Envia pacote e verifica pelo LED ***/
    /*****************************************/
    int ret = sendXBee();
    
    if(ret > 0)
      flashLed(ERROR_RED, ret, ERROR_TIME);
    else
      flashLed(SEND_GREEN, 3, 50);
      
    /** Zera os dados, pra garantir :) **/
    contador = 0;
    for (int i = 0; i < QTD_DADOS; i++)
      data_f[i] = 0.00f;
  }
}

/**************************************************************************/
/** Carrega os dados lidos dos sensores:                                 **/
/** transforma floats em bytes e carrega no 'payload'                    **/
/**************************************************************************/
void switchToPayload(){
  
  int it = 0;
  for (int i = 0; i < QTD_DADOS; i++){
    //if (!isnan(data_f[i]))    // Verifica se o valor eh um numero, nao usamos mais
    u.fval = data_f[i] / contador; /** Faz a media dos valores lidos **/
    if(i == (QTD_DADOS - 2)) u.fval = (float) (error3*100 + error2*10 + error1); /** Envia os valores de erro concatenados tipo: 321 **/
    if(i == (QTD_DADOS - 1)) u.fval = (float) contador;
    for (int j = 0; j < 4; j++){
      payload[j + it] = u.b[j];
    }
    it += 4; /* incrementa de 4 em 4 */
  }
}
    


/***************************************************************************/
/** Envia o zbTx ja carregado com o payload alocado globalmente           **/
/***************************************************************************/
int sendXBee(){
  
  xbee.send(zbTx); /* ENVIANDO PACOTE */

  /** Depois de enviar uma requisicao de TX,  **/
  /** deve-se esperar o status da resposta, espere por 500ms **/
  if (xbee.readPacket(TIMEOUT)) {
    /** Tivemos resposta! Deve ser o status do znet TX    **/        
    if (xbee.getResponse().getApiId() == ZB_TX_STATUS_RESPONSE) {
      xbee.getResponse().getZBTxStatusResponse(txStatus);
      /** Pega o status da entrega, ie, o 5o byte **/
      if (txStatus.getDeliveryStatus() == SUCCESS){
        /** sucesso! vamos celebrar **/
        return 0;
      }
      else{
        error3++;
        if(error3 > ERR3_MAX)
          asm volatile ("  jmp 0");//resetFunc();
        /** O xbee remoto nao recebeu nosso pacote, ele esta OK? 
        * Possivel problema com timeout. **/
        return 3;
      }
    }
  }
  else if (xbee.getResponse().isError()) {
    #if DEBUG_MODE == 1
      Serial.print("Erro ao ler pacote.  codigo: ");  
      Serial.println(xbee.getResponse().getErrorCode());
    #endif 
    
    error2++;
    if(error2 > ERR2_MAX)
      asm volatile ("  jmp 0");//resetFunc();
    return 2;
  } 
  else{ 
    error1++;
    if(error1 > ERR1_MAX)
      asm volatile ("  jmp 0");//resetFunc(); // comentar para nodo 04
    /** O Xbee local nao recebeu o status TX em tempo habil
    -> Nao deveria acontecer **/
    return 1; 
  }
}

/***************************************************************************/
/** Responsavel verificar os valores de entradas digitais: **/
/** como os sensores de presenca e incendio **/
/***************************************************************************/
bool digitalPinTest(const int pin, const int led){
  if (digitalRead(pin) == HIGH) {
    flashLed(led, 4, 50);
    return true;
  }
  else
    digitalWrite(led, LOW);
  return false;
}

/***************************************************************************/
/** Responsavel por piscar o LED,                                         **/
/** --> @TODO: dar um jeito de remover o delay()                          **/
/***************************************************************************/
void flashLed(int pin, int times, int wait) {
  for (int i = 0; i < times; i++) {
    digitalWrite(pin, HIGH);
    delay(wait);
    digitalWrite(pin, LOW);
    if (i + 1 < times)
      delay(wait);
  }
}

/************************************************************************/
/** Precisamos esperar um minuto para o sensor ser calibrado          **/
/** Saia da frente do sensor durante esse tempo                      **/
/** Pisca LED enquanto calibra                                      **/
/********************************************************************/
void calibraPIR(){
  for (unsigned int i=0; i < CALIBRA_PIR; i+=1000) {
    digitalWrite(PIR_LED, HIGH);
    delay(500);
    digitalWrite(PIR_LED, LOW);
    delay(500);
  } 
}

/***** EOF *****/
