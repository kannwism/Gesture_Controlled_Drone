char cmd;
// ================================================================
// ===                      key                        ===
// ================================================================
#define interruptPin7 7 //c.23
#define PIO_mask7 (1 << 23)

#define interruptPin6 6 //c.24
#define PIO_mask6 (1 << 24)

#define interruptPin5 5 //c.25
#define PIO_mask5 (1 << 25)

#define interruptPin4 4 //c.26
#define PIO_mask4 (1 << 26)

char btn1_flag=0;
char btn2_flag=0;
char btn3_flag=0;
char btn4_flag=0;

// connect the key to digital pin 9,8,7,6
void key_setup()
{

  REG_PIOC_WPMR = 0x50494F00;
  REG_PIOC_PER |= PIO_mask7|PIO_mask6|PIO_mask5|PIO_mask4;
  REG_PIOC_MDDR |= PIO_mask7|PIO_mask6|PIO_mask5|PIO_mask4;
  REG_PIOC_PUER |= PIO_mask7|PIO_mask6|PIO_mask5|PIO_mask4;
  REG_PIOC_DIFSR |= PIO_mask7|PIO_mask6|PIO_mask5|PIO_mask4;
  REG_PIOC_SCDR = 1;
  REG_PIOC_IFER |= PIO_mask7|PIO_mask6|PIO_mask5|PIO_mask4;

  NVIC_SetPriority(PIOC_IRQn,15);

  attachInterrupt(digitalPinToInterrupt(interruptPin4), button1_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(interruptPin5), button2_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(interruptPin6), button3_ISR, FALLING);
  attachInterrupt(digitalPinToInterrupt(interruptPin7), button4_ISR, FALLING);
}


void button1_ISR() {
  btn1_flag=1;
//  Serial.println("4 pres");
}
void button2_ISR() {
  btn2_flag=1;
//  Serial.println("5");
}
void button3_ISR() {
  btn3_flag=1;
//  Serial.println("6");
}
void button4_ISR() {
  btn4_flag=1;
//  Serial.println("7");
}


// ================================================================
// ===                      FLEX SENSORS                        ===
// ================================================================
#include "controller.h"
#define A0 54 //#define A0 14
#define A1 55 //#define A1 15
#define A2 56 //#define A2 16

Controller control(A0, A1, A2);

// ================================================================
// ===                      ACCELEROMETER                       ===
// ================================================================
//DMP
#include "I2Cdev.h"
#include "MPU6050_6Axis_MotionApps20.h"
#if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
    #include "Wire.h"
#endif

#define INTERRUPT_PIN 2  // use pin 2 on Arduino Uno & most boards

MPU6050 mpu;

//enum{THROTTLE,YAW,PITCH,ROLL};
//uint16_t rcData[4]={1500,1500,1500,1500};

unsigned long counter = 0; //number that gets send

// MPU control/status vars
bool dmpReady = false;  // set true if DMP init was successful
uint8_t mpuIntStatus;   // holds actual interrupt status byte from MPU
uint8_t devStatus;      // return status after each device operation (0 = success, !0 = error)
uint16_t packetSize;    // expected DMP packet size (default is 42 bytes)
uint16_t fifoCount;     // count of all bytes currently in FIFO
uint8_t fifoBuffer[64]; // FIFO storage buffer

// orientation/motion vars
Quaternion q;           // [w, x, y, z]         quaternion container
VectorFloat gravity;    // [x, y, z]            gravity vector
float ypr[3];           // [yaw, pitch, roll]   yaw/pitch/roll container and gravity vector

volatile bool mpuInterrupt = false;     // indicates whether MPU interrupt pin has gone high
void dmpDataReady() {
    mpuInterrupt = true;
    Serial.println("dma interrupt");
}

