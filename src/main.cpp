#include <Arduino.h>
#include <WiFi.h>
#include <LiquidCrystal_I2C.h>
#include <AccelStepper.h>
#include <Adafruit_MAX31865.h>

float soilWeight = 0.0;
float foodWeight = 0.0;
float soilLevel = 0.0;
float foodLevel = 0.0;



#define WIFI_SSID "TP-Link_DADB"
#define WIFI_PASSWORD "24446666678"

// #define WIFI_SSID "yourWifiSsid"
// #define WIFI_PASSWORD "yourWifiPassword"

/* -----------------TEMPERATURE SENSOR CONTROLLER------------------------- */
#define THERMOMETER_CS_PIN 5
#define RREF 430.0     // Reference resistor value in ohms
#define RNOMINAL 100.0 // Nominal resistance of PT100 at 0°C in ohms

Adafruit_MAX31865 thermometer = Adafruit_MAX31865(THERMOMETER_CS_PIN);

void setupThermometer(){
  thermometer.begin(MAX31865_3WIRE);
}

float getTemperature()
{
    // Read raw RTD measurement
    uint16_t rtd = thermometer.readRTD();

    // Calculate resistance
    float ratio = static_cast<float>(rtd) / 32768.0f;
    float resistance = RREF * ratio;

    // Check sensor faults
    uint8_t fault = thermometer.readFault();

    if (fault)
    {
        Serial.printf("[PT100] FAULT: 0x%02X\n", fault);

        if (fault & MAX31865_FAULT_HIGHTHRESH)
            Serial.println("[PT100] RTD High Threshold");

        if (fault & MAX31865_FAULT_LOWTHRESH)
            Serial.println("[PT100] RTD Low Threshold");

        if (fault & MAX31865_FAULT_REFINLOW)
            Serial.println("[PT100] REFIN- > 0.85 x Bias");

        if (fault & MAX31865_FAULT_REFINHIGH)
            Serial.println("[PT100] REFIN- < 0.85 x Bias - FORCE- open");

        if (fault & MAX31865_FAULT_RTDINLOW)
            Serial.println("[PT100] RTDIN- < 0.85 x Bias - FORCE- open");

        if (fault & MAX31865_FAULT_OVUV)
            Serial.println("[PT100] Under/Over voltage");

        thermometer.clearFault();

        return NAN;
    }

    // Calculate temperature
    float temperature = thermometer.temperature(RNOMINAL, RREF);

    // Print diagnostics
    // Serial.printf(
    //     "[PT100] RTD: %u | RATIO: %.5f | RESISTANCE: %.2f | TEMPERATURE: %.2f C\n",
    //     static_cast<unsigned int>(rtd),
    //     ratio,
    //     resistance,
    //     temperature
    // );

    return temperature;
}

void thermometerController()
{
  // static unsigned long lastReadTime = 0;
  // unsigned long currentTime = millis();

  // if (currentTime - lastReadTime >= 1000)
  // {
  //   lastReadTime = currentTime;

  //   float temperature = getTemperature();
  //   Serial.printf("Current Temperature: %.2f °C\n", temperature);
  // }
}


/* -----------------STEPPER MOTOR CONTROLLER------------------------- */
// ESP32 GPIO PINS USED TO CONTROL THE STEPPER MOTOR DRIVERS FOR SENSOR, MIXER, AND GATE STEPPERS MOTORS
#define SENSOR_STEPPER_PULSE_PIN 32
#define SENSOR_STEPPER_DIR_PIN 33
#define MIXER_STEPPER_PULSE_PIN 26
#define MIXER_STEPPER_DIR_PIN 27
#define GATE_STEPPER_PULSE_PIN 12
#define GATE_STEPPER_DIR_PIN 14

