/*
 * Footprint predictor for 3D DRAM
 * cs 752 fall 2015
 * written by Mitchell Manar - manarm@cs.wisc.edu
 */

#ifndef __FOOTPRINT_PRED_HH__
#define __FOOTPRINT_PRED_HH__

#include <stdint.h>

#include <cmath>
#include <cstdlib>
#include <string>

#include "base/random.hh"
#include "base/statistics.hh"
#include "base/types.hh"

#define PGBITS  12 // lg page size (4K)
#define TIMESLICE 10000 // Memory accesses bt swaps for hottest/coldest

//using namespace std;

//typedef uint64_t Addr;

enum Migrate_policy {HOTCOLD, FOOTPRINT};

struct fp_entry {
  Addr tag;
  bool valid;
  int footprint_sum;
  bool *footprint;
};

struct trans_entry {
  Addr tag;
  bool valid;
  bool clock_set;
};

class FootprintPred
{
  private:
    int fpsize, lg_fpsize, fpassoc, addrbits; //addrbits = number of bits in addr
    Addr mru; // track mru of main memory for hottest/coldest
    int hugesize, hugebits; // size of huge page in KB
    int ram_pages, ram_size; // # of huge pages in ram, size of ram in KB
    Migrate_policy policy;
    float threshold; // fraction of footprint touched before migration
    int clock_hand, hotCold_clock; // for pLRU and hottest/coldest timeslice
    Random random_gen;
    fp_entry ***footprint_table;
    trans_entry *trans_table;

    void init_fp_entry(fp_entry *entry);
    trans_entry init_trans_entry();
    Addr get_tag(Addr addr);
    Addr get_hugetag(Addr addr);
    Addr get_index(Addr addr);
    Addr get_small_pg(Addr addr);
    void record_access(Addr addr);
    void migrate_page(Addr addr);

    // Statistics
    //Stats::Vector vector;
    //Stats::Average avg;
    Stats::Scalar fprint_hits;
    Stats::Scalar fprint_misses;
    Stats::Scalar fastmem_hits;
    Stats::Scalar fastmem_misses;
    Stats::Scalar migrations_slow_to_fast;

  public:
    // Fsize = total number of ways in footprint pred
    // Fassoc = associativity of footprint pred
    // Hsize = size of a hugepage in KB
    // Ramsize = size of 3d ram in KB
    // Thresh = % 4K pages in a hugepage touched before migration
    // mpolicy = "footprint" or "hotcold" for migration policy
    FootprintPred(int fsize, int fassoc, int hsize, int ramsize, float thresh, std::string mpolicy);
    bool addr_inFastMem(Addr address);
    void regStats(); // function to init stats
};

#endif //__FOOTPRINT_PRED_HH__
