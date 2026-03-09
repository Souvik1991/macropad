/*
 * Key Matrix Module - Header
 * Handles 3x4 key matrix scanning and macro execution
 */

#ifndef KEYMATRIX_H
#define KEYMATRIX_H

void initKeyMatrix();
void scanKeyMatrix();
void tickSequenceMacro();  // Non-blocking sequence continuation (call from loop)
void enterSystemMenu();
void updateSystemMenu();
void updateSystemResetConfirm();

// Returns true if the key at (row, col) is currently pressed
bool isKeyPressed(int row, int col);

#endif // KEYMATRIX_H
