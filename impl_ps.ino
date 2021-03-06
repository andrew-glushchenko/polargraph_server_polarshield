/**
*  Polargraph Server for ATMEGA1280+ 
*  Written by Sandy Noble
*  Released under GNU License version 3.
*  http://www.polargraph.co.uk
*  https://github.com/euphy/polargraph_server_polarshield

Specific features for Polarshield / arduino mega.
Implementation.

So this file is the interface for the extra features available in the 
mega/polarshield version of the polargraph.

*/

/*  Implementation of executeCommand for MEGA-sized boards that
have command storage features. */
void impl_processCommand(String com)
{
  lcd_echoLastCommandToDisplay(com, "usb:");
  
  // check for change mode commands
  if (com.startsWith(CMD_MODE_STORE_COMMANDS)
  || com.startsWith(CMD_MODE_LIVE))
  {
    Serial.println("Changing mode.");
    impl_executeCommand(com);
  }
  // else execute / store the command
  else if (storeCommands)
  {
    Serial.print(F("Storing command:"));
    Serial.println(com);
    sd_storeCommand(com);
  }
  else
  {
    impl_executeCommand(com);
  }
}

/**  
*  This includes the extra commands the MEGA is capable of handling.
*  It tries to run the command using the core executeBasicCommand
*  first, but if that doesn't work, then it will go through
*  it's own decision tree to try and run one of the additional
*  routines.
*/
void impl_executeCommand(String &com)
{
  if (exec_executeBasicCommand(com))
  {
    // that's nice, it worked
    //Serial.println("Executed basic.");
  }
  else if (com.startsWith(CMD_DRAWCIRCLEPIXEL))
    curves_pixel_drawCircularPixel();
//  else if (com.startsWith(CMD_TESTPATTERN))
//    testPattern();
  else if (com.startsWith(CMD_TESTPENWIDTHSCRIBBLE))
    impl_pixel_testPenWidthScribble();
  else if (com.startsWith(CMD_DRAWSAWPIXEL))
    impl_pixel_drawSawtoothPixel();
  else if (com.startsWith(CMD_DRAWDIRECTIONTEST))
    impl_exec_drawTestDirectionSquare();
  else if (com.startsWith(CMD_MODE_STORE_COMMANDS))
    impl_exec_changeToStoreCommandMode();
  else if (com.startsWith(CMD_MODE_LIVE))
    impl_exec_changeToLiveCommandMode();
  else if (com.startsWith(CMD_MODE_EXEC_FROM_STORE))
    impl_exec_execFromStore();
  else if (com.startsWith(CMD_RANDOM_DRAW))
    drawRandom();
  else if (com.startsWith(CMD_SET_ROVE_AREA))
    rove_setRoveArea();
  else if (com.startsWith(CMD_START_TEXT))
    rove_startText();
  else if (com.startsWith(CMD_DRAW_SPRITE))
    sprite_drawSprite();
  else if (com.startsWith(CMD_DRAW_RANDOM_SPRITE))
    sprite_drawRandomPositionedSprite();
  else if (com.startsWith(CMD_CHANGELENGTH_RELATIVE))
    exec_changeLength();
  else if (com.startsWith(CMD_SWIRLING))
    rove_controlSwirling();
  else if (com.startsWith(CMD_DRAW_NORWEGIAN))
    rove_drawNorwegianFromFile();
  else if (com.startsWith(CMD_DRAW_NORWEGIAN_OUTLINE))
    rove_drawRoveAreaFittedToImage();
  else
  {
    comms_unrecognisedCommand(com);
    comms_ready();
  }
  inNoOfParams = 0;
}

/** Polarshield implementation of runBackgroundProcesses. This is basically stuff to do with 
the screen.
*/
void impl_runBackgroundProcesses()
{
#ifdef USE_LCD  
  lcd_checkForInput();
  lcd_updateDisplay();
#endif
      
  long motorCutoffTime = millis() - lastOperationTime;
  if ((automaticPowerDown) && (powerIsOn) && (motorCutoffTime > motorIdleTimeBeforePowerDown))
  {
    Serial.println("Powering down because of inactivity.");
    lcd_runEndScript();
    lcd_updateDisplay();
  }
  
  if (swirling)
    rove_swirl();
}

void impl_loadMachineSpecFromEeprom()
{
}

void impl_exec_execFromStore()
{
  String fileToExec = inParam1;
  if (fileToExec != "")
  {
    currentlyDrawingFromFile = true;
    Serial.print("Filename to read from: ");
    Serial.println(fileToExec);
    commandFilename = fileToExec;
    impl_exec_execFromStore(commandFilename);
    currentlyDrawingFromFile = true;
  }
  else
  {
    Serial.println("No filename supplied to read from.");
  }
  
}