void DMPinit() {
  // join I2C bus (I2Cdev library doesn't do this automatically)
    #if I2CDEV_IMPLEMENTATION == I2CDEV_ARDUINO_WIRE
        Wire.begin();
        //Wire.setClock(200000); // 400kHz I2C clock. Comment this line if having compilation difficulties
    #elif I2CDEV_IMPLEMENTATION == I2CDEV_BUILTIN_FASTWIRE
        Fastwire::setup(400, true);
    #endif

    mpu.initialize();
//    pinMode(INTERRUPT_PIN, INPUT);

    Serial.println(mpu.testConnection() ? F("MPU6050 connection successful") : F("MPU6050 connection failed"));

    devStatus = mpu.dmpInitialize();

    // supply your own gyro offsets here, scaled for min sensitivity
    mpu.setXGyroOffset(220);
    mpu.setYGyroOffset(76);
    mpu.setZGyroOffset(-85);
    mpu.setZAccelOffset(1788); // 1688 factory default for my test chip

    // make sure it worked (returns 0 if so)
    if (devStatus == 0) {
        // turn on the DMP, now that it's ready
        Serial.println(F("Enabling DMP..."));
        mpu.setDMPEnabled(true);
        pinMode(INTERRUPT_PIN, INPUT);
        // enable Arduino interrupt detection
        Serial.print(F("Enabling interrupt detection (Arduino external interrupt "));
        Serial.print(digitalPinToInterrupt(INTERRUPT_PIN));
        Serial.println(F(")..."));
        attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), dmpDataReady, RISING);
        mpuIntStatus = mpu.getIntStatus();

        // set our DMP Ready flag so the main loop() function knows it's okay to use it
        Serial.println(F("DMP ready! Waiting for first interrupt..."));
        dmpReady = true;

        // get expected DMP packet size for later comparison
        packetSize = mpu.dmpGetFIFOPacketSize();
    } else {
        // ERROR!
        // 1 = initial memory load failed
        // 2 = DMP configuration updates failed
        // (if it's going to break, usually the code will be 1)
        Serial.print(F("DMP Initialization failed (code "));
        Serial.print(devStatus);
        Serial.println(F(")"));
    }
}

// ================================================================
// ===                      RF24                                ===
// ================================================================
#include <SPI.h>
#include "RF24.h"
#include <printf.h>
#include "CommUAV.h"
#include "DueTimer.h"

// Hardware configuration: Set up nRF24L01 radio on SPI bus plus pins. CE is 8, CS is 10.
RF24 radio(8,10);

byte add1[] = {0x34, 0xC3, 0x10, 0x10, 0x01};
uint8_t sendBuf[32];
uint8_t sendCnt=0;
uint8_t checksum=0;

uint8_t state=0;

int Throttle = 1500;
int Roll = 1499;
int Pitch = 1499;
int Yaw = 1500;

bool shouldSend = false;

void NRF24SendISR() {
  shouldSend = true;
}

void NRF24init() {
  radio.begin();
  radio.setPALevel(RF24_PA_LOW); //set to max to test drone
  radio.setChannel(40);
  radio.setCRCLength(RF24_CRC_16);
  radio.setDataRate(RF24_2MBPS);
  radio.setRetries(1,10);
  //radio.enableAckPayload();
  radio.enableDynamicAck();
  radio.openReadingPipe(0,add1);
  radio.openWritingPipe(add1);
  radio.closeReadingPipe(1); //was opened by radio.begin();
  radio.printDetails();
  NVIC_SetPriority(TC3_IRQn, 14);
//  Timer3.attachInterrupt(NRF24SendISR).setPeriod(2000000).start();
  Timer3.attachInterrupt(NRF24SendISR).setPeriod(20000).start();
}

void NRF24Send() {
  //Serial.print(F("Now sending "));
  //Serial.println(counter);

  if (!radio.write( &counter, sizeof(unsigned long))){
    Serial.println(F("failed"));
  }
  counter++;
}

void NRF24SendBuffer(uint8_t *buf) {
  /*
  Serial.print(F("Now sending "));
  for(int i = 0; i < 32; i++)
  {
    Serial.print(buf[i], HEX);
    Serial.print('-');
  }
  Serial.println();
  */

  if (!radio.write( buf, 32)) {
    Serial.println(F("failed"));
  }
}

