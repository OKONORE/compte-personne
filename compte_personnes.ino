#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <TimeLib.h>

String DATA_FILE = "data.csv";
String HOME_CHEMIN = "main.html";
String MAIN_RESET = "reset.html";
String TEMPS_CHEMIN = "heure.html";
String ENVOI_HEURE = "envoi";

const int CAPTEUR_IR_1 = D1;
const int CAPTEUR_IR_2 = D2;
const int SD_CARD = D4;
const int RESET_PIN = D4;

String SSID_WIFI = "Compte_Personne";
String PASS_WIFI = "12345678";
String MAC;
ESP8266WebServer server(80);

bool Capteur_1 = 0;
bool Capteur_2 = 0;
bool précé_capteur_1 = 0;
bool précé_capteur_2 = 0;

const unsigned long delai = 2000;
unsigned long debut_timer;

File FICHIER_DATA;

String heure_vers_string() {
  return String(year())+"-"+String(month())+"-"+String(day())+"-"+String(hour())+"-"+String(minute())+"-"+String(second());
}

void handle_main() {
  File file = SD.open(HOME_CHEMIN);
  server.streamFile(file, "text/html");
  file.close();
  return;
}

void handle_data() {
  File file = SD.open(DATA_FILE);
  server.streamFile(file, "text/plain");
  file.close();
  return;
}

void handle_reset() {
  File file = SD.open(MAIN_RESET);
  server.streamFile(file, "text/html");
  file.close();
  reset_data();
  return;
}

void handle_synchro_temp() {
  if (server.method() == HTTP_POST) {
    String temps_local = server.arg("local_time");

    struct tm tm;
    if (strptime(temps_local.c_str(), "%Y-%m-%d %H:%M:%S", &tm) != NULL) {
      time_t temps_puce = mktime(&tm);
      Serial.print("Heure de l'appareil mise à jour: ");
      setTime(temps_puce);
      Serial.println(heure_vers_string());
      server.send(200, "text/plain", "Heure synchronisée avec succès");
    } else {
      server.send(400, "text/plain", "Format d'heure invalide");
    }
  } else {
    server.send(405, "text/plain", "Méthode non autorisée");
  }
}

void handle_heure() {
  File file = SD.open(TEMPS_CHEMIN);
  server.streamFile(file, "text/html");
  file.close();
  reset_data();
  return;
}

void setup_wifi_server() {
  Serial.println("\n- Info Réseaux -");
  MAC = WiFi.macAddress();
  Serial.println(MAC);
  
  if (WiFi.softAP(SSID_WIFI, PASS_WIFI)) {
    Serial.print("SSID: "+SSID_WIFI+"\n");
  } else {
    Serial.print("Erreur Serveur: "+SSID_WIFI+"\n");
    return;
  }
  server.on("/", handle_heure);
  server.on("/"+ENVOI_HEURE, handle_synchro_temp);
  server.on("/"+HOME_CHEMIN, handle_main);
  server.on("/"+DATA_FILE, handle_data);
  server.on("/"+MAIN_RESET, handle_reset);
  
  server.begin();

  Serial.println(WiFi.softAPIP());
}

void setup_sd_card() {
  while (!SD.begin(D4)) {
    Serial.println("Echec SD init");
    delay(200);
  }
  
  File Fichier = SD.open("/");
  if (!SD.exists("./")) {
    return;
  }
}

void ajouter_data(String id_salle, int type, String temps) {
    File fichier_data = SD.open(DATA_FILE, FILE_WRITE);
    fichier_data.print(id_salle+";"+String(type)+";"+temps+"\n");
    fichier_data.close();
    return;
}

void reset_data(){
  SD.remove(DATA_FILE);
  File Fichier = SD.open(DATA_FILE, FILE_WRITE);
  Fichier.close();
  Serial.println("Les données ont été reset");
  return;
}

void setup() {
  Serial.begin(9600);
  while (!Serial) {}
  
  setup_sd_card();
  setup_wifi_server();
  Serial.println("Heure de la puce non définie");
  
  pinMode(CAPTEUR_IR_1, INPUT_PULLUP);
  pinMode(CAPTEUR_IR_2, INPUT_PULLUP);

}

void loop() {
  server.handleClient();
  yield();

  Capteur_1 = digitalRead(CAPTEUR_IR_1);
  Capteur_2 = digitalRead(CAPTEUR_IR_2);
  
  if (timeStatus() == 2) {
    if (Capteur_1){
      Serial.println("E1 Activé");
    }
    if (Capteur_2){
      Serial.println("S2 Activé");
    }
    
    if (!précé_capteur_1 && Capteur_1) {
      debut_timer = millis();
      while ((millis() - debut_timer) < delai) {
        Capteur_2 = digitalRead(CAPTEUR_IR_2);
        if (Capteur_2) {
          Serial.println(" -- Entrée détectée");
          ajouter_data(MAC, 1, heure_vers_string());
          delay(200);
          break;
        }
        if (millis() - debut_timer >= delai) {
          Serial.println("Timeout pour entrée");
          break;
        }
      }
    }
  
    if (!précé_capteur_2 && Capteur_2) {
      debut_timer = millis();
      while ((millis() - debut_timer) < delai) {
        Capteur_1 = digitalRead(CAPTEUR_IR_1);
        if (Capteur_1) {
          Serial.println("-- Sortie détectée");
          ajouter_data(MAC, 0, heure_vers_string());
          delay(200);
          break;
        }
        if (millis() - debut_timer >= delai) {
          Serial.println("Timeout pour sortie");
          break;
        }
      }
    }
  

    précé_capteur_1 = Capteur_1;
    précé_capteur_2 = Capteur_2;
  
    delay(100);
  }
}
