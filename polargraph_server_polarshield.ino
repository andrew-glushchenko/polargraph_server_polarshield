/**
*  Polargraph Server for ATMEGA1280+ based arduino boards.
*  Written by Sandy Noble
*  Released under GNU License version 3.
*  http://www.polargraph.co.uk
*  https://github.com/euphy/polargraph_server_polarshield

The program has a core part that consists of the following files:

- comms.ino
- configuration.ino
- eeprom.ino
- exec.ino
- penlift.ino
- pixel.ino
- util.ino

and the first portion of this main file, probably called
something like polargraph_server_polarshield.ino.

This version which is for the polarshield has a 
bunch of other files too, providing extra functions.

The file called impl_ps perhaps deserves a special mention, and 
that file contains alternative implementations of a few functions,
where the changes to make it work on ATMEGA1280+ mean that code
is _different_ to the basic implemenation.

The UTouch library needs a couple of calibration values:

The UTouch library needs a couple of calibration values - these
ones are the ones I use for the ITDB02-2.2 inch screen.
#define CAL_X 0x039281CCUL
#define CAL_Y 0x03A2C1DEUL
#define CAL_S 0x000AF0DBUL

// for the 2.4in screen that is current.
#define CAL_X 0x03C34136UL
#define CAL_Y 0x03C0018AUL
#define CAL_S 0x000EF13FUL

Put them in libraries/UTouch/UTouchCD.h

**/

//http://forum.arduino.cc/index.php?topic=173584.0
#include <SPI.h>
#include <SD.h>
#include <AccelStepper.h>
#include <Servo.h>
#include <EEPROM.h>
#include "EEPROMAnything.h"

/*  ===========================================================  
         CONFIGURATION!!
    =========================================================== */    

//Uncomment the following line to use a 2.4" panel, August 2014 and later
#define LCD_TYPE TFT01_24_8
//Uncomment the following line to use an older 2.4" panel, prior to August 2014.
//#define LCD_TYPE ITDB24E_8
//Uncomment the following line to use a 2.2" panel
//#define LCD_TYPE ITDB22


/*  ===========================================================  
         Define what kind of driver breakout you're using.
         (By commenting out the one's you _haven't_ got.)
    =========================================================== */    
#ifndef MOTHERBOARD
#define MOTHERBOARD POLARSHIELD
//#define MOTHERBOARD RAMPS14
//#define MOTHERBOARD TFTSHIELD
#endif


#define POLARSHIELD 1
#define RAMPS14 2
#define TFTSHIELD 3

/*  ===========================================================  
    Control whether to look for touch input or update LCD
    Comment this out if you DON'T have an LCD connected
=========================================================== */    
#define USE_LCD

/*  ===========================================================  
    Some debugging flags
=========================================================== */    

//#define DEBUG_SD
//#define DEBUG_STATE

/*  ===========================================================  
    These variables are common to all polargraph server builds
=========================================================== */    

const String FIRMWARE_VERSION_NO = "1.10";
#if MOTHERBOARD == RAMPS14
  const String MB_NAME = "RAMPS14";
#elif MOTHERBOARD == POLARSHIELD
  const String MB_NAME = "POLARSHIELD";
#elif MOTHERBOARD == TFTSHIELD
  const String MB_NAME = "TFTSHIELD";
#endif

// for working out CRCs
static PROGMEM prog_uint32_t crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};

boolean usingCrc = false;

//  EEPROM addresses
const int EEPROM_MACHINE_WIDTH = 0;
const int EEPROM_MACHINE_HEIGHT = 2;
const int EEPROM_MACHINE_NAME = 4;
const int EEPROM_MACHINE_MM_PER_REV = 14; // 4 bytes (float)
const int EEPROM_MACHINE_STEPS_PER_REV = 18;
const int EEPROM_MACHINE_STEP_MULTIPLIER = 20;


const int EEPROM_MACHINE_MOTOR_SPEED = 22; // 4 bytes float
const int EEPROM_MACHINE_MOTOR_ACCEL = 26; // 4 bytes float
const int EEPROM_MACHINE_PEN_WIDTH = 30; // 4 bytes float

const long EEPROM_MACHINE_HOME_A = 34; // 4 bytes
const long EEPROM_MACHINE_HOME_B = 38; // 4 bytes

const int EEPROM_PENLIFT_DOWN = 42; // 2 bytes
const int EEPROM_PENLIFT_UP = 44; // 2 bytes

// Pen raising servo
Servo penHeight;
const int DEFAULT_DOWN_POSITION = 90;
const int DEFAULT_UP_POSITION = 180;
static int upPosition = DEFAULT_UP_POSITION; // defaults
static int downPosition = DEFAULT_DOWN_POSITION;
static int penLiftSpeed = 3; // ms between steps of moving motor

#if MOTHERBOARD == RAMPS14
  #define PEN_HEIGHT_SERVO_PIN 4
#else // MOTHERBOARD == POLARSHIELD
  #define PEN_HEIGHT_SERVO_PIN 9

