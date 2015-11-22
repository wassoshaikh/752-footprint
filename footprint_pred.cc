/*
 * Footprint predictor for 3D DRAM 
 * cs 752 fall 2015
 * written by Mitchell Manar - manarm@cs.wisc.edu
 */
#include "footprint_pred.hh"
#include <iostream>
#include <climits> 

int main()
{
  bool in_mem, in_mem_prev;
  Random random_gen;


  // Test one: access memory in 4K strides. Watch footprint change, migrate on threshhold
  cout << "Starting test 1: access memory in 4K strides" << endl;
  FootprintPred fpred(2048,1,4096,1024*1024,0.5,"footprint");
  in_mem = false;
  in_mem_prev = false;
  for(int page = 0; page < 2048; ++page){
    in_mem_prev = in_mem;
    in_mem = fpred.addr_inFastMem((Addr)page*4096);
    if(!in_mem_prev && in_mem){
      cout << "Migrated on page " << page << endl;
    }
  }

  cout << endl << "-----------------------" << endl;

  // Test two: create 4-way footprint table and test eviction. 
  cout << "Starting test 2: testing eviction in the footprint table" << endl;
  FootprintPred fpred2(32,4,4096,1024*1024,0.5,"footprint");
  Addr addr = 4*1024*1024; // Second huge page (create entry)
  // Last acess should generate an eviction in fprint table
  for(int i = 0; i < 5; ++i){
    in_mem = fpred2.addr_inFastMem(addr);
    addr += 32*1024*1024; // stride 32MB
  }

  cout << endl << "-----------------------" << endl;

  // Test three: fill the translation table so that the next page migrated in forces an eviction
  // There are 256 entries in the trans table; create 257
  cout << "Starting test 3: testing eviction in the translation table" << endl;
  FootprintPred fpred3(2048,1,4096,1024*1024,0.5,"footprint");
  for(Addr hugePage = 0; hugePage < 257; ++hugePage){
    for(int page = 0; page < 515; ++page){
      fpred3.addr_inFastMem((Addr)(hugePage*4*1024*1024 + page*4096));
    }
  }

  cout << endl << "-----------------------" << endl;

  // Test four: perform 100K random accesses. Could take ahwile.
  cout << "Starting test 4: 100K random accesses" << endl;
  FootprintPred fpred4(32,4,4096,1024*1024,0.5,"footprint");
  for(int i = 0; i < 100000; ++i){
      fpred4.addr_inFastMem((Addr)random_gen.random<Addr>(0, ULONG_MAX));
  }
 
  cout << endl << "-----------------------" << endl;

  // Test five: perform 100K random accesses with hot/cold. Should generate 10 migrations.
  cout << "Starting test 5: 100K random accesses with hot/cold" << endl;
  FootprintPred fpred5(32,4,4096,1024*1024,0.5,"hotcold");
  for(int i = 0; i < 100000; ++i){
      fpred5.addr_inFastMem((Addr)random_gen.random<Addr>(0, ULONG_MAX));
  }
  
  cout << "All tests succeeded!" << endl;
 
  return 0;
}

// Hsize = size of huge page in KB
// fsize = total # of ways in footprint table
// ramsize = size of 3d ram in KB
// thresh = % small pages touched before migration 
FootprintPred::FootprintPred(int fsize, int fassoc, int hsize, int ramsize, float thresh, string mpolicy) :
fpsize(fsize/fassoc), fpassoc(fassoc), hugesize(hsize), ram_size(ramsize), threshold(thresh),
hotCold_clock(0), clock_hand(0), random_gen() 
{
  lg_fpsize = (int)log2(fpsize);
  hugebits = (int)log2(hugesize) + 10; // in KB
  ram_pages = ram_size / hugesize; // both in KB
  addrbits = sizeof(Addr) * 8;
   
  if(mpolicy.compare("footprint") == 0){
    policy = FOOTPRINT; 
  } else {
    policy = HOTCOLD;
  } 

  bool hotTrue = policy == HOTCOLD;

  cout << "Footprint table size " << fpsize << endl;
  cout << "Footprint table assoc " << fpassoc << endl;
  cout << "Pages in 3DRAM " << ram_pages << endl;
  cout << "Bits in hugepage offset " << hugebits << endl;
  cout << "Policy is hotcold? " << hotTrue << endl;

  footprint_table = (fp_entry ***)malloc(fpsize * sizeof(fp_entry **));
  for(int i = 0; i < fpsize; ++i){
    footprint_table[i] = (fp_entry **)malloc(fpassoc * sizeof(fp_entry *));
    for(int j = 0; j < fpassoc; ++j){
      footprint_table[i][j] = (fp_entry *)malloc(sizeof(fp_entry));
      init_fp_entry(footprint_table[i][j]);
    }
  }

  trans_table = (trans_entry *)malloc(ram_pages * sizeof(trans_entry));
  for(int i = 0; i < ram_pages; ++i){
    trans_table[i] = init_trans_entry();
  }
};
    
