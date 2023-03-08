// Name: Paul Kim 
// Netid: obawolo
// FALL 2020 CS 416 PROJECT 3
// cp.csrutgers.edu

#include "my_vm.h"

bool PhyMemSet = false; // Checks physical memeory initialization

int OffSetBit; // offset bits = log_2(Page Size)
int PgTableBit; // page table bits 
int PDirBit; // phyiscal memory directory bits 

int FrameNumber; // number of frame of the physical number 
int PgNumber;  // number of pages of the page map table 

pde_t maxBMapIndex;

void *mem_root = NULL;
void *phy_map = NULL;
void *vir_map = NULL;
void *pgdir_map = NULL;
pde_t *pgdir2 = NULL;

pthread_mutex_t mutex;
pthread_mutex_t mutex2;

bool needVirtual = false;

/*
Function responsible for allocating and setting your physical memory
*/
void SetPhysicalMem() {
        // Bits Calculation
        OffSetBit = (int)ceil(log2(PGSIZE));
        PgTableBit = (int)floor((32 - OffSetBit) / 2); // given using 32 bit address space 
        PDirBit = 32 - PgTableBit - OffSetBit; 

        // Frame Number of the physical memeory
        FrameNumber = (int)floor((double)(MEMSIZE)/(PGSIZE));

        // Page Number of the page table
        PgNumber = (int)ceil((double)(pow(2, PgTableBit+2)) / PGSIZE); 

        //Allocate physical memory using malloc; this is the total size of your memory you are simulating
        maxBMapIndex = ceil((double)FrameNumber / 8);
        while(!pgdir2){
            pgdir2 = (void*)malloc(ceil((double)(pow(2,PDirBit)*sizeof(pde_t))/8));
        }
        memset(pgdir2, 0, ceil((double)(pow(2,PDirBit)*sizeof(pde_t))/8));
        
        if(MEMSIZE > MAX_MEMSIZE){
            printf("ERROR: Defined memory size is greater than max memory size supported.\nSetting to max memory size of 4GB.\n");
            while(!mem_root){
            mem_root = (void*)malloc(ceil((double)(MAX_MEMSIZE)/8));
        }
        } else{
            while(!mem_root){
                mem_root = (void*)malloc(ceil((double)(MEMSIZE)/8));
            }
        }
        //HINT: Also calculate the number of physical and virtual pages and allocate
        //virtual and physical bitmaps and initialize them
        while(!phy_map){
            phy_map = (void*)malloc(ceil((double)FrameNumber/8));
        }
        memset(phy_map, 0, ceil((double)FrameNumber/8));
        while(!vir_map){
            vir_map = (void*)malloc(ceil((double)FrameNumber/8));
        }
        memset(vir_map, 0, ceil((double)FrameNumber/8));
        //initialize page directory bitmap
        while(!pgdir_map){
            pgdir_map = (void*)malloc(ceil((double)pow(2,PDirBit)/8));
        }
        memset(pgdir_map, 0, ceil((double)pow(2,PDirBit)/8));

        PhyMemSet = true;   // Physical memory is now set
 
}

/*
 * Part 2: Print TLB miss rate.
 * Feel free to extend the function arguments or return type.
 */
void
print_TLB_missrate()
{
    double miss_rate = 0;

    /*Part 2 Code here to calculate and print the TLB miss rate*/
    int tlbTotalAccess = tlbMiss + tlbHit;
    miss_rate = (double) tlbMiss / tlbTotalAccess;
    fprintf(stderr, "TLB miss rate of %d Bytes Page Size : %lf \n",PGSIZE, miss_rate);
}

// Starts the run time timer
void 
start_runtime() 
{
    gettimeofday(&start, NULL);
}

// Ends the run time timer and prints total runtime
void
end_runtime_print()
{
    // Ends the timer
    gettimeofday(&end, NULL);

    // Runtime alculation 
    long seconds = (end.tv_sec - start.tv_sec);
    long micros = (end.tv_usec - start.tv_usec);
    double tt = (seconds*1000000) + micros;

    // Printing the total runtime
    printf("Total Run time of %d Bytes Page Size: %g microseconds \n", PGSIZE, tt);
}