#endif

boolean isPenUp = false;

int motorStepsPerRev = 400;
float mmPerRev = 95;
int stepMultiplier = 8;

static float translateX = 0.0;
static float translateY = 0.0;
static float scaleX = 1.0;
static float scaleY = 1.0;
static int rotateTransform = 0;

static int machineWidth = 650;
static int machineHeight = 800;
static int sqtest = 0;

static int defaultMachineWidth = 650;
static int defaultMachineHeight = 650;
static int defaultMmPerRev = 95;
static int defaultStepsPerRev = 400;
static int defaultStepMultiplier = 8;

static long startLengthStepsA = 8000;
static long startLengthStepsB = 8000;

String machineName = "";
const String DEFAULT_MACHINE_NAME = "PG01    ";

float currentMaxSpeed = 2000.0;
float currentAcceleration = 2000.0;
boolean usingAcceleration = true;

float mmPerStep = mmPerRev / multiplier(motorStepsPerRev);
float stepsPerMM = multiplier(motorStepsPerRev) / mmPerRev;

long pageWidth = machineWidth * stepsPerMM;
long pageHeight = machineHeight * stepsPerMM;
long maxLength = 0;

static String rowAxis = "A";
const int INLENGTH = 70;
const char INTERMINATOR = 10;

const String DIRECTION_STRING_LTR = "LTR";
static int SRAM_SIZE = 2048;
const String FREE_MEMORY_STRING = "MEMORY,";
int availMem = 0;

static float penWidth = 0.8; // line width in mm

boolean reportingPosition = true;
boolean acceleration = true;

extern AccelStepper motorA; 
extern AccelStepper motorB; 

volatile boolean currentlyRunning = true;

static String inCmd = "                                                  ";
static String inParam1 = "              ";
static String inParam2 = "              ";
static String inParam3 = "              ";
static String inParam4 = "              ";

int inNoOfParams;

int rebroadcastReadyInterval = 5000L;
volatile long lastOperationTime = 0L;
long motorIdleTimeBeforePowerDown = 600000L;
boolean automaticPowerDown = true;

volatile long lastInteractionTime = 0L;

static boolean lastWaveWasTop = true;
static boolean lastMotorBiasWasA = true;

//  Drawing direction
const static byte DIR_NE = 1;
const static byte DIR_SE = 2;
const static byte DIR_SW = 3;
const static byte DIR_NW = 4;

const static byte DIR_N = 5;
const static byte DIR_E = 6;
const static byte DIR_S = 7;
const static byte DIR_W = 8;
static int globalDrawDirection = DIR_NW;

const static byte DIR_MODE_AUTO = 1;
const static byte DIR_MODE_PRESET = 2;
const static byte DIR_MODE_RANDOM = 3;
static int globalDrawDirectionMode = DIR_MODE_AUTO;


static int currentRow = 0;

const String READY = "READY_200";
const String RESEND = "RESEND";
const String DRAWING = "DRAWING";
const static String OUT_CMD_CARTESIAN = "CARTESIAN,";
const static String OUT_CMD_SYNC = "SYNC,";

const String MSG = "MSG,";
const String MSG_ERROR = "E,";
const String MSG_INFO = "I,";
const String MSG_DEBUG = "D,";

static String readyString = READY;
static boolean pixelDebug = true;

static const byte ALONG_A_AXIS = 0;
static const byte ALONG_B_AXIS = 1;
static const byte SQUARE_SHAPE = 0;
static const byte SAW_SHAPE = 1;

String lastCommand = "";
boolean commandConfirmed = false;

const static String COMMA = ",";
const static String CMD_END = ",END";
const static String CMD_CHANGELENGTH = "C01";
const static String CMD_CHANGEPENWIDTH = "C02";
const static String CMD_CHANGEMOTORSPEED = "C03";
const static String CMD_CHANGEMOTORACCEL = "C04";
const static String CMD_DRAWPIXEL = "C05";
const static String CMD_DRAWSCRIBBLEPIXEL = "C06";
const static String CMD_CHANGEDRAWINGDIRECTION = "C08";
const static String CMD_SETPOSITION = "C09";
const static String CMD_TESTPATTERN = "C10";
const static String CMD_TESTPENWIDTHSQUARE = "C11";
const static String CMD_PENDOWN = "C13";
const static String CMD_PENUP = "C14";
const static String CMD_CHANGELENGTHDIRECT = "C17";
const static String CMD_SETMACHINESIZE = "C24";
const static String CMD_SETMACHINENAME = "C25";
const static String CMD_GETMACHINEDETAILS = "C26";
const static String CMD_RESETEEPROM = "C27";
const static String CMD_SETMACHINEMMPERREV = "C29";
const static String CMD_SETMACHINESTEPSPERREV = "C30";
const static String CMD_SETMOTORSPEED = "C31";
const static String CMD_SETMOTORACCEL = "C32";
const static String CMD_SETMACHINESTEPMULTIPLIER = "C37";
const static String CMD_SETPENLIFTRANGE = "C45";
const static String CMD_PIXELDIAGNOSTIC = "C46";