void impl_exec_execFromStore(String inFilename)
{
  if (inFilename != "")
  {
    String noBlanks = "";
    // remove blanks
    for (int i = 0; i<inFilename.length(); i++)
    {
      if (inFilename[i] != ' ')
        noBlanks = noBlanks + inFilename[i];
    }
    
    char filename[noBlanks.length()+1];
    noBlanks.toCharArray(filename, noBlanks.length()+1);
#ifdef DEBUG_SD    
    Serial.print("Array to read from: ");
    Serial.println(filename);
#endif
    File readFile = SD.open(filename, FILE_READ);
    if (readFile)
    {
      Serial.print("Opened file:");
      Serial.println(noBlanks);
      String command = "";
      while (readFile.available() && currentlyDrawingFromFile)
      {
#ifdef DEBUG_SD        
        Serial.println("Reading...");
        // poll for input
#endif
        char ch = readFile.read();
#ifdef DEBUG_SD        
        Serial.print(".");
        Serial.print(ch);
        Serial.print("-");
#endif
        if (ch == 13 || ch == 10)
        {
#ifdef DEBUG_SD        
          Serial.println("New line");
#endif
          // execute the line
          command.trim();
          boolean commandParsed = comms_parseCommand(command);
          if (commandParsed)
          {
#ifdef DEBUG_SD        
            Serial.println("Stored command parsed.");
#endif
            Serial.print(F("Executing command:"));
            Serial.println(command);
            if (echoingStoredCommands) lcd_echoLastCommandToDisplay(command, inFilename+": ");
            impl_executeCommand(command);
          }
#ifdef DEBUG_SD        
          else Serial.println("Stored command WAS NOT parsed.");
#endif            
          command = "";
          lcd_checkForInput();
        }
        else
          command += ch;

#ifdef DEBUG_SD        
        Serial.print("Command building:");
        Serial.println(command);
#endif
      }
      Serial.println("Finished with the file.");
      currentlyDrawingFromFile = false;
      readFile.close();
    }
    else
    {
      Serial.println("Couldn't find that file, btw.");
      currentlyDrawingFromFile = false;
    }
  }
  else
  {
    Serial.println("No filename supplied to read from.");
    currentlyDrawingFromFile = false;
  }
  
}

void impl_exec_changeToStoreCommandMode()
{
  String newfilename = inParam1;
  String newFile = inParam2;
  if (newfilename != "")
  {
    Serial.print("Filename for command store: ");
    Serial.println(newfilename);
    storeCommands = true;
    commandFilename = newfilename;
    if (newFile.equals("R"))
    {
      // delete file if it exists
      char filename[newfilename.length()+1];
      newfilename.toCharArray(filename, newfilename.length()+1);
      
      if (SD.exists(filename))
      {
        // file exists
        Serial.println(F("File already exists."));
        boolean removed = SD.remove(filename);
        if (removed)
          Serial.println(F("File removed."));
        
      }
    }
  }
  else
  {
    Serial.println("No filename supplied to write to.");
  }
}

void impl_exec_changeToLiveCommandMode()
{
  Serial.println(F("Changing back to live mode."));
  storeCommands = false;
}

void impl_pixel_testPenWidthScribble()
{
  int rowWidth = multiplier(asInt(inParam1));
  float startWidth = asFloat(inParam2);
  float endWidth = asFloat(inParam3); 
  float incSize = asFloat(inParam4);
  
  boolean ltr = true;
  
  float oldPenWidth = penWidth;
  int iterations = 0;
  
  int posA = motorA.currentPosition();
  int posB = motorB.currentPosition();

  int startColumn = posA;
  int startRow = posB;
  
  for (float pw = startWidth; pw <= endWidth; pw+=incSize)
  {
    iterations++;
    int column = posA;
    
    penWidth = pw;
    int maxDens = pixel_maxDensity(penWidth, rowWidth);
    Serial.print(F("Penwidth test "));
    Serial.print(iterations);
    Serial.print(F(", pen width: "));
    Serial.print(penWidth);
    Serial.print(F(", max density: "));
    Serial.println(maxDens);
    
    for (int density = maxDens; density >= 0; density--)
    {
      pixel_drawScribblePixel(posA, posB, rowWidth, density);
      posB+=rowWidth;
    }
    
    posA+=rowWidth;
    posB = startRow;
  }
  
  changeLength(long(posA-(rowWidth/2)), long(startRow-(rowWidth/2)));

  penWidth = oldPenWidth;
  
  moveB(0-rowWidth);
  for (int i = 1; i <= iterations; i++)
  {
    moveB(0-(rowWidth/2));
    moveA(0-rowWidth);
    moveB(rowWidth/2);
  }
  
  penWidth = oldPenWidth;
}    

