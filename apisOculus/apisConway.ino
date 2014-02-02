// Calculates frames in Conway's "Game of Life" simulation.
// This code was adapted from someone's reference implementation- JBK can't remember whose,
// but attribution for the core fuctionality goes to them.

byte world[FRAME_WIDTH][FRAME_HEIGHT][3]; // 0=currentgen, 1=nextgen, 2=age
long density = 50;
ColorRGB colors[8] = {{0, 0, 0}, {0x44, 0x44, 0}, {0, 0x44, 0x44}, {0x44, 0, 0x44}, {0x88, 0, 0}, {0, 0x88, 0}, {0, 0, 0x88}, {0x99, 0x99, 0x99}};
 
void randomizeWorld() {
  for (int col = 0; col < FRAME_WIDTH; col++) {
    for (int row = 0; row < FRAME_HEIGHT; row++) {
      if (random(100) < density) {
        world[col][row][0] = 1;
      }
      else {
        world[col][row][0] = 0;
      }
      world[col][row][1] = 0;
      world[col][row][2] = 0;
    }
  }
}
  
void nextFrameConway() {
  if(frameCount == 0)
    randomizeWorld();
    
  // Display current generation
  for (int col = 0; col < FRAME_WIDTH; col++) {
    for (int row = 0; row < FRAME_HEIGHT; row++) {
      nextFrameMatrix[col][row] = colors[world[col][row][2]];
    }
  }
 
  // Birth and death cycle 
  for (int col = 0; col < FRAME_WIDTH; col++) {
    for (int row = 0; row < FRAME_HEIGHT; row++) {
      // Default is for cell to stay the same
      world[col][row][1] = world[col][row][0];
      int count = neighbours(col, row); 
      if (count == 3 && world[col][row][0] == 0) {
        // A new cell is born
        world[col][row][1] = 1;
      }
      if (world[col][row][2] < 7) {
        world[col][row][2] += world[col][row][1];
      }
      if ((count < 2 || count > 3) && world[col][row][0] == 1) {
        // Cell dies
        world[col][row][1] = 0;
        world[col][row][2] = 0;
      }
    }
  }
  
  boolean changes = false;
  // Copy next generation into place
  for (int col = 0; col < FRAME_WIDTH; col++) { 
    for (int row = 0; row < FRAME_HEIGHT; row++) {
      if (world[col][row][0] != world[col][row][1]) {
        changes = true;
      }
      world[col][row][0] = world[col][row][1];
    }
  }
  if (!changes) {
    randomizeWorld();
  }
}
 
int neighbours(int col, int row) {
 return world[(col + 1) % FRAME_WIDTH][row][0] + 
         world[col][(row + 1) % FRAME_HEIGHT][0] + 
         world[(col + FRAME_WIDTH - 1) % FRAME_WIDTH][row][0] + 
         world[col][(row + FRAME_HEIGHT - 1) % FRAME_HEIGHT][0] + 
         world[(col + 1) % FRAME_WIDTH][(row + 1) % FRAME_HEIGHT][0] + 
         world[(col + FRAME_WIDTH - 1) % FRAME_WIDTH][(row + 1) % FRAME_HEIGHT][0] + 
         world[(col + FRAME_WIDTH - 1) % FRAME_WIDTH][(row + FRAME_HEIGHT - 1) % FRAME_HEIGHT][0] + 
         world[(col + 1) % FRAME_WIDTH][(row + FRAME_HEIGHT - 1) % FRAME_HEIGHT][0]; 
}