#define SENSOR_STEPPER_MAX_SPEED 500
#define SENSOR_STEPPER_ACCELERATION 100
#define GATE_STEPPER_MAX_SPEED 1000
#define GATE_STEPPER_ACCELERATION 500
#define MIXER_STEPPER_MAX_SPEED 100 // DITO BABAGUHIN BILIS NG MIXER
#define MIXER_STEPPER_ACCELERATION 50
#define MIXER_STEPPER_HOMING_SPEED 50
#define MIXER_STEPPER_HOMING_ACCELERATION 10

#define SENSOR_STEPPER_MAX_STEPS 24000
#define GATE_STEPPER_MAX_STEPS 8000
#define MIXER_STEPPER_MIXING_STEPS 5000    // DITO MABABAGO GAANO KATAGAL MAGMIX YUNG MIXER
#define MIXER_STEPPER_DROPPING_STEPS 5000 // DITO MABABAGO GAANO KATAGAL IIKOT YUNG MIXER PARA MAG DROP NG LAMAN

AccelStepper sensorStepper(AccelStepper::DRIVER, SENSOR_STEPPER_PULSE_PIN, SENSOR_STEPPER_DIR_PIN);
AccelStepper mixerStepper(AccelStepper::DRIVER, MIXER_STEPPER_PULSE_PIN, MIXER_STEPPER_DIR_PIN);
AccelStepper gateStepper(AccelStepper::DRIVER, GATE_STEPPER_PULSE_PIN, GATE_STEPPER_DIR_PIN);


void setupStepperMotors() {
  sensorStepper.setMaxSpeed(SENSOR_STEPPER_MAX_SPEED);
  sensorStepper.setAcceleration(SENSOR_STEPPER_ACCELERATION);
  sensorStepper.setCurrentPosition(0);

  mixerStepper.setMaxSpeed(MIXER_STEPPER_MAX_SPEED);
  mixerStepper.setAcceleration(MIXER_STEPPER_ACCELERATION);
  mixerStepper.setCurrentPosition(0);

  gateStepper.setMaxSpeed(GATE_STEPPER_MAX_SPEED);
  gateStepper.setAcceleration(GATE_STEPPER_ACCELERATION);
  gateStepper.setCurrentPosition(0);

  Serial.println("Steppers setup complete.");
}

void loopStepperMotors(){
  sensorStepper.run();
  mixerStepper.run();
  gateStepper.run();
}



/* -----------------MACHINE FEEDBACK CONTROLLER------------------------- */

#define MIXER_STEPPER_HOME_PIN 13 // NOT YET WORKING
#define GATE_STEPPER_HOME_PIN 25
#define SENSOR_STEPPER_HOME_PIN 34
#define SOIL_COVER_LIMIT_SWITCH_PIN 36 // VP
#define FOOD_COVER_LIMIT_SWITCH_PIN 39 // VN

void setupMachineFeedback() {
  pinMode(FOOD_COVER_LIMIT_SWITCH_PIN, INPUT);
  pinMode(SOIL_COVER_LIMIT_SWITCH_PIN, INPUT);
  pinMode(GATE_STEPPER_HOME_PIN, INPUT);
  pinMode(SENSOR_STEPPER_HOME_PIN, INPUT);
  Serial.println("Machine feedback setup complete.");

  pinMode(MIXER_STEPPER_HOME_PIN, INPUT);
}

bool isFoodCoverClose() {
  return digitalRead(FOOD_COVER_LIMIT_SWITCH_PIN) == HIGH;
}

bool isSoilCoverClose() {
  return digitalRead(SOIL_COVER_LIMIT_SWITCH_PIN) == HIGH;
}

bool isGateStepperHome() {
  return digitalRead(GATE_STEPPER_HOME_PIN) == HIGH;
}

bool isMixerStepperHome() {
  return digitalRead(MIXER_STEPPER_HOME_PIN) == HIGH;
}

bool isSensorStepperHome() {
  return digitalRead(SENSOR_STEPPER_HOME_PIN) == HIGH;
}

/* -----------------SERIAL MONITOR CONTROLLER------------------------- */

