#include "2_way_set_associative.h"

CacheL1 Cache1;
SetAssociativeL2Cache Cache2;

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
  Cache1.init = 0; 
  Cache2.init = 0;
  }

void accessL1(uint32_t address, uint8_t *data, uint32_t mode) {

  uint32_t index, Tag, offset, MemAddress;
  uint8_t TempBlock[BLOCK_SIZE];

  /* init cache */
  if (Cache1.init == 0) {
  
    Cache1.init = 1;

    for (int i = 0; i < L1_NUM_LINES; i++){
      Cache1.lines[i].Valid = 0;
      Cache1.lines[i].Dirty = 0;
      Cache1.lines[i].Tag = 0;
    }
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

  CacheLine *Line = &Cache1.lines[index];    // cache line extracted from the address

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
  return Cache1.lines[index].Dirty;
}

/*********************** L1 cache *************************/

void accessL2(uint32_t address, uint8_t *data, uint32_t mode) {

  uint32_t index, Tag, MemAddress, offset;
  uint8_t TempBlock[BLOCK_SIZE];  

  /* init cache */
  if (Cache2.init == 0) {
  
    Cache2.init = 1;

    for (int i = 0; i < NUM_SETS; i++){     // For each set we initialize both lines
      Cache2.sets[i].line1.Valid = 0;       
      Cache2.sets[i].line1.Dirty = 0;
      Cache2.sets[i].line1.Tag = 0;
      
      Cache2.sets[i].line2.Valid = 0;
      Cache2.sets[i].line2.Dirty = 0;
      Cache2.sets[i].line2.Tag = 0;
      
      Cache2.sets[i].head = &Cache2.sets[i].line1; // Initially, line1 is the head (most recently accessed)
      Cache2.sets[i].tail = &Cache2.sets[i].line2; // Initially, line2 is the tail (least recently accessed)
    }
    
  }
  // 18 tag bits + 8 index bits (256 sets) + 6 offset bits  
  // tag - checks if the loaded block matches the one in memory that contains the data we want to access (associated with an index)
  // index - tells us in what cache line the block is stored
  // block offset - tells us in which byte of the block the data is stored

  uint32_t offset_mask = 0x3F;
  uint32_t index_mask = 0x0003FC0;
  uint32_t tag_mask = 0xFFFF8000;

  offset = address & offset_mask;
  index = (address & index_mask) >> 6;
  Tag = (address & tag_mask) >> 14;

  MemAddress = address >> 6;     // removes offset bits to get the memory address to fetch block from    
  MemAddress = MemAddress << 6;  // adds back the least signifcant bits set to 0

  Set *set = &Cache2.sets[index];    // set extracted from the address
  CacheLine *Line1 = &Cache2.sets[index].line1;
  CacheLine *Line2 = &Cache2.sets[index].line2;

  uint8_t *block_ptr;

  /* Line 1 Miss*/
  if (!Line1->Valid || Line1->Tag != Tag) {         // if line1 not valid or block not present - line 1 miss
    
    /* Line 2 Miss*/
    if (!Line2->Valid || Line2->Tag != Tag){        // if line2 not valid or block not present - line 2 miss

      // Both lines missed, so we fetch a new block from DRAM
      accessDRAM(MemAddress, TempBlock, MODE_READ); 

      CacheLine *evictLine = set->tail;             // line to evict from set (least recently used)

      uint32_t evict_block_offset = (evictLine == Line1) ? 0 : BLOCK_SIZE;
      uint8_t *evicted_block_ptr = L2Cache + (index * 2 * BLOCK_SIZE) + evict_block_offset;

      // Eviction: Write back if dirty
      if (evictLine->Valid && evictLine->Dirty) {
          uint32_t evictMemAddress = (evictLine->Tag << 14) | (index << 6);
          accessDRAM(evictMemAddress, evicted_block_ptr, MODE_WRITE);             // writing dirty block to DRAM
      }

      memcpy(evicted_block_ptr, TempBlock, BLOCK_SIZE); // copy new block to the most recently accessed block
      evictLine->Valid = 1;
      evictLine->Tag = Tag;
      evictLine->Dirty = 0;

      set->head = evictLine;                                // Most recently accessed block updated
      set->tail = (evictLine == Line1) ? Line2 : Line1;     // Least recently accessed block updated
      
    } else{ // Line 2 Hit

      block_ptr = L2Cache + (index * 2* BLOCK_SIZE) + BLOCK_SIZE;
      set->head = Line2;
      set->tail = Line1;

    } 
     
  } else{ // Line 1 Hit
    
    block_ptr = L2Cache + (index * 2* BLOCK_SIZE);
    set->head = Line1;
    set->tail = Line2;
  
  }

  if (mode == MODE_READ) {    // reading from L2 cache
    memcpy(data, block_ptr + offset, WORD_SIZE);
    time += L2_READ_TIME;
  }

  if (mode == MODE_WRITE) { // writing data to L2 cache line from L1
    memcpy(block_ptr + offset, data, WORD_SIZE);
    time += L2_WRITE_TIME;
    set->head->Dirty = is_L1_dirty(address); // set the dirty bit to the value in L1 cache
  }
}


uint8_t is_L2_dirty(uint32_t address) {
    uint32_t index_mask = 0x0003FC0;
    uint32_t index = (address & index_mask) >> 6;

    uint32_t tag_mask = 0xFFFF8000;
    uint32_t tag = (address & tag_mask);

    Set *set = &Cache2.sets[index]; 

    // Checking Line 1
    if (set->line1.Valid && set->line1.Tag == tag) {
        return set->line1.Dirty; 
    }

    // Checking Line 2
    if (set->line2.Valid && set->line2.Tag == tag) {
        return set->line2.Dirty; 
    }

    return 0; // If not found, assume it's not dirty
}

void read(uint32_t address, uint8_t *data) {
  accessL1(address, data, MODE_READ);
}

void write(uint32_t address, uint8_t *data) {
  accessL1(address, data, MODE_WRITE);
}