void setup_timer7()
{
  // These are the clock frequencies available to the timers /2,/8,/32,/128
// 84Mhz/2 = 42.000 MHz
// 84Mhz/8 = 10.500 MHz
// 84Mhz/32 = 2.625 MHz
// 84Mhz/128 = 656.250 KHz
//
// 42Mhz/44.1Khz = 952.38
// 10.5Mhz/44.1Khz = 238.09
// 2.625Hmz/44.1Khz = 59.5
// 656Khz/44.1Khz = 14.88 // 131200 / 656000 = .2 (.2 seconds)

// 84Mhz/44.1Khz = 1904 instructions per tick
  /* turn on the timer clock in the power management controller */
  pmc_set_writeprotect(false);     // disable write protection for pmc registers
  pmc_enable_periph_clk(ID_TC7);   // enable peripheral clock TC7

  /* we want wavesel 01 with RC */
  TC_Configure(/* clock */TC2,/* channel */1, TC_CMR_WAVE | TC_CMR_WAVSEL_UP_RC | TC_CMR_TCCLKS_TIMER_CLOCK4);
  TC_SetRC(TC2, 1, 656000);
  TC_Start(TC2, 1);

  // enable timer interrupts on the timer
  TC2->TC_CHANNEL[1].TC_IER=TC_IER_CPCS;   // IER = interrupt enable register
  TC2->TC_CHANNEL[1].TC_IDR=~TC_IER_CPCS;  // IDR = interrupt disable register

  /* Enable the interrupt in the nested vector interrupt controller */
  /* TC4_IRQn where 4 is the timer number * timer channels (3) + the channel number (=(1*3)+1) for timer1 channel1 */
  NVIC_EnableIRQ(TC7_IRQn);
}

//void TC7_Handler()
//{
//  // We need to get the status to clear it and allow the interrupt to fire again
//  TC_GetStatus(TC2, 1);
//  state = !state;
//  TC_SetRC(TC2, 1, 656000); // 656000/ 656000 = 1 second
//
//}

// ================================================================
// ===                      SETUP                               ===
// ================================================================

void setup() {
  Serial.begin(115200);
  printf_begin();
  Serial.println("I'm alive");
  key_setup();

  NRF24init();
  control.init();
  DMPinit();
  delay(2000);
}

void check_btn()
{
  if(btn1_flag==1)
  {
    CommUAVUpload(MSP_ARM_IT);
    CommUAVUpload(MSP_ARM_IT);

    Serial.println("btn1_pressed");
    btn1_flag = 0;
  }

    if(btn2_flag==1)
  {
    CommUAVUpload(MSP_DISARM_IT);
    CommUAVUpload(MSP_DISARM_IT);
    Serial.println("btn2_pressed");
    btn2_flag = 0;
  }

    if(btn3_flag==1)
  {
    CommUAVUpload(MSP_ACC_CALI);
    CommUAVUpload(MSP_ACC_CALI);
    Serial.println("btn3_pressed");
    btn3_flag = 0;
  }

//    if(btn4_flag==1)
//  {
//    CommUAVUpload(MSP_SET_4CON);
//    Serial.println("btn4_pressed");
//    btn4_flag = 0;
//  }
}

int i =0;
void loop() {
  i++;

  //Serial.println("-");
  check_btn();

  if (Serial.available())
  {
    int input = toupper(Serial.read());
    if (input != 0 && input != 13 && input != 10) {
      cmd = (char) input;
    }
  }

  if ( cmd == 'A') {
    CommUAVUpload(MSP_ARM_IT);
  }
  if( cmd=='D') {
    CommUAVUpload(MSP_DISARM_IT);
  }
  if(cmd == 'C') {
    CommUAVUpload(MSP_ACC_CALI);
  }
//  if(cmd == 'T') {
//    CommUAVUpload(MSP_SET_4CON);
//  }


  noInterrupts();
  processDMP();
  interrupts();

  if(shouldSend == true){
    CommUAVUpload(MSP_SET_4CON);
    shouldSend = false;
    //Serial.println("rf sent");
  }

  if(i>=5000)
  {
//    Pitch = limit(map(ypr[1], M_PI/4, -M_PI/4, 1000.0, 2000.0), 1000, 2000);
//    Roll = limit(map(ypr[2], -M_PI/4, M_PI/4, 1000.0, 2000.0), 1000, 2000);
    Pitch = limit(map(ypr[1], -M_PI/4, M_PI/4, 1000.0, 2000.0), 1000, 2000);
    Roll = limit(map(ypr[2], M_PI/4, -M_PI/4, 1000.0, 2000.0), 1000, 2000);
    if (1600 > Pitch && Pitch > 1400) {
      Pitch = 1500;
    }
    if (1600 > Roll && Roll > 1400) {
      Roll = 1500;
    }
    Throttle = limit(control.readThrottle(), 1000, 1800); //limit(map(control.readFlex(A1), 0, 700, 1000, 2000), 1000, 2000); //TODO: Change map;
    Yaw = limit(control.readYaw(), 1000, 2000); //TODO: Change map;
    //Serial.print("Throttle: ");
    Serial.print(Throttle);
    Serial.print(",");
  //  //Serial.print(", Yaw: ");
    Serial.print(Yaw);
    Serial.print(",");
  //  //Serial.print(", Pitch: ");
    Serial.print(Pitch);
    Serial.print(",");
  //  //Serial.print(", Roll: ");
    Serial.println(Roll);

    i=0;
  }
}