const unsigned long printInterval = 1000; // Print every 1 second
void serialMonitorController(){
  
  static unsigned long lastPrintTime = 0; // Store the last time we printed to the serial monitor
  unsigned long currentTime = millis();

  if (currentTime - lastPrintTime >= printInterval) {
    lastPrintTime = currentTime;
    Serial.printf("%u \t| FC:%d | SC:%d | MH:%d | GH:%d | SH:%d || TEMP: %.2f\n", 
      currentTime, 
      isFoodCoverClose(),
      isSoilCoverClose(),
      isMixerStepperHome(),
      isGateStepperHome(),
      isSensorStepperHome(), 
      getTemperature()
    );
  }
}


/* -----------------LCD FEATURES------------------------- */
#define LCD_I2C_ADDRESS 0x27
#define LCD_COLUMNS 20
#define LCD_ROWS 4
LiquidCrystal_I2C lcd(LCD_I2C_ADDRESS, LCD_COLUMNS, LCD_ROWS);

/**
 * @brief Updates the LCD with food and soil hopper levels.
 * @details Displays three-character integer percentages without decimals.
 * @note Values are padded with leading spaces instead of zeros.
 */
void lcdUpdateHopperLevel(float foodLevel, float soilLevel)
{
    static bool firstRun = false;

    if (!firstRun)
    {
        lcd.setCursor(0, 1);
        lcd.print("FOOD:   % SOIL:   %");
        firstRun = true;
    }

    lcd.setCursor(5, 1);
    lcd.printf("%3d", (int)foodLevel);

    lcd.setCursor(15, 1);
    lcd.printf("%3d", (int)soilLevel);
}

/**
 * @brief Updates LCD with food and soil weight readings.
 * @details Displays four-character integer weights in grams.
 * @note Values are padded with leading spaces instead of zeros.
 */
void lcdUpdateWeight(float foodWeight, float soilWeight)
{
    static bool firstRun = false;

    if (!firstRun)
    {
        lcd.setCursor(0, 2);
        lcd.print("FOOD:0000 SOIL:0000");
        firstRun = true;
    }

    lcd.setCursor(5, 2);
    lcd.printf("%4d", (int)foodWeight);

    lcd.setCursor(15, 2);
    lcd.printf("%4d", (int)soilWeight);
}

/**
 * @brief Updates the LCD with the current machine feedback status on the last line.
 *
 * @details Displays the actual status of the food cover, soil cover,
 * mixer stepper, gate stepper, and sensor stepper.
 *
 * @note '1' = Active/Closed/Homed, '0' = Inactive/Open/Not Homed.
 */
void lcdUpdateFeedback() {
    char lcdFeedback[21];

    snprintf(
        lcdFeedback,
        sizeof(lcdFeedback),
        "FC%c SC%c MH%c GH%c SH%c",
        isFoodCoverClose() ? '1' : '0',
        isSoilCoverClose() ? '1' : '0',
        isMixerStepperHome() ? '1' : '0',
        isGateStepperHome() ? '1' : '0',
        isSensorStepperHome() ? '1' : '0'
    );

    lcd.setCursor(0, 3);
    lcd.print(lcdFeedback);
}

void lcdController()
{
    static unsigned long lastUpdateTime = 0;
    constexpr unsigned long UPDATE_INTERVAL = 500;

    if (millis() - lastUpdateTime < UPDATE_INTERVAL)
        return;

    lastUpdateTime = millis();

    // Update machine feedback
    lcdUpdateFeedback();

    // Update hopper levels
    lcdUpdateHopperLevel(foodLevel, soilLevel);

    // Update weight percentages
    lcdUpdateWeight(foodWeight, soilWeight);
}

// SETUP
void setup() {
  Serial.begin(115200);

  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("ECOCYCLE");

  setupMachineFeedback();
  setupStepperMotors();
  setupThermometer();
}

void stepperMotorController(){
  loopStepperMotors();
}

void stateController(){

}

void machineController(){
  thermometerController();
  stepperMotorController();
  lcdController();



}

void loop() {

  serialMonitorController();
  stateController();
  machineController();


  
  // put your main code here, to run repeatedly:
}