/*
The function takes a virtual address and page directories starting address and
performs translation to return the physical address
*/
pte_t * Translate(pde_t *pgdir, void *va) {
    //HINT: Get the Page directory index (1st level) Then get the
    //2nd-level-page table index using the virtual address.  Using the page
    //directory index and page table index get the physical address
    pte_t va_long = (long)va;
    pte_t offset_va = (va_long<<(PDirBit+PgTableBit)) >> (PDirBit+PgTableBit);
     //check if virtual address is taken
    //isolate pd+pt
    pte_t va_long_un = va_long>>(OffSetBit);
    pde_t tlb_index = va_long_un % (TLB_SIZE);

    //Checks the tlb first
    if(!needVirtual){
        if(tlb_store.entries[tlb_index].va == va_long_un && tlb_store.entries[tlb_index].valid == true){
            tlbHit++;
            return (pte_t*)(tlb_store.entries[tlb_index].pa + offset_va);
        }
    }

    pte_t index = floor((double)va_long_un / 8);
    pte_t offset = va_long_un % 8;
    //printf("VA: %lx, Index: %lu, Offset: %lu\n", va, index, offset);
    unsigned char * byte = ((unsigned char*)vir_map + index);
    if(!(*byte & (1 << offset))){
        printf("\nNo such address exists.\n");
        return NULL;
    }

    //save pd index
    pde_t pd_ind = va_long>>(OffSetBit+PgTableBit);
    //save pt index
    pte_t pt_ind = va_long<<(PDirBit);
    pt_ind = pt_ind>>(PDirBit + OffSetBit);
    //save offset
    pde_t off_ind = va_long<<(PDirBit + PgTableBit);
    off_ind = off_ind>>(PDirBit + PgTableBit);

    //get pd entry w/ pd index, check if valid, proceed
    pde_t index_pd = floor((double)pd_ind / 8);
    pde_t offset_pd = pd_ind % 8;
    unsigned char * byte_pd = ((char*)pgdir_map + index_pd);
    if(!(*byte_pd & (1 << offset_pd))){
            printf("\nInvalid Page Directory Index.\n");
            return NULL;
    }
    pde_t *pd_entry = pgdir + pd_ind;
    //get pt entry w/ pt index, check if valid, return address stored there
    //printf("PD entry: 0x%lx, index: %lu\n", pd_entry, pt_ind);
    pte_t *pt_entry = ((pte_t*)(mem_root + (((long)*(pd_entry) >> OffSetBit)*PGSIZE))) + pt_ind;
    if(!needVirtual){
        //printf("TLB Miss\n");
        tlbMiss++;
        tlb_store.entries[tlb_index].va = va_long_un;
        tlb_store.entries[tlb_index].pa = (pte_t)(mem_root + ((*pt_entry >> OffSetBit)*PGSIZE));
        tlb_store.entries[tlb_index].valid = true;
        return(pte_t*)((pte_t)(mem_root + ((*pt_entry >> OffSetBit)*PGSIZE))+offset_va);
    }
    //printf("\nPointer: %p\n", (void*)*pt_entry);
    return (pte_t*)(*pt_entry + offset_va);
}