static  void uart8chk(uint8_t _x)
{
  sendBuf[sendCnt++]=_x;
  checksum ^= _x;
}

static void uart16chk(int16_t a)
{
    uart8chk((uint8_t)(a&0xff));
    uart8chk((uint8_t)(a>>8));
}


void CommUAVUpload(uint8_t cmd)
{
//  uint8_t len;
    sendCnt=0;
    for(int i = 0; i++; i < 32) {
      sendBuf[i] = 0;
    }

    uart8chk('$');
    uart8chk('M');
    uart8chk('<');
    checksum = 0;

    switch(cmd)
    {
      case MSP_SET_4CON:
        uart8chk(8);            //data payload len
        uart8chk(cmd);
        uart16chk(Throttle);
        uart16chk(Yaw);
        uart16chk(Pitch);
        uart16chk(Roll);
        break;
      case MSP_ARM_IT:
        uart8chk(0);
        uart8chk(cmd);
        break;
      case MSP_DISARM_IT:
        uart8chk(0);
        uart8chk(cmd);
        break;
      case MSP_HOLD_ALT:
        uart8chk(0);
        uart8chk(cmd);
        break;
      case MSP_STOP_HOLD_ALT:
        uart8chk(0);
        uart8chk(cmd);
        break;
      case MSP_HEAD_FREE:
        uart8chk(0);
        uart8chk(cmd);
        break;
      case MSP_STOP_HEAD_FREE:
        uart8chk(0);
        uart8chk(cmd);
        break;
      case MSP_AUTO_LAND_DISARM:
        uart8chk(0);
        uart8chk(cmd);
        break;
      case MSP_ACC_CALI:
        uart8chk(0);
        uart8chk(cmd);
        break;
    }
    uart8chk(checksum);

    NRF24SendBuffer(sendBuf);
}

void processDMP() {
  // if programming failed, don't try to do anything
  if (!dmpReady) return;
  if(!mpuInterrupt) return;
  mpuInterrupt = false;
  mpuIntStatus = mpu.getIntStatus();

  // check for overflow (this should never happen unless our code is too inefficient)
  if ((mpuIntStatus & _BV(MPU6050_INTERRUPT_FIFO_OFLOW_BIT)) || fifoCount >= 1024) {
      // reset so we can continue cleanly
      mpu.resetFIFO();
      fifoCount = mpu.getFIFOCount();
      Serial.println(F("FIFO overflow!"));
  } else if (mpuIntStatus & _BV(MPU6050_INTERRUPT_DMP_INT_BIT)) {

    fifoCount = mpu.getFIFOCount();

    while (fifoCount >= packetSize) {
      // read a packet from FIFO
      mpu.getFIFOBytes(fifoBuffer, packetSize);
      // track FIFO count here in case there is > 1 packet available
      // (this lets us immediately read more without waiting for an interrupt)
      fifoCount -= packetSize;
      //display YAW PITCH ROLL
      mpu.dmpGetQuaternion(&q, fifoBuffer);
      mpu.dmpGetGravity(&gravity, &q);
      mpu.dmpGetYawPitchRoll(ypr, &q, &gravity);
    }
  }
}

long map(float x, float in_min, float in_max, float out_min, float out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

long map(long x, long in_min, long in_max, long out_min, long out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

long map(int x, int in_min, int in_max, int out_min, int out_max) {
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

long limit(long x, long low_val, long max_val) {
  if (x < low_val) {
    return low_val;
  } else if (x > max_val) {
    return max_val;
  } else {
    return x;
  }
}
