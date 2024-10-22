#include <ESP8266WiFi.h> 
#include <PubSubClient.h>
#include <NTPClient.h>//Biblioteca do NTP.
#include <WiFiUdp.h>//Biblioteca do UDP.

//WiFi
const char* SSID = "...";                // SSID / nome da rede WiFi que deseja se conectar
const char* PASSWORD = "...";   // Senha da rede WiFi que deseja se conectar
WiFiClient wifiClient;                        
 
//MQTT Server
const char* BROKER_MQTT = "test.mosquitto.org"; //URL do broker MQTT que se deseja utilizar
int BROKER_PORT = 1883;                      // Porta do Broker MQTT

#define ID_MQTT  "Sensor_caixa_de_agua_do_Lameu_esp8266" //Informe um ID unico e seu. Caso sejam usados IDs repetidos a ultima conexão irá sobrepor a anterior. 
#define TOPIC_HISTORY "water/tank/level/history"    //Tópico para os dados históricos semanais
#define TOPIC_INSTANT "water/tank/level/instant"    //Tópico para os dados instantâneos
PubSubClient MQTT(wifiClient);        // Instancia o Cliente MQTT passando o objeto espClient

String dados;
char nivel[4];
int nMedidas = 0; //Número de medidas realizadas

//Conexão do sensor
#define echoPin 0
#define trigPin 2

long duration; //tempo que q onda trafega
int distance; //distância medida

//Obter hora na internet
WiFiUDP udp;//Cria um objeto "UDP".
NTPClient ntp(udp, "a.st1.ntp.br", -3 * 3600, 60000);//Cria um objeto "NTP" com as configurações.

unsigned long tempo;//Váriavel que armazenara os segundos desde de 1970

//Declaração das Funções
void mantemConexoes();  //Garante que as conexoes com WiFi e MQTT Broker se mantenham ativas
void conectaWiFi();     //Faz conexão com WiFi
void conectaMQTT();     //Faz conexão com Broker MQTT
void enviaValores();     //

void medeDistancia();

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  Serial.begin(115200);

  conectaWiFi();
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);

  ntp.begin();//Inicia o NTP.
  ntp.forceUpdate();//Força o Update.   
}

void loop() {
  
  mantemConexoes();
  
  MQTT.loop();

  //Lógica principal, tranformar em fução mais tarde
  tempo = ntp.getEpochTime();

  //Atulaliza os dados
  medeDistancia();
  int level = 100 - distance; //Sensor posicionado a 1 metro (100cm) do fundo do reservatório
  Serial.println("nível: "+ level);
  itoa(level, nivel, 10); //Converte int para char* para envio via MQTT
  MQTT.publish(TOPIC_INSTANT, nivel); //envia apenas o nível atual

  if (tempo % 1800 == 0) { //A cada 1800 segundos ou meia hora   

    //Gera o Array de Objetos JSON
    if (nMedidas == 0) {
      dados = String("[{\"time\":") + tempo + ",\"level\":" + level + "}]"; //Primeiro objeto JSON no Array
    } else if (nMedidas < 336){ //Do segundo em diante até 336
      nMedidas++;
      dados = dados.substring(0, dados.length()-1) + ",{\"time\":" + tempo + ",\"level\":" + level + "}]"; //Remove o ']' e add mais um objeto
    } else { //Depois do 366...
      int primeiraVirgula = dados.indexOf(',');
      int segundaVirgula = dados.indexOf(',',primeiraVirgula); //Localiza o índice do separador entre o primeiro e segundo objeto
      dados = String("[") + dados.substring(segundaVirgula + 1, dados.length()-1) + ",{\"time\":" + tempo + ",\"level\":" + level + "}]"; //Remove o objeto mais antigo e add um novo a lista
    }
    enviaValores(); //Envia o Array de objetos
  }

  delay(1000);//Espera 1 segundo
  
}

void mantemConexoes() {
    if (!MQTT.connected()) {
       conectaMQTT(); 
    }
    
    conectaWiFi(); //se não há conexão com o WiFI, a conexão é refeita
}

void conectaWiFi() {

  if (WiFi.status() == WL_CONNECTED) {
     return;
  }
        
  Serial.print("Conectando-se na rede: ");
  Serial.print(SSID);
  Serial.println("  Aguarde!");

  WiFi.mode(WIFI_STA); //Add pelo exemplo do NTPClient, configura o esp como estação
  WiFi.begin(SSID, PASSWORD); // Conecta na rede WI-FI  
  while (WiFi.status() != WL_CONNECTED) {
      delay(100);
      Serial.print(".");
  }
  
  Serial.println();
  Serial.print("Conectado com sucesso, na rede: ");
  Serial.print(SSID);  
  Serial.print("  IP obtido: ");
  Serial.println(WiFi.localIP()); 
}

void conectaMQTT() { 
    while (!MQTT.connected()) {
        Serial.print("Conectando ao Broker MQTT: ");
        Serial.println(BROKER_MQTT);
        if (MQTT.connect(ID_MQTT)) {
            Serial.println("Conectado ao Broker com sucesso!");
        } 
        else {
            Serial.println("Não foi possivel se conectar ao broker.");
            Serial.println("Nova tentatica de conexao em 10s");
            delay(10000);
        }
    }
}

void enviaValores() {
      
  MQTT.publish(TOPIC_HISTORY, (uint8_t*)dados.c_str(), dados.length(), true); //Envia os dados com a flag retain habilitada (true)
  Serial.println(dados);        

}

void medeDistancia() {

  digitalWrite(trigPin, LOW);//Limpa a condição do trigPin
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);//Seta em nível alto por 10 microsegundos
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  duration = pulseIn(echoPin, HIGH); //Retorna o tempo que a onda trafegou
  distance = duration * 0.034 / 2; //* Velocidade do som dividida por 2 (ida e volta)
}

