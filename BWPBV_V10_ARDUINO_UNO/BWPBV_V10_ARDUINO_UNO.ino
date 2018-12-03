/*
  Projekt: BWPBV
  Beschreibung: Target fuer Schiessstand
  Datum: 15.12.2015
  (ARDUINO UNO)
  ATMEL 328P

  Sensoreingänge PORT-B: PIN: 14, 15, 16, 17
  BlueTooth PIN:5 RX, 6 TX
  Reset: PIN 23
  LED: PIN 24

  Autor: M.HÃ¶ller-Schlieper

  Einstellungen für Programmer:
  ATMEL STK500 development board
  Arduino Uno

  - Kreisschnittpunkt berechnen
  - Korrektur: lineares Prüfen
  - Speichern des InfoStrings im EEProm

  Version 2051:
  10.10.2016 Kennzeichen: "VE"
  28.11.2016 Berechnungsalgorithmus in der APP

  Version 2100
  09.09.2018 Version für make

*/


#include <avr/io.h>
#include <SoftwareSerial.h>

SoftwareSerial BTSerial = SoftwareSerial(9, 10); //10 RX | 11 TX
//======================================
// Konstanten

const int MaxMicros = 4000;

const int Version = 2100;

// Schrittkette
const int skWarteAufSchwelle_Neu = 5;
const int skSendSamples_Neu = 6;

const float c_CLCK = 6.25; // 1/16MHZ = 6,25*10-8

const unsigned long c_TimeOutCounter = 100000;
const int iSensorPlattenBreite = 34;
// IOs
const int Sensor1 = 0;
const int Sensor2 = 1;
const int Sensor3 = 2;
const int Sensor4 = 3;

const int LED_Ready = 13;
const int RESET = 12; // die Schmitt-Trigger zurücksetzen

// Anzahl der Samples
const int c_MaxSamples = 1200;
const int c_notUsed = -1;
//======================================
// Strukturen
struct TSensor {
  double iLaufzeitMykroSec;
  double iLaufzeitMykroSecCalc;
  int iSensorNr;
  double dRadius;
  double dRadiusCalc;
};

//======================================
// Variablen

int Schritt = skWarteAufSchwelle_Neu;
unsigned long Micros = 0;
byte inputs[c_MaxSamples];

TSensor Sensoren[4];

long iSampleIDX = 0;
float iDeltaMS = 0;
long iPlattenID = 0;

double iLaufzeitMykroSec = 0;

//=========================================
//  SendSamples - die Samples an Host senden
//=========================================

void SendSamples_Neu() {
  //================================================
  // Bluetooth, USB, Funk
  //================================================
  if (BTSerial) {

    BTSerial.flush();
    BTSerial.print("KOORD ");
    BTSerial.print(Version);// Versionsnummer
    BTSerial.print(" ");
    BTSerial.print(-1); // Kennung, dass hier die XY-Position nicht berechnet wurde
    BTSerial.print(" ");
    BTSerial.print(iSensorPlattenBreite);
    BTSerial.print(" ");
    BTSerial.print(Sensoren[0].iLaufzeitMykroSec);
    BTSerial.print(" ");
    BTSerial.print(Sensoren[1].iLaufzeitMykroSec);
    BTSerial.print(" ");
    BTSerial.print(Sensoren[2].iLaufzeitMykroSec);
    BTSerial.print(" ");
    BTSerial.print(Sensoren[3].iLaufzeitMykroSec);
    BTSerial.print(" ");
    BTSerial.print(iDeltaMS);
    BTSerial.print(" ");
    BTSerial.print(iSampleIDX);
    BTSerial.print(" ");
    BTSerial.print(iPlattenID);
    BTSerial.print(" ");
    BTSerial.print("I");
    BTSerial.print(" ");
    BTSerial.print("");
    BTSerial.print(" ");
    BTSerial.println("|"); // end of file


  }
}
//=========================================
// Setup
//=========================================
void setup() {
  randomSeed(analogRead(0));
  iPlattenID = random(1000000);

  BTSerial.begin(9600);
  BTSerial.println("connected");

  // setze die Konfiguration an PORT - D
  // FÜR den MEGA an PORT-D: PIN 2, 3, 4, 5  Sensoreingänge
  // nur lesen an Port D
  DDRD = 0b00000000;


  pinMode(LED_Ready, OUTPUT);
  pinMode(RESET, OUTPUT);

  // Ready LED EIN
  digitalWrite(LED_Ready, LOW);
  delay(200);
  digitalWrite(RESET, LOW);
  delay(200);
  digitalWrite(RESET, HIGH);

}

