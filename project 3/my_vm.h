// Name: Paul Kim 
// Netid: obawolo
// FALL 2020 CS 416 PROJECT 3
// cp.csrutgers.edu

#ifndef MY_VM_H_INCLUDED
#define MY_VM_H_INCLUDED
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

//Assume the address space is 32 bits, so the max memory size is 4GB
//Page size is 4KB

//Add any important includes here which you may need

#define PGSIZE 4096

// Maximum size of your memory
#define MAX_MEMSIZE 4ULL*1024*1024*1024 //4GB

#define MEMSIZE 1024*1024*1024

// Represents a page table entry
typedef unsigned long pte_t;

// Represents a page directory entry
typedef unsigned long pde_t;

#define TLB_SIZE 128

int tlbMiss; // TLB miss count
int tlbHit; // TLB hit count

// Run time calculation
struct timeval start;
struct timeval end;

//Structure to represents TLB
struct tlb {
    struct tlb_entry {
        unsigned long va;
        unsigned long pa;
        bool valid;
    } entries[TLB_SIZE];
    //Assume your TLB is a direct mapped TLB of TBL_SIZE (entries)
    // You must also define wth TBL_SIZE in this file.
    //Assume each bucket to be 4 bytes
}tlb;

struct tlb tlb_store;

void SetPhysicalMem();
pte_t* Translate(pde_t *pgdir, void *va);
int PageMap(pde_t *pgdir, void *va, void* pa);
bool check_in_tlb(void *va);
void put_in_tlb(void *va, void *pa);
void *myalloc(unsigned int num_bytes);
void myfree(void *va, int size);
void PutVal(void *va, void *val, int size);
void GetVal(void *va, void *val, int size);
void MatMult(void *mat1, void *mat2, int size, void *answer);
void print_TLB_missrate();
void *get_next_avail_phy_mult(int num_pages);
void start_runtime();
void end_runtime_print();

#endif