bool 
FootprintPred::addr_inFastMem(Addr addr){
  int tag = get_hugetag(addr);
  bool inFastMem = false;
  hotCold_clock++; 

  for(int i = 0; i < ram_pages; ++i){
    if(trans_table[i].valid && trans_table[i].tag == tag){
      trans_table[i].clock_set = true;
      inFastMem = true;
    } 
  }

  mru = addr;
  if(policy == FOOTPRINT){ // handles footprint predictor + migration
    if(!inFastMem){
      record_access(addr);
    }
  } else { // HOTCOLD
    if(hotCold_clock == TIMESLICE){
      migrate_page(mru);
      hotCold_clock = 0; 
    }
  }

  return inFastMem;
}  

    
void 
FootprintPred::record_access(Addr addr){
  Addr index = get_index(addr);
  Addr tag = get_tag(addr);
  int idx;

  fp_entry *entry = NULL;

  for(int i = 0; i < fpassoc; ++i){
    if(footprint_table[index][i]->valid && footprint_table[index][i]->tag == tag){
      entry = footprint_table[index][i];
    }
  }

  if(entry){
    // entry found, mark page in footprint
    Addr offset = get_small_pg(addr);
    bool already_set = entry->footprint[offset];
   
    // if page not already marked, recalc sum
    if(!already_set){
      entry->footprint[offset] = true;
      entry->footprint_sum++;
    }
    
    // If count exceeds threshhold, migrate
    if(entry->footprint_sum > threshold * pow(2,hugebits-PGBITS)){
      entry->valid = false;
      migrate_page(addr);
    } else {    // promote to head for MRU
      fp_entry tmp = *footprint_table[index][0];
      *footprint_table[index][0] = *entry;
      *entry = tmp; 
    }
  } else {      // No entry found -- nMRU replacement (MRU in slot 0)
    //cout << "New footprint entry for page index " << index << " and tag " << tag << endl;
    if(fpassoc > 1){
      idx = -1;
      for(int i = 0; i < fpassoc; ++i){
        if(footprint_table[index][i]->valid == false){
          idx = i;
          break;
        }
      } 
      if(idx == -1){
        int idx = random_gen.random<int>(1, fpassoc - 1);
        //cout << "Evicting way " << "from index " << index << endl;
      }

      footprint_table[index][idx] = footprint_table[index][0];
    }
  
    fp_entry *new_entry = footprint_table[index][0];
    new_entry->valid = true;
    new_entry->tag = tag;
    new_entry->footprint_sum = 1;
    for(int i = 0; i < pow(2,hugebits-PGBITS); ++i){
      new_entry->footprint[i] = false;
    }
    new_entry->footprint[get_small_pg(addr)] = true;
  }

  return;
} 

// Places addr huge page in translation table, evicting using
// clock plru as necessary 
void 
FootprintPred::migrate_page(Addr addr){
  cout << "Migrating page..." << endl;

  // look for invalid page first
  int victim = -1;
  bool entry_found = false;
  for(int i = 0; i < ram_pages; ++i){
    if(trans_table[i].valid == false){
      victim = i;
      entry_found = true;
      break;
    }
  }
  if(!entry_found){
  cout << "Evicting a page from trans table" << endl;
  // clock: Advance hand: clear 1's, evict on 0. 
    while(victim == -1){
      if(trans_table[clock_hand].clock_set){
        trans_table[clock_hand].clock_set = false;
        clock_hand = (clock_hand + 1) % ram_pages;
      } else {
        victim = clock_hand;
        clock_hand = (clock_hand + 1) % ram_pages;
      }
    }
  }
      
  trans_table[victim].tag = get_hugetag(addr);
  trans_table[victim].valid = true;
  trans_table[victim].clock_set = false;
  
  return;
}

void
FootprintPred::init_fp_entry(fp_entry *entry){
  entry->tag = 0;
  entry->valid = false;
  entry->footprint_sum = 0;
  entry->footprint = (bool *)malloc(pow(2,hugebits - PGBITS)*sizeof(bool));
  for(int i = 0; i < pow(2,hugebits-PGBITS); ++i){
    entry->footprint[i] = false;
  }

  return;
}

trans_entry
FootprintPred::init_trans_entry(void){
  trans_entry ret;
  ret.tag = 0;
  ret.valid = false;
  ret.clock_set = false;
  return ret;
}

Addr 
FootprintPred::get_tag(Addr addr){
  Addr result = addr;
  Addr tag_mask = 1;
  tag_mask = tag_mask << (addrbits - lg_fpsize - hugebits);
  tag_mask -= 1;
  result = result >> (lg_fpsize + hugebits);
  result = result & tag_mask;
  return result;
}

// Get tag of huge page for addr translation table
Addr 
FootprintPred::get_hugetag(Addr addr){
  Addr result = addr;
  Addr tag_mask = 1; 
  tag_mask = tag_mask << (addrbits - hugebits);
  tag_mask -= 1;
  result = result >> hugebits;
  result = result & tag_mask;
  return result;
}

Addr 
FootprintPred::get_index(Addr addr){
  Addr result = addr;
  Addr index_mask = 1; 
  index_mask = index_mask << lg_fpsize;
  index_mask -= 1;
  result = result >> hugebits;
  result = result & index_mask;
  return result;
}

Addr 
FootprintPred::get_small_pg(Addr addr){
  Addr result = addr;
  Addr pg_mask = 1; 
  pg_mask = pg_mask << (hugebits - PGBITS);
  pg_mask -= 1;
  result = result >> PGBITS;
  result = result & pg_mask;
  return result;
}