void setup() 
{
  Serial.begin(57600);           // set up Serial library at 57600 bps
  Serial.println(F("POLARGRAPH ON!"));
  Serial.print(F("v"));
  Serial.println(FIRMWARE_VERSION_NO);
  Serial.print(F("Hardware: "));
  Serial.println(MB_NAME);

  Serial.print(F("Servo "));
  Serial.println(PEN_HEIGHT_SERVO_PIN);


  configuration_motorSetup();
  eeprom_loadMachineSpecFromEeprom();
  configuration_setup();

  motorA.setMaxSpeed(currentMaxSpeed);
  motorA.setAcceleration(currentAcceleration);  
  motorB.setMaxSpeed(currentMaxSpeed);
  motorB.setAcceleration(currentAcceleration);
  
  motorA.setCurrentPosition(startLengthStepsA);
  motorB.setCurrentPosition(startLengthStepsB);
  readyString = READY;
  comms_establishContact();

  penlift_penUp();
  delay(500);
  outputAvailableMemory();
  
//  impl_engageMotors();
//  lcd_runStartScript()
}

void loop()
{
  lastCommand = comms_waitForNextCommand();
  comms_parseAndExecuteCommand(lastCommand);
}


/*===========================================================  
    These variables are for the polarshield / mega
=========================================================== */    
#include <UTFT.h>
#include <UTouch.h>



const static String CMD_TESTPENWIDTHSCRIBBLE = "C12";
const static String CMD_DRAWSAWPIXEL = "C15,";
const static String CMD_DRAWCIRCLEPIXEL = "C16";
const static String CMD_SET_ROVE_AREA = "C21";
const static String CMD_DRAWDIRECTIONTEST = "C28";
const static String CMD_MODE_STORE_COMMANDS = "C33";
const static String CMD_MODE_EXEC_FROM_STORE = "C34";
const static String CMD_MODE_LIVE = "C35";
const static String CMD_RANDOM_DRAW = "C36";
const static String CMD_START_TEXT = "C38";
const static String CMD_DRAW_SPRITE = "C39";
const static String CMD_CHANGELENGTH_RELATIVE = "C40";
const static String CMD_SWIRLING = "C41";
const static String CMD_DRAW_RANDOM_SPRITE = "C42";
const static String CMD_DRAW_NORWEGIAN = "C43";
const static String CMD_DRAW_NORWEGIAN_OUTLINE = "C44";

/*  End stop pin definitions  */
const int ENDSTOP_X_MAX = 17;
const int ENDSTOP_X_MIN = 16;
const int ENDSTOP_Y_MAX = 15;
const int ENDSTOP_Y_MIN = 14;

long ENDSTOP_X_MIN_POSITION = 130;
long ENDSTOP_Y_MIN_POSITION = 130;

long motorARestPoint = 0;
long motorBRestPoint = 0;

/* Stuff for display */
extern uint8_t SmallFont[];
extern uint8_t BigFont[];
UTFT   lcd(LCD_TYPE, 38,39,40,41);

#if MOTHERBOARD == TFTSHIELD
UTouch touch(6, 5, 4, 3, 2); // pinouts for the TFT shield
#else
UTouch touch(11,12,18,19, 2);
#endif
const int INTERRUPT_TOUCH_PIN = 0;
boolean displayTouched = false;
int touchX = 0;
int touchY = 0;

// size and location of rove area
long rove1x = 1000;
long rove1y = 1000;
long roveWidth = 5000;
long roveHeight = 8000;

boolean swirling = false;
String spritePrefix = "";
int textRowSize = 200;
int textCharSize = 180;

boolean useRoveArea = false;

int commandNo = 0;
int errorInjection = 0;

long screenSaveIdleTime = 1200000L;
const static byte SCREEN_STATE_NORMAL = 0;
const static byte SCREEN_STATE_POWER_SAVE = 1;
byte screenState = SCREEN_STATE_NORMAL;

boolean storeCommands = false;
boolean drawFromStore = false;
String commandFilename = "";

// sd card stuff
const int chipSelect = 53;
boolean sdCardInit = false;

// set up variables using the SD utility library functions:
File root;
boolean cardPresent = false;
boolean cardInit = false;
boolean echoingStoredCommands = false;

// the file itself
File pbmFile;

// information we extract about the bitmap file
long pbmWidth, pbmHeight;
float pbmScaling = 1.0;
int pbmDepth, pbmImageoffset;
long pbmFileLength = 0;
float pbmAspectRatio = 1.0;

volatile int speedChangeIncrement = 100;
volatile int accelChangeIncrement = 100;
volatile float penWidthIncrement = 0.05;
volatile int moveIncrement = 400;

boolean currentlyDrawingFromFile = false;
String currentlyDrawingFilename = "";
boolean powerIsOn = false;
boolean isCalibrated = false;


















