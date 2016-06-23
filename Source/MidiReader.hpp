#ifndef _MIDIREADER_H_
#define _MIDIREADER_H_

#include <inttypes.h>
#include "MidiStatus.h"
#include "MidiHandler.h"

enum MidiReaderStatus {
  READY_STATUS, INCOMPLETE_STATUS, ERROR_STATUS
};

class MidiReader : public MidiHandler {
private:
  uint8_t buffer[MIDI_MAX_MESSAGE_SIZE];
  static const int size = MIDI_MAX_MESSAGE_SIZE;
  MidiReaderStatus status; // state, should be status: READY / INCOMPLETE / ERROR
  unsigned char runningStatus;
  int pos;
public:
  MidiReader() : 
    status(READY_STATUS), 
    runningStatus(0), 
    pos(0) {
  }

  ~MidiReader(){
  }

  void clear(){
    runningStatus = buffer[0];
    pos = 0;
    status = READY_STATUS;
  }

  unsigned char* getMessage(int& length){
    length = pos;
    return buffer;
  }

  MidiReaderStatus read(unsigned char data){
    if(status == READY_STATUS){
      clear(); // discard previous message
    }else if(pos > size){
      status = ERROR_STATUS;
      // todo: throw exception
      return status;
    }
    buffer[pos++] = data;
    switch(buffer[0] & MIDI_STATUS_MASK){
      // two byte messages
    case PROGRAM_CHANGE:
      if(pos == 2){
	status = READY_STATUS;
	handleProgramChange(buffer[0], buffer[1]);
      }else{
	status = INCOMPLETE_STATUS;
      }
      break;
    case CHANNEL_PRESSURE:
      if(pos == 2){
	status = READY_STATUS;
	handleChannelPressure(buffer[0], buffer[1]);
      }else{
	status = INCOMPLETE_STATUS;
      }
      break;
      // three byte messages
    case NOTE_OFF:
      if(pos == 3){
	status = READY_STATUS;
	handleNoteOff(buffer[0], buffer[1], buffer[2]);
      }else{
	status = INCOMPLETE_STATUS;
      }
      break;
    case NOTE_ON:
      if(pos == 3){
	status = READY_STATUS;
	if(buffer[2] == 0)
	  handleNoteOff(buffer[0], buffer[1], buffer[2]);
	else
	  handleNoteOn(buffer[0], buffer[1], buffer[2]);
      }else{
	status = INCOMPLETE_STATUS;
      }
      break;
    case POLY_KEY_PRESSURE:
      if(pos == 3){
	status = READY_STATUS;
	handlePolyKeyPressure(buffer[0], buffer[1], buffer[2]);
      }else{
	status = INCOMPLETE_STATUS;
      }
      break;
    case CONTROL_CHANGE:
      if(pos == 3){
	status = READY_STATUS;
	handleControlChange(buffer[0], buffer[1], buffer[2]);
      }else{
	status = INCOMPLETE_STATUS;
      }
      break;
    case PITCH_BEND_CHANGE:
      if(pos == 3){
	status = READY_STATUS;
	handlePitchBend(buffer[0], (buffer[2]<<7) | buffer[1]);
      }else{
	status = INCOMPLETE_STATUS;
      }
      break;
    case SYSTEM_COMMON:
      switch(buffer[0]){
      case TIME_CODE_QUARTER_FRAME:
      case RESERVED_F4:
      case RESERVED_F9:
      case TUNE_REQUEST:
      case TIMING_CLOCK:
      case START:
      case CONTINUE:
      case STOP:
      case RESERVED_FD:
      case ACTIVE_SENSING:
      case SYSTEM_RESET:
	// one byte messages
	status = READY_STATUS;
	handleSystemCommon(buffer[0]);
	break;
      case SYSEX:
	if(data == SYSEX_EOX){
	  status = READY_STATUS;
	  handleSysEx(buffer+1, pos-2);
	}else if(data >= STATUS_BYTE && pos > 1){
	  // SysEx message terminated by a status byte different from SYSEX_EOX
	  buffer[pos-1] = SYSEX_EOX;
	  status = READY_STATUS;
	  handleSysEx(buffer+1, pos-2);
	  buffer[0] = data; // save status byte for next message - will be saved as running status
	}else{
	  status = INCOMPLETE_STATUS;
	}
	break;
      case SYSEX_EOX: // receiving SYSEX_EOX on its own is really an error
      default:
	status = ERROR_STATUS;
	break;
      }
      break;
    default:
      if(pos == 1 && data < STATUS_BYTE && runningStatus >= STATUS_BYTE){
	// MIDI running status: this message is missing the status byte, re-use previous status
	buffer[pos++] = data;
	buffer[0] = runningStatus;
      }else{
	status = ERROR_STATUS;
      }
    }
    return status;
  }

  // read a 4-byte USB data frame
  // todo: dispatch directly to handleXYZ() methods
  MidiReaderStatus readFrame(unsigned char* frame){
    MidiReaderStatus status = INCOMPLETE_STATUS;
    switch(frame[0]){
    case USB_COMMAND_MISC:
    case USB_COMMAND_CABLE_EVENT:
      // ignore
      break;
    case USB_COMMAND_SINGLE_BYTE:
    case USB_COMMAND_SYSEX_EOX1:
      // one byte message
      status = read(frame[1]);
      break;
    case USB_COMMAND_2BYTE_SYSTEM_COMMON:
    case USB_COMMAND_SYSEX_EOX2:
    case USB_COMMAND_PROGRAM_CHANGE:
    case USB_COMMAND_CHANNEL_PRESSURE:
      read(frame[1]);
      status = read(frame[2]);
      break;
    case USB_COMMAND_NOTE_OFF:
    case USB_COMMAND_NOTE_ON:
    case USB_COMMAND_POLY_KEY_PRESSURE:
    case USB_COMMAND_CONTROL_CHANGE:
    case USB_COMMAND_PITCH_BEND_CHANGE:
      // three byte message
      read(frame[1]);
      read(frame[2]);
      status = read(frame[3]);
      break;
    default:
      switch(frame[0] & 0xf0){
      case OWL_COMMAND_DISCOVER:
	break;
      case OWL_COMMAND_PARAMETER:
	// parameter change
	handleParameterChange(frame[1], (frame[2]<<8) | frame[3]);
	status = READY_STATUS;
	break;
      case OWL_COMMAND_DATA:
	// OSC, firmware or file data
	// 0x30, type, sizeH, sizeL
	// uint16_t size = (frame[2]<<8) | frame[3]);
	break;
      case OWL_COMMAND_SYNC:
	// 0xc0 until 0xff at end of frame
	// use ASCII SYN instead?
	break;
      }
      // if((frame[0] & 0x0f) != uid && (frame[0] & 0x0f) != (uid+1)){
      // 	// we are not the originator
      // 	// forward message
      // }
      // ignore
    }
    return status;
  }

};


#endif /* _MIDIREADER_H_ */