/*
The function takes a page directory address, virtual address, physical address
as an argument, and sets a page table entry. This function will walk the page
directory to see if there is an existing mapping for a virtual address. If the
virtual address is not present, then a new entry will be added
*/
int
PageMap(pde_t *pgdir, void *va, void *pa)
{
    //printf("PGMAP\n");
    pde_t va_long = (long)va;
    pde_t pa_long = (long)pa;
    //printf("\nVA: %lx", va_long);
    //check if virtual address is taken
    //isolate pd+pt
    pde_t va_long_un = va_long>>(OffSetBit);
    pde_t index = floor((double)va_long_un / 8);
    pde_t offset = va_long_un % 8;
    unsigned char * byte = ((unsigned char*)vir_map + index);
    if(*byte & (1 << offset)){
        printf("\nvirtual address is taken\n");
        return -1;
    }

    
    //check if physical address is taken
    //isolate pd+pt
    pde_t pa_long_un = pa_long>>(OffSetBit);
    pde_t index_p = floor((double)pa_long_un / 8);
    pde_t offset_p = pa_long_un % 8;
    //printf("\nVA: %lx, Index: %lu, Offset %lu\n", va_long, index, offset);
    unsigned char * byte_p = ((char*)phy_map + index_p);
    if(*byte_p & (1 << offset_p)){
        printf("physical address is taken\n");
        return -1;
    }

    /*HINT: Similar to Translate(), find the page directory (1st level)
    and page table (2nd-level) indices. If no mapping exists, set the
    virtual to physical mapping */

        //save pd index
    pde_t pd_ind = va_long>>(OffSetBit+PgTableBit);
    //save pt index
    pte_t pt_ind = va_long<<(PDirBit);
    pt_ind = pt_ind>>(PDirBit + OffSetBit);
    //save offset
    pde_t off_ind = va_long<<(PDirBit + PgTableBit);
    off_ind = off_ind>>(PDirBit + PgTableBit);

    //printf("PD: %lx, PT: %lx, OFF: %lx\n", pd_ind, pt_ind, off_ind);
    //check if pd_ind is initialized
    pde_t index_pd = floor((double)pd_ind / 8);
    pde_t offset_pd = pd_ind % 8;
    unsigned char * byte_pd = ((char*)pgdir_map + index_pd);
    *byte_p = *byte_p | (1 << offset_p);
    if(!(*byte_pd & (1 << offset_pd))){
        //printf("initializing pd_entry %lu\n", pd_ind);
        //insert next available pfn(s) into pg_directory
        *(pgdir + pd_ind) = (pde_t)get_next_avail_phy_mult(PgNumber);
        //mark bitmap
        *byte_pd = *byte_pd | (1 << offset_pd);
    }
    //get pd entry w/ pd index, proceed
    pde_t *pd_entry = pgdir + pd_ind;
    //printf("\nPD entry: %lu\n", (long)*(pd_entry) >> OffSetBit);
    //get pt entry w/ pt index
    pte_t *pt_entry = ((pte_t*)(mem_root + (((long)*(pd_entry) >> OffSetBit)*PGSIZE))) + pt_ind;
    //printf("Loading: %p\n", (void*)pa);
    //load pa into pt entry
    *pt_entry = (pte_t)pa;
    *byte = *byte | (1 << offset);
    return 0;
}

/*Function that gets the next available page
*/
void *get_next_avail(int num_pages) {

    //Use virtual address bitmap to find the next free page
    //virtual addresses are contiguous, physical addresses, no need
    //scan virtual bitmap for num_pages contiguous free bits

    //for loop going through bitmap, bytewise
    pde_t i;
    pde_t j;
    unsigned char * byte;
    unsigned char bmask;
    pde_t offset;
    pde_t index;
    pde_t count;
    bool first = false;
    for(i = 0; i < maxBMapIndex; i++){
        byte = ((unsigned char*)vir_map + i);
        //for loop going through byte with 8 masks to count zeroes
        for(j = 0; j < 8; j++){
            bmask = (1 << j);
            if(((bmask ^ *byte) >> j) == 1){
                //on first zero, save offset & index
                if((!offset) && (!index) && !first){
                    index = i;
                    offset = j;
                    if(index == 0 && offset == 0){
                        first = true;
                    }
                }
                //keep going through, if count = num_pages, 
                count++;
                if(count == num_pages){
                    //mark bmask, return address of original offset
                    return (void*)(((index*8)+offset) << OffSetBit);
                }
            } else {
                //if a 1 is encountered, reset count and offsets
                //we need CONTIGUOUS pages
                //printf("No\n");
                count = 0;
                offset = (pde_t)NULL;
                index = (pde_t)NULL;
            }
            //if byte ends, keep count, offset & index and keep counting
        }
    }
    // printf("No available address space.\n");
    return NULL;    
}

/*Function that gets the next physical available page
*/
void *get_next_avail_phy(){
    //printf("\nGNAP\n");
    //scan phys bitmap, return first free physical address
    pde_t i;
    pde_t j;
    unsigned char * byte;
    unsigned char bmask;
    //loop through bitmap, byte by byte, look for one less than max value of byte
    //this indicates that there is at least one zero in it
    for(i = 0; i < maxBMapIndex; i++){
        byte = (unsigned char*)phy_map + i;
        //for loop going through byte with 8 masks to count zeroes
        for(j = 0; j < 8; j++){
            bmask = (1 << j);
            if(((bmask ^ *byte) >> j) == 1){
                //on first zero, return physical address w/ offset & index
                //printf("Assigning Index %lu, Offset %lu\n", i, j);
                return (void*)((((i*8)+j)) << OffSetBit);
            }
        }
    }
    printf("No available physical addresses.\n");
    return NULL;

}

