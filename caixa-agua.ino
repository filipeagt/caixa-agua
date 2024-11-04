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
#define TOPIC_PUB_SUB "water/tank/level"    //Tópico único
PubSubClient MQTT(wifiClient);        // Instancia o Cliente MQTT passando o objeto espClient

String dados;
char nivel[4];
int level = 0;
int nMedidas = 0; //Número de medidas realizadas
bool inicio = true; //Variável auxiliar para capturar os dados apena uma vez na inicialização

//Conexão do sensor
#define echoPin 3 //Conectar por um resistor de 1k para limitar a corrente no boot, ao ligar o pino3 inicia em high
#define trigPin 1 //0,1 e 2 se conecatdos a LOW na inicialização da falha no boot, então o esp não iniciliza se ligados no echo

long duration; //tempo que q onda trafega
int distance; //distância medida

//Obter hora na internet
WiFiUDP udp;//Cria um objeto "UDP".
NTPClient ntp(udp, "a.st1.ntp.br", 0 * 3600, 60000);//Cria um objeto "NTP" com as configurações.

unsigned long tempo;//Váriavel que armazenara os segundos desde de 1970
int minutosAnt = 59;  //salva o minuto do último envio de JSON para enviar somente uma vez a a cada meia hora
int minutos = 0; //Armazena o minuto atual 

//Declaração das Funções
void mantemConexoes();  //Garante que as conexoes com WiFi e MQTT Broker se mantenham ativas
void conectaWiFi();     //Faz conexão com WiFi
void conectaMQTT();     //Faz conexão com Broker MQTT
void enviaValores();     //
void recebePacote(char* topic, byte* payload, unsigned int length);
void medeDistancia();
void gerencia();
void medeNivel();

void setup() {
  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  Serial.begin(115200);

  conectaWiFi();
  MQTT.setServer(BROKER_MQTT, BROKER_PORT);
  MQTT.setBufferSize(16384); //Para enviar mensagens maiores
  MQTT.setCallback(recebePacote);

  ntp.begin();//Inicia o NTP.
  ntp.forceUpdate();//Força o Update.   
}

void loop() {
  
  mantemConexoes();
  
  MQTT.loop();

  if (!inicio) { //Só é executado depois de ter recebibido um pacote 
    gerencia();
  } else {
    medeNivel(); //Para garantir que vai ter pelo menos um dado no tópico
  }

  delay(5000);//Espera 5 segundos
  
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
            MQTT.subscribe(TOPIC_PUB_SUB);
        } 
        else {
            Serial.println("Não foi possivel se conectar ao broker.");
            Serial.println("Nova tentatica de conexao em 10s");
            delay(10000);
        }
    }
}

void enviaValores() {
      
  MQTT.publish(TOPIC_PUB_SUB, (uint8_t*)dados.c_str(), dados.length(), true); //Envia os dados com a flag retain habilitada (true)
  //Serial.println(dados);        

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

void gerencia() {
  //Atulaliza os dados
  tempo = ntp.getEpochTime();
  minutos = ntp.getMinutes();
  
  medeNivel();

  if (minutos % 30 == 0 && minutos != minutosAnt) { //A cada meia hora envia os dados
    minutosAnt = minutos;
    //Gera o Array de Objetos JSON
    if (nMedidas == 0) {
      nMedidas++;
      dados = String("[{\"time\":") + tempo + ",\"level\":" + level + "}]"; //Primeiro objeto JSON no Array
    } else if (nMedidas < 336){ //Do segundo em diante até 336
      nMedidas++;
      dados = dados.substring(0, dados.length()-1) + ",{\"time\":" + tempo + ",\"level\":" + level + "}]"; //Remove o ']' e add mais um objeto
    } else { //Depois do 366...
      int fechaChave = dados.indexOf('}'); //Localiza o índice do fechamento do primeiro objeto
      dados = String("[") + dados.substring(fechaChave + 2, dados.length()-1) + ",{\"time\":" + tempo + ",\"level\":" + level + "}]"; //Remove o objeto mais antigo e add um novo a lista
    }
    delay(250); //Tempo para estabilizar os dados na memória
    enviaValores(); //Envia o Array de objetos
    delay(250);
  }
}

void recebePacote(char* topic, byte* payload, unsigned int length) 
{  
  if ((char)payload[0] == '[' && inicio && (char)payload[length-1] == ']') { //Se o pacote for um Array e estiver na inicilização do esp    
    for(int i = 0; i < length; i++) //obtem a string do payload recebido
    {
       char c = (char)payload[i];
       dados += c;
       if (c == '}') { //Conta o número de objetos no Array
         nMedidas++;
       }
    }
    
  } 
  inicio = false; //Seta inicio para false para garantir que não vai mais entrar neste loop 
  Serial.println(dados);
  MQTT.unsubscribe(TOPIC_PUB_SUB);
}

void medeNivel() {
  medeDistancia();
  level = 100 - distance; //Sensor posicionado a 1 metro (100cm) do fundo do reservatório
  //Serial.println("nível: "+ level);
  itoa(level, nivel, 10); //Converte int para char* para envio via MQTT
  MQTT.publish(TOPIC_PUB_SUB, nivel); //envia apenas o nível atual
}
