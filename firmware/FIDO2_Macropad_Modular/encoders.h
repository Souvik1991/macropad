/*
 * Encoder Module - Header
 * Handles single rotary encoder input (volume + mute)
 */

#ifndef ENCODERS_H
#define ENCODERS_H

void initEncoders();
void updateEncoders();
bool isMutedState();
extern volatile int encoderPosition;

#endif // ENCODERS_H