void *get_next_avail_phy_mult(int num_pages) {
   //printf("\n\nGNAPM\n");
    //Use virtual address bitmap to find the next free page
    //virtual addresses are contiguous, physical addresses, no need
    //scan virtual bitmap for num_pages contiguous free bits

    //for loop going through bitmap, bytewise
    pde_t i;
    pde_t j;
    unsigned char * byte;
    unsigned char bmask;
    pde_t offset;
    pde_t index;
    pde_t count;
    bool first = false;
    for(i = 0; i < maxBMapIndex; i++){
        byte = ((unsigned char*)phy_map + i);
        //for loop going through byte with 8 masks to count zeroes
        for(j = 0; j < 8; j++){
            bmask = (1 << j);
            if(((bmask ^ *byte) >> j) == 1){
                //on first zero, save offset & index
                if((!offset) && (!index) && !first){
                    index = i;
                    offset = j;
                    if(index == 0 && offset == 0){
                        first = true;
                    }
                    //printf("here %lu %lu\n", index, offset);
                }
                *byte = *byte | (1 << j);
                //keep going through, if count = num_pages, 
                count++;
                if(count == num_pages){
                    //printf("Count: %lu, Index2: %lu, Offset2: %lu | %lu\n", count, i, j, ((index*8)+offset));
                    //mark bmask, return address of original offset
                    return (void*)(((index*8)+offset) << OffSetBit);
                }
            } else {
                //if a 1 is encountered, reset count and offsets
                //we need CONTIGUOUS pages
                //printf("No\n");
                count = 0;
                offset = (pde_t)NULL;
                index = (pde_t)NULL;
            }
            //if byte ends, keep count, offset & index and keep counting
        }
    }
    printf("No available address space.\n");
    return NULL;
}

/* Function responsible for allocating pages
and used by the benchmark
*/
void *myalloc(unsigned int num_bytes) {
    pthread_mutex_lock(&mutex);
    //printf("\nMALLOC\n");
    //HINT: If the physical memory is not yet initialized, then allocate and initialize.
    if(!PhyMemSet){
        SetPhysicalMem();
    }

    unsigned int num_pages = (int)ceil((double)num_bytes / (PGSIZE));
    void *va = get_next_avail(num_pages);
    pde_t va_long = (long)va;
    pde_t va_long_un = va_long>>(OffSetBit);
    pde_t index = floor((double)va_long_un / 8);
    pde_t offset = va_long_un % 8;
    pde_t count = num_pages;
    unsigned char * byte;

    void *va2 = (void*)(((long)va) + (long)num_bytes);

    pde_t va_long2 = (long)va2;
    pde_t va_long_un2 = va_long2>>(OffSetBit);
    pde_t index2 = floor((double)va_long_un2 / 8);
    pde_t offset2 = va_long_un2 % 8;

    //printf("VA: %lx, Count: %lu, Index: %lu, Offset: %lu\n", va, count, index, offset);
    while(index <= index2){
        if(index == index2 && offset > offset2){
            index = maxBMapIndex;
            offset = 0;
            break;
        }
        byte = ((unsigned char*)vir_map + index);
        if(*byte & (1 << offset)){
            printf("Range selected by get_next_avail is not free.\n");
            pthread_mutex_unlock(&mutex);
            return NULL;
        }
        PageMap(pgdir2, (void*)(((index*8)+offset) << OffSetBit), get_next_avail_phy());
        count--;
        if(count == 0){
            break;
        }
        offset++;
        if(offset == 8){
            offset = 0;
            index++;
        }
    }

    if(count != 0){
        printf("Range selected by get_next is invalid, vir_bitmap is now also invalid.\n");
        //printf("Count: %lu, Index: %lu, Offset: %lu\n", count, index, offset);
        pthread_mutex_unlock(&mutex);
        return NULL;
    }
   /* HINT: If the page directory is not initialized, then initialize the
   page directory. Next, using get_next_avail(), check if there are free pages. If
   free pages are available, set the bitmaps and map a new page. Note, you will 
   have to mark which physical pages are used. */
   //printf("malloced up to %lu %lu", index, offset);
    pthread_mutex_unlock(&mutex);
    return va;
}