void impl_engageMotors()
{
  motorA.enableOutputs();
  motorB.enableOutputs();
  powerIsOn = true;
  motorA.runToNewPosition(motorA.currentPosition()+32);
  motorB.runToNewPosition(motorB.currentPosition()+32);
  motorA.runToNewPosition(motorA.currentPosition()-32);
  motorB.runToNewPosition(motorB.currentPosition()-32);
  Serial.println("Engaged motors.");
}

void impl_releaseMotors()
{
  motorA.disableOutputs();
  motorB.disableOutputs();
  powerIsOn = false;
  Serial.println("Released motors");
}


void drawRandom()
{
  for (int i = 0; i < 1000; i++)
  {
    Serial.print("Drawing:");
    Serial.println(i);
    while (motorA.distanceToGo() != 0 && motorB.distanceToGo() != 0)
    {
      motorA.run();
      motorB.run();
    }

    if (motorA.distanceToGo() == 0)
    {
      int r = random(-2,3);
      motorA.move(r);

      Serial.print("Chosen new A target: ");
      Serial.println(r);
    }

    if (motorB.distanceToGo() == 0)
    {
      int r = random(-2,3);
      motorB.move(r);
      Serial.print("Chosen new B target: ");
      Serial.println(r);
    }
    
    reportPosition();
  }
}

void impl_exec_drawTestDirectionSquare()
{
  int rowWidth = multiplier(asInt(inParam1));
  int segments = asInt(inParam2);
  pixel_drawSquarePixel(rowWidth, rowWidth, segments, DIR_SE);
  moveA(rowWidth*2);
  
  pixel_drawSquarePixel(rowWidth, rowWidth, segments, DIR_SW);
  moveB(rowWidth*2);
  
  pixel_drawSquarePixel(rowWidth, rowWidth, segments, DIR_NW);
  moveA(0-(rowWidth*2));
  
  pixel_drawSquarePixel(rowWidth, rowWidth, segments, DIR_NE);
  moveB(0-(rowWidth*2));
  
}

void impl_pixel_drawSawtoothPixel()
{
    long originA = multiplier(asLong(inParam1));
    long originB = multiplier(asLong(inParam2));
    int size = multiplier(asInt(inParam3));
    int density = asInt(inParam4);

    int halfSize = size / 2;
    
    long startPointA;
    long startPointB;
    long endPointA;
    long endPointB;

    int calcFullSize = halfSize * 2; // see if there's any rounding errors
    int offsetStart = size - calcFullSize;
    
    if (globalDrawDirectionMode == DIR_MODE_AUTO)
      globalDrawDirection = pixel_getAutoDrawDirection(originA, originB, motorA.currentPosition(), motorB.currentPosition());
      

    if (globalDrawDirection == DIR_SE) 
    {
//      Serial.println(F("d: SE"));
      startPointA = originA - halfSize;
      startPointA += offsetStart;
      startPointB = originB;
      endPointA = originA + halfSize;
      endPointB = originB;
    }
    else if (globalDrawDirection == DIR_SW)
    {
//      Serial.println(F("d: SW"));
      startPointA = originA;
      startPointB = originB - halfSize;
      startPointB += offsetStart;
      endPointA = originA;
      endPointB = originB + halfSize;
    }
    else if (globalDrawDirection == DIR_NW)
    {
//      Serial.println(F("d: NW"));
      startPointA = originA + halfSize;
      startPointA -= offsetStart;
      startPointB = originB;
      endPointA = originA - halfSize;
      endPointB = originB;
    }
    else //(drawDirection == DIR_NE)
    {
//      Serial.println(F("d: NE"));
      startPointA = originA;
      startPointB = originB + halfSize;
      startPointB -= offsetStart;
      endPointA = originA;
      endPointB = originB - halfSize;
    }

    density = pixel_scaleDensity(density, 255, pixel_maxDensity(penWidth, size));
    
    changeLength(startPointA, startPointB);
    if (density > 1)
    {
      pixel_drawWavePixel(size, size, density, globalDrawDirection, SAW_SHAPE);
    }
    changeLength(endPointA, endPointB);
    
    //outputAvailableMemory();   
}