//=========================================
//=========================================
//=========================================
//=========================================
//=========================================
// Hauptprogramm
//=========================================
//=========================================
//=========================================
//=========================================
//=========================================

void loop() {

  //===========================================================
  switch (Schritt) {

    case skWarteAufSchwelle_Neu:

      // leere das Feld
      for (int i = 0; i < c_MaxSamples; i++) {
        inputs[i] = 0;
      }

      iSampleIDX = 0;

      //==================================================
      // solange alle Eingänge 0 sind
      //==================================================
      noInterrupts(); // Turbo einschalten
      while ((PIND & 0x0F)  == 0); // warte wis ein Eingang gesetzt wurde
      TCNT2 = 0;
      Micros = TCNT2; // die Startzeit merken
      do {
        //==================================================
        // lese die Eingänge und schreibe in das Array
        //==================================================
        inputs[iSampleIDX++] = (byte)PIND;
      } while (iSampleIDX < c_MaxSamples);

      iDeltaMS = (TCNT2 - Micros); // die gemessene Zeit
      interrupts();  // Turbo ausschalten und Interrupts ein

      //==================================================
      // die Auflösung berechnen
      //==================================================
      if (iSampleIDX > 0 && iDeltaMS > 0) {

        // die Auflösung berechnen
        // c_CLCK: 1/16MHZ = 6,25*10-8
        iDeltaMS = 10000 * iDeltaMS * c_CLCK / iSampleIDX;    

        //==================================================
        // die Flanken aus dem Array lesen...
        //==================================================

        Sensoren[0].iLaufzeitMykroSec = c_notUsed;
        Sensoren[1].iLaufzeitMykroSec = c_notUsed;
        Sensoren[2].iLaufzeitMykroSec = c_notUsed;
        Sensoren[3].iLaufzeitMykroSec = c_notUsed;

        for (long i = 0; i < iSampleIDX; i++) {

          iLaufzeitMykroSec = i * iDeltaMS; // Umrechnen der Auflösung des Timers in Millisekunden (1/16MHZ=0.0625µs)

          if (bitRead(inputs[i], 0) && Sensoren[0].iLaufzeitMykroSec == c_notUsed) {
            Sensoren[0].iLaufzeitMykroSec = iLaufzeitMykroSec;
          }

          if (bitRead(inputs[i], 1) && Sensoren[1].iLaufzeitMykroSec == c_notUsed) {
            Sensoren[1].iLaufzeitMykroSec = iLaufzeitMykroSec;
          }

          if (bitRead(inputs[i], 2) && Sensoren[2].iLaufzeitMykroSec == c_notUsed) {
            Sensoren[2].iLaufzeitMykroSec = iLaufzeitMykroSec;
          }

          if (bitRead(inputs[i], 3) && Sensoren[3].iLaufzeitMykroSec == c_notUsed) {
            Sensoren[3].iLaufzeitMykroSec = iLaufzeitMykroSec;
          }
        }

        // berechne die XY-Position
        Schritt = skSendSamples_Neu;

      }

      break;
    //=========================================
    case skSendSamples_Neu:
      // READY LED EIN
      digitalWrite(LED_Ready, HIGH);
      // entprelle das System
      // die Sensoren zurücksetzen
      digitalWrite(RESET, LOW);

      // Sampels senden
      SendSamples_Neu();
      delay(50);

      // warte bis die Eingänge wirklich zurückgesetzt sind
      while ((PIND & 0x0F)  != 0);

      delay(20);
      digitalWrite(RESET, HIGH);
      digitalWrite(LED_Ready, LOW);

      // warte auf Schwelle
      Schritt = skWarteAufSchwelle_Neu;
      break;

  }
}