/* Responsible for releasing one or more memory pages using virtual address (va)
*/
void myfree(void *va, int size) {
    pthread_mutex_lock(&mutex);
    //Free the page table entries starting from this virtual address (va)
    //Also mark the pages free in the bitmap
    //Only free if the memory from "va" to va+size is valid
    unsigned int num_pages = (int)ceil((double)size / (double)(PGSIZE));
    pde_t va_long = (long)va;
    pde_t va_long_un = va_long>>(OffSetBit);
    pde_t tlb_index = va_long_un % (TLB_SIZE);
    pde_t index = floor((double)va_long_un / 8);
    pde_t offset = va_long_un % 8;
    pde_t count = num_pages;
    unsigned char * byte;
    unsigned char bmask;

    void *va2 = (void*)(((long)va) + (long)size);

    pde_t va_long2 = (long)va2;
    pde_t va_long_un2 = va_long2>>(OffSetBit);
    pde_t index2 = floor((double)va_long_un2 / 8);
    pde_t offset2 = va_long_un2 % 8;

    pde_t pa_long;
    pde_t pa_long_un;
    pde_t index_p;
    pde_t offset_p;
    unsigned char * byte_p;
    unsigned char bmask_p;

    pde_t i;
    pde_t j;
    for(i = index; i <= index2; i++){
        byte = (unsigned char*)vir_map + i;
        //for loop going through byte with 8 masks to count zeroes
        for(j = (i == index) ? offset : 0; j < 8; j++){
            //printf("%lu %lu %lu\n",count, i ,j);
            bmask = (1 << j);
            if(i >= index2 && j > offset2){
                i = maxBMapIndex;
                j = 8;
                break;
            }
            if(((bmask & *byte) >> j) != 1){
                printf("This address is not allocated. %lu %lu %lu\n", count, i, j);
                pthread_mutex_unlock(&mutex);
                return;
            }
            //mark physical bitmap
            needVirtual = true;
            void *pa = Translate(pgdir2, (void*)(((i*8)+j) << OffSetBit));
            needVirtual = false;
            pa_long = (long)pa;
            pa_long_un = pa_long>>(OffSetBit);
            index_p = floor((double)pa_long_un / 8);
            offset_p = pa_long_un % 8;
            byte_p = (unsigned char*)phy_map + index_p;
            bmask_p = (1 << offset_p);
            *byte_p = *byte_p ^ (bmask_p);
            //mark virtual bitmap
            *byte = *byte ^ (bmask);
            //clear TLB entry if applicable
            if(tlb_store.entries[tlb_index].va == va_long_un && tlb_store.entries[tlb_index].valid == true){
                tlb_store.entries[tlb_index].va = (pde_t)NULL;
                tlb_store.entries[tlb_index].pa = (pde_t)NULL;
                tlb_store.entries[tlb_index].valid = false;
            }
            count--;
            if(count == 0){
                pthread_mutex_unlock(&mutex);
                return;
            }
        }
    }
}

/* The function copies data pointed by "val" to physical
 * memory pages using virtual address (va)
*/
void PutVal(void *va, void *val, int size) {

    /* HINT: Using the virtual address and translate(), find the physical page. Copy
       the contents of "val" to a physical page. NOTE: The "size" value can be larger
       than one page. Therefore, you may have to find multiple pages using translate()
       function.*/
    pthread_mutex_lock(&mutex);
    void *va2 = (void*)(((long)va) + (long)size);

    pde_t va_long = (long)va;
    pde_t va_long_un = va_long>>(OffSetBit);
    pde_t index = floor((double)va_long_un / 8);
    pde_t offset = va_long_un % 8;

    pde_t va_long2 = (long)va2;
    pde_t va_long_un2 = va_long2>>(OffSetBit);
    pde_t index2 = floor((double)va_long_un2 / 8);
    pde_t offset2 = (va_long_un2 % 8);

    pde_t i;
    pde_t j;
    unsigned char *byte;
    unsigned char bmask;

    //check validity of virtual address range
    for(i = index; i <= index2; i++){
        byte = ((unsigned char*)vir_map + i);
        //for loop going through byte with 8 masks to count zeroes
        for(j = (i == index) ? offset : 0; j < 8; j++){
            bmask = (1 << j);
            if(i >= index2 && j >= offset2){
                i = maxBMapIndex;
                j = 8;
                break;
            }
            if(((bmask & *byte) >> j) != 1){
                printf("Put value fail: invalid Index: %lu %lu+Offset: %lu %lu\n", i, index2, j, offset2);
                pthread_mutex_unlock(&mutex);
                return;
            }
        }
    }

    void *pa;
    void *va_inc = va;
    for(i=0; i < size; i++){
        va_inc = va + i;
        pa = Translate(pgdir2, va_inc);
        *(char*)pa = *(char*)val;
        val = (char*)val + 1;
    }
    pthread_mutex_unlock(&mutex);

}

