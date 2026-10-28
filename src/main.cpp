#include <Arduino.h>
#include <Wire.h>

// Teste para identificar qual MPU voce realmente tem (versao 2).
// SDA=GPIO21, SCL=GPIO22

static const int SDA_PIN = 21;
static const int SCL_PIN = 22;
static const uint8_t AK8963_ADDR = 0x0C;

// Gera 9 pulsos em SCL para liberar um SDA travado
void i2cRecover() {
  pinMode(SDA_PIN, INPUT_PULLUP);
  pinMode(SCL_PIN, OUTPUT);
  for (int i = 0; i < 9; i++) {
    digitalWrite(SCL_PIN, HIGH); delayMicroseconds(5);
    digitalWrite(SCL_PIN, LOW);  delayMicroseconds(5);
  }
  digitalWrite(SCL_PIN, HIGH);
  delayMicroseconds(5);
}

bool devicePresent(uint8_t addr) {
  Wire.beginTransmission(addr);
  return Wire.endTransmission() == 0;
}

// Tenta achar o dispositivo repetidamente ate estourar o tempo limite
bool waitForDevice(uint8_t addr, uint32_t timeout_ms) {
  uint32_t start = millis();
  while (millis() - start < timeout_ms) {
    if (devicePresent(addr)) return true;
    delay(50);
  }
  return false;
}

// Le 1 byte de um registrador. Retorna -1 se houver erro de I2C.
int readReg(uint8_t addr, uint8_t reg) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  if (Wire.endTransmission(false) != 0) return -1;
  if (Wire.requestFrom((int)addr, 1) != 1) return -1;
  return Wire.read();
}

void writeReg(uint8_t addr, uint8_t reg, uint8_t val) {
  Wire.beginTransmission(addr);
  Wire.write(reg);
  Wire.write(val);
  Wire.endTransmission();
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  i2cRecover();
  Wire.begin(SDA_PIN, SCL_PIN);
  Wire.setClock(100000);

  Serial.println("=== Teste de identificacao da MPU (v2) ===");

  // 1) Procura o sensor por ate 2 s em 0x68 e, se nao achar, em 0x69
  uint8_t mpu = 0;
  if (waitForDevice(0x68, 2000)) mpu = 0x68;
  else if (waitForDevice(0x69, 500)) mpu = 0x69;

  if (mpu == 0) {
    Serial.println("Nenhum sensor em 0x68/0x69 (apos 2,5 s de tentativas).");
    return;
  }
  Serial.printf("Sensor encontrado no endereco 0x%02X\n", mpu);

  // 2) WHO_AM_I (registrador 0x75)
  int who = readReg(mpu, 0x75);
  Serial.printf("WHO_AM_I = 0x%02X -> ", who);
  switch (who) {
    case 0x68: Serial.println("MPU6050 (sem magnetometro)"); break;
    case 0x70: Serial.println("MPU6500 (sem magnetometro)"); break;
    case 0x71: Serial.println("MPU9250 (com magnetometro AK8963)"); break;
    case 0x73: Serial.println("MPU9255 (com magnetometro)"); break;
    default:   Serial.println("Valor desconhecido (possivel clone)"); break;
  }

  // 3) Acorda o chip e ativa o bypass para enxergar o magnetometro
  writeReg(mpu, 0x6B, 0x00);
  delay(100);
  writeReg(mpu, 0x37, 0x02);
  delay(10);

  // 4) Magnetometro AK8963 (WHO_AM_I esperado: 0x48)
  bool mag = waitForDevice(AK8963_ADDR, 500);
  if (mag) {
    int akWho = readReg(AK8963_ADDR, 0x00);
    Serial.printf("Magnetometro em 0x0C, WHO_AM_I = 0x%02X -> %s\n", akWho,
                  akWho == 0x48 ? "AK8963 OK" : "valor inesperado");
  } else {
    Serial.println("Magnetometro NAO encontrado em 0x0C.");
  }

  // 5) Conclusao
  Serial.println("--- Conclusao ---");
  if ((who == 0x71 || who == 0x73) && mag) {
    Serial.println("MPU9250/9255 verdadeira (acel + giro + magnetometro).");
  } else if (!mag) {
    Serial.println("Sem magnetometro: so acel + giro (MPU6050/6500 ou falsa).");
  } else {
    Serial.println("Resultado inconclusivo; veja os valores acima.");
  }
}

void loop() {}