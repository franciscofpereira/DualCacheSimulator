#include "L2Cache.h"

Cache DualCache;

uint8_t L1Cache[L1_SIZE];
uint8_t L2Cache[L2_SIZE];
uint8_t DRAM[DRAM_SIZE];
uint32_t time;


/**************** Time Manipulation ***************/
void resetTime() { time = 0; }

uint32_t getTime() { return time; }

/****************  RAM memory (byte addressable) ***************/
void accessDRAM(uint32_t address, uint8_t *data, uint32_t mode) {

  if (address >= DRAM_SIZE - WORD_SIZE + 1)
    exit(-1);

  // Reading from DRAM
  if (mode == MODE_READ) {
    memcpy(data, &(DRAM[address]), BLOCK_SIZE);
    time += DRAM_READ_TIME;
  }

  // Writing to DRAM
  if (mode == MODE_WRITE) {
    memcpy(&(DRAM[address]), data, BLOCK_SIZE);
    time += DRAM_WRITE_TIME;
  }
}

/*********************** L1 cache *************************/

void initCache() { 
  
  DualCache.init = 0; 

  for (int i = 0; i < L1_NUM_LINES; i++){
      DualCache.L1[i].Valid = 0;
      DualCache.L1[i].Dirty = 0;
      DualCache.L1[i].Tag = 0;
    }

  for (int i = 0; i < L2_NUM_LINES; i++){
      DualCache.L2[i].Valid = 0;
      DualCache.L2[i].Dirty = 0;
      DualCache.L2[i].Tag = 0;
    }
  }

void accessL1(uint32_t address, uint8_t *data, uint32_t mode) {

  uint32_t index, Tag, offset, MemAddress;
  uint8_t TempBlock[BLOCK_SIZE];

  /* init cache */
  if (DualCache.init == 0) {  
    DualCache.init = 1;
  }

  // 18 tag bits + 8 index bits + 6 offset bits
  // tag - checks if the loaded block matches the one in memory that contains the data we want to access (associated with an index)
  // index - tells us in what cache line the block is stored
  // block offset - tells us in which byte of the block the data is stored

  uint32_t offset_mask = 0x3F;
  uint32_t index_mask = 0x0003FC0;
  uint32_t tag_mask = 0xFFFFC000;

  MemAddress = address >> 6;     // removes offset bits to get the memory address to fetch block from    
  MemAddress = MemAddress << 6;  // adds back the least signifcant bits set to 0

  offset = address & offset_mask;
  index = (address & index_mask) >> 6;
  Tag = (address & tag_mask) >> 14;

  uint8_t *block_ptr = L1Cache + (index * BLOCK_SIZE);

  CacheLine *Line = &DualCache.L1[index];    // cache line extracted from the address

  /* access Cache*/
  if (!Line->Valid || Line->Tag != Tag) {   // if line not valid or block not present - L1 cache miss
    
    accessL2(address, TempBlock, MODE_READ); // get new block from L2 cache
    
    if ((Line->Valid) && (Line->Dirty)) { // line in L1 cache has dirty block (we need to write it to L2 cache before overwriting it)
      
      accessL2(address, block_ptr, MODE_WRITE); // writing back to L2 cache
    }

    memcpy(block_ptr, TempBlock, BLOCK_SIZE); // loading data block to L1 cache line
    Line->Valid = 1;
    Line->Tag = Tag;
    Line->Dirty = is_L2_dirty(address); // no need to change dirty bit
  } // if miss, then replaced with the correct block


  if (mode == MODE_READ) {    // read data from cache line
    memcpy(data, block_ptr + offset, WORD_SIZE);
    time += L1_READ_TIME;
  }

  if (mode == MODE_WRITE) { // write data to cache line
    memcpy(block_ptr + offset, data, WORD_SIZE);
    time += L1_WRITE_TIME;
    Line->Dirty = 1;
  }
}

uint8_t is_L1_dirty(uint32_t address) {
  uint32_t index_mask = 0x0003FC0;
  uint32_t index = (address & index_mask) >> 6;
  return DualCache.L1[index].Dirty;
}

/*********************** L1 cache *************************/

void accessL2(uint32_t address, uint8_t *data, uint32_t mode) {

  uint32_t index, Tag, MemAddress, offset;
  uint8_t TempBlock[BLOCK_SIZE];

  /* init cache */
  if (DualCache.init == 0) {
    DualCache.init = 1;
  }
  // 17 tag bits + 9 index bits + 6 offset bits
  // tag - checks if the loaded block matches the one in memory that contains the data we want to access (associated with an index)
  // index - tells us in what cache line the block is stored
  // block offset - tells us in which byte of the block the data is stored

  uint32_t offset_mask = 0x3F;
  uint32_t index_mask = 0x0007FC0;
  uint32_t tag_mask = 0xFFFF8000;

  offset = address & offset_mask;
  index = (address & index_mask) >> 6;
  Tag = (address & tag_mask) >> 15;

  MemAddress = address >> 6;     // removes offset bits to get the memory address to fetch block from    
  MemAddress = MemAddress << 6;  // adds back the least signifcant bits set to 0

  uint8_t *block_ptr = L2Cache + (index * BLOCK_SIZE);

  CacheLine *Line = &DualCache.L2[index];    // cache line extracted from the address

  /* access Cache*/
  if (!Line->Valid || Line->Tag != Tag) {         // if line not valid or block not present - cache miss
    accessDRAM(MemAddress, TempBlock, MODE_READ); // get new block from DRAM

    if ((Line->Valid) && (Line->Dirty)) { // line has dirty block (we need to write it to DRAM before overwriting it)
      //MemAddress = Line->Tag << 6;        // get address of the block in memory
      accessDRAM(MemAddress, block_ptr, MODE_WRITE); // then write back old block
    }

    memcpy(block_ptr, TempBlock, BLOCK_SIZE); // copy new block to cache line
    Line->Valid = 1;
    Line->Tag = Tag;
    Line->Dirty = 0;
  } // if miss, then replaced with the correct block


  if (mode == MODE_READ) {    // reading from L2 cache
    memcpy(data, block_ptr + offset, WORD_SIZE);
    time += L2_READ_TIME;
  }

  if (mode == MODE_WRITE) { // writing data to L2 cache line from L1
    memcpy(block_ptr + offset, data, WORD_SIZE);
    time += L2_WRITE_TIME;
    Line->Dirty = is_L1_dirty(address); // set the dirty bit to the value in L1 cache
  }
}

uint8_t is_L2_dirty(uint32_t address) {
  uint32_t index_mask = 0x0007FC0;
  uint32_t index = (address & index_mask) >> 6;
  return DualCache.L2[index].Dirty;
}

void read(uint32_t address, uint8_t *data) {
  accessL1(address, data, MODE_READ);
}

void write(uint32_t address, uint8_t *data) {
  accessL1(address, data, MODE_WRITE);
}