/*Given a virtual address, this function copies the contents of the page to val*/
void GetVal(void *va, void *val, int size) {

    pthread_mutex_lock(&mutex);
    /* HINT: put the values pointed to by "va" inside the physical memory at given
    "val" address. Assume you can access "val" directly by derefencing them.
    If you are implementing TLB,  always check first the presence of translation
    in TLB before proceeding forward */

    void *va2 = (void*)(((long)va) + (long)size);

    pde_t va_long = (long)va;
    pde_t va_long_un = va_long>>(OffSetBit);
    pde_t index = floor((double)va_long_un / 8);
    pde_t offset = va_long_un % 8;

     pde_t va_long2 = (long)va2;
    pde_t va_long_un2 = va_long2>>(OffSetBit);
    pde_t index2 = floor((double)va_long_un2 / 8);
    pde_t offset2 = (va_long_un2 % 8);

    pde_t i;
    pde_t j;
    unsigned char *byte;
    unsigned char bmask;

    //check validity of virtual address range
    for(i = index; i <= index2; i++){
        byte = ((unsigned char*)vir_map + i);
        //for loop going through byte with 8 masks to count zeroes
        for(j = (i == index) ? offset : 0; j < 8; j++){
            bmask = (1 << j);
            if(i >= index2 && j >= offset2){
                i = maxBMapIndex;
                j = 8;
                break;
            }
            if(((bmask & *byte) >> j) != 1){
                printf("Get value fail: invalid address+range\n");
                pthread_mutex_unlock(&mutex);
                return;
            }
        }
    }

    void *pa;
    void *va_inc = va;
    for(i=0; i < size; i++){
        va_inc = va + i;
        pa = Translate(pgdir2, va_inc);
        //printf("PA: %p\n", pa);
        //printf("val: %c\n", *(char*)pa);
        *(char*)val = *(char*)pa;
        val = (char*)val + 1;
    }
    pthread_mutex_unlock(&mutex);

}

/*
This function receives two matrices mat1 and mat2 as an argument with size
argument representing the number of rows and columns. After performing matrix
multiplication, copy the result to answer.
*/
void MatMult(void *mat1, void *mat2, int size, void *answer) {
    /* Hint: You will index as [i * size + j] where  "i, j" are the indices of the
    matrix accessed. Similar to the code in test.c, you will use GetVal() to
    load each element and perform multiplication. Take a look at test.c! In addition to 
    getting the values from two matrices, you will perform multiplication and 
    store the result to the "answer array"*/
    int *mat1_val = (int*)malloc(sizeof(int));
    int *mat2_val = (int*)malloc(sizeof(int));
    int *mat_sum = (int*)malloc(sizeof(int));
    int i;
    int j;
    int k;
    for(i = 0; i < size; i++){
        for(j = 0; j < size; j++){
            *mat_sum = 0;
            for(k = 0; k < size; k++){
                GetVal((int*)mat1 + ((i*size)+k), (void*)mat1_val, sizeof(int));
                GetVal((int*)mat2 + ((k*size)+j), (void*)mat2_val, sizeof(int));
                *mat_sum +=  (*mat1_val) * (*mat2_val);
            }
            PutVal((int*)answer + ((i*size)+j), (void*)mat_sum, sizeof(int));
        }
    }  
}