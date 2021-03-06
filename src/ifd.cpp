#include <iostream>
#include <list>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "ifd.h"

using namespace std;

/**
 * PTR_IN_RANGE - examine whether a pointer falls in [base, base + limit)
 * @param ptr:    the non-void* pointer to a single arbitrary-sized object.
 * @param base:   base address represented with char* type.
 * @param limit:  upper limit of the legal address.
 *
 */
#define PTR_IN_RANGE(ptr, base, limit)			\
	((const char *)(ptr) >= (base) &&		\
	 (const char *)&(ptr)[1] <= (base) + (limit))

static const struct region_name region_names[MAX_REGIONS] = {
	{ "Flash Descriptor", "FD"},
	{ "BIOS", 			"BIOS"}, 
	{ "Intel ME", 		  "ME"},
	{ "GbE", 			 "GbE"}, 
	{ "Platform Data",    "PD"}, 
	{ "Reserved_1",     "RES1"}, 
	{ "Reserved_2",     "RES2"}, 
	{ "Reserved_3",     "RES3"}, 
	{ "EC",               "EC"}, 
};

static char *bios_image;
static int  bios_size = -1;

void open_rom_to_memory(char* filename)
{
    int file_descriptor = -1;
    struct stat bios_buffer;
    
    cout << "Preparing to opening the rom: " << filename << endl;
	
    file_descriptor = open(filename, O_RDONLY | O_BINARY);
    if (file_descriptor == -1)
    {
       cout << "Could not open the rom file" << endl;
       exit(EXIT_FAILURE);
    }
	
	if (fstat(file_descriptor, &bios_buffer) == -1) {
		cout << "Could not stat the rom file" << endl;
		exit(EXIT_FAILURE);
	}
	
    bios_size = bios_buffer.st_size;
	cout << "Your ROM is " << bios_size/1024/1024 << "MB" 
		<< " end address at " << hex << showbase << bios_size << endl << endl;
    
	bios_image = new char[bios_size];
    if (!bios_image) {
		cout << "Out of memory" << endl;
		exit(EXIT_FAILURE);
	}

	if (read(file_descriptor, bios_image, bios_size) != bios_size) {
		cout << "Could not read file" << endl; 
		exit(EXIT_FAILURE);
	}

	close(file_descriptor);
	
    dump_layout();

    delete[] bios_image;
}

void dump_layout()
{
	const frba_t *frba = find_frba();
	if (!frba)
		exit(EXIT_FAILURE);

	dump_frba_layout(frba);
}

frba_t *find_frba()
{
	fdbar_t *fdb = find_flash_descriptor();
	if (!fdb)
		return NULL;
	frba_t *frba =
		(frba_t *) (bios_image + (((fdb->flmap0 >> 16) & 0xff) << 4));
	return PTR_IN_RANGE(frba, bios_image, bios_size) ? frba : NULL;
}

fdbar_t *find_flash_descriptor()
{
	int i, found = 0;

	/* Scan for FD signature */
	for (i = 0; i < (bios_size - 4); i += 4) {
		if (*(uint32_t *) (bios_image + i) == 0x0FF0A55A) {
			found = 1;
			break;	// signature found.
		}
	}

	if (!found) {
		printf("No Flash Descriptor found in this image\n");
		return NULL;
	}

	fdbar_t *fdb = (fdbar_t *) (bios_image + i);
	return PTR_IN_RANGE(fdb, bios_image, bios_size) ? fdb : NULL;
}

void dump_frba_layout(const frba_t *frba)
{
	unsigned int i;
	list<region_t> detected_regions;

	for (i = 0; i < MAX_REGIONS; i++) {
		region_t region = get_region(frba, i);
		/* is region invalid? */
		if (region.size < 1)
			continue;	
		detected_regions.push_back(region);
	}

	detected_regions.sort(compare_base);
	
	for(region_t var : detected_regions)
	{
		cout << "--------------" << " " << showbase << hex << var.base << endl;
		
		if(var.size < 10000){
			cout << var.name << " = " << noshowbase << var.size/1024 << "Kb" << endl;
		}
		else{
			cout << var.name << " = " << noshowbase << var.size/1024/1024 << "MB" << endl;
		}
		
		cout << "--------------" << " " << showbase << hex << var.limit << endl << endl;
	}

}

region_t get_region(const frba_t *frba, unsigned int region_type)
{
	int base_mask = 0x7fff; //0xfff  for IFD v1
	int limit_mask;
	uint32_t flreg;
	region_t region;

	limit_mask = base_mask << 16;

	if (region_type >= MAX_REGIONS) {
		cout << "Invalid region type: " << region_type << endl;
		exit (EXIT_FAILURE);
	}

	flreg = frba->flreg[region_type];
	region.base = (flreg & base_mask) << 12;
	region.limit = ((flreg & limit_mask) >> 4) | 0xfff;
	region.size = region.limit - region.base + 1;

	//test
	region.name = region_names[region_type].short_name;

	if (region.size < 0)
		region.size = 0;

	return region;
}

void dump_region_layout(unsigned int num, const frba_t *frba)
{
	region_t region = get_region(frba, num);

	printf("%08x:%08x %s\n", region.base, region.limit, region_name_short(num));
}

const char *region_name_short(unsigned int region_type)
{
	if (region_type >= MAX_REGIONS) {
		cout << "Invalid region type" << endl;
		exit (EXIT_FAILURE);
	}

	return region_names[region_type].short_name;
}

bool compare_base(region_t first_element, region_t second_element)
{
	 //First Argument Stays First (Return true)
    if (first_element.base < second_element.base) 
        return true; 
 
    //First Argument goes Next (Swap) (Return false)
    if (first_element.base > second_element.base) 
        return false;
 
    //a==b. First Argument Stays first 
    //(No need to Swap)
    return true;
}