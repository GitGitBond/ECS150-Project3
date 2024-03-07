#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#include "disk.h"
#include "fs.h"

/* TODO: Phase 1 */
typedef struct __attribute__((__packed__)){
    char signature[8];
    uint16_t total_blocks_amount;
    uint16_t root_dirctory_block_index;
    uint16_t data_block_start_index;
    uint16_t data_blocks_amount;
    uint8_t fat_blocks_number;
    char padding[4079];
}super_block_t;

typedef struct __attribute__((__packed__)){
    char filename[16];
    uint32_t file_size;
    uint16_t first_data_block_index;
    char padding[10];
}entry_t;

typedef struct{
    entry_t* file;
    uint32_t file_offset;
}file_descriptor_t;

super_block_t* super_block=NULL;
uint16_t* file_allocation_table=NULL;
entry_t* root_directory=NULL;
file_descriptor_t opened_files[FS_OPEN_MAX_COUNT];

int check_signature();
int get_open_files_count();
int get_free_data_blocks_count();
int get_files_count(entry_t* entry);
int look_for_one_free_data_block(int start_index);
int get_data_block_index_by_offset(file_descriptor_t* file);
void free_all();

int fs_mount(const char *diskname)
{
	/* TODO: Phase 1 */
	if(block_disk_open(diskname)==-1){
        return -1;
	}
	super_block=(super_block_t*)malloc(sizeof(super_block_t));
	block_read(0,super_block);
    if(check_signature()==-1){

        return -1;
    }
    if(super_block->total_blocks_amount!=block_disk_count()){
        free_all();
        return -1;
    }
    if(super_block->total_blocks_amount!=1+super_block->fat_blocks_number+1+super_block->data_blocks_amount){
        free_all();
        return -1;
    }
    root_directory=(entry_t*)malloc(sizeof(entry_t)*FS_FILE_MAX_COUNT);
    block_read(super_block->root_dirctory_block_index,root_directory);
    file_allocation_table=(uint16_t*)malloc(super_block->fat_blocks_number*BLOCK_SIZE);

    for(int i=0;i<super_block->fat_blocks_number;i++){
        block_read(1+i,file_allocation_table+BLOCK_SIZE/2*i);
    }
    for(int i=0;i<FS_OPEN_MAX_COUNT;i++){
        opened_files[i].file=NULL;
    }
    return 0;
}

int fs_umount(void)
{
	/* TODO: Phase 1 */
	if(super_block==NULL){
        return -1;
	}
	if(get_open_files_count()>0){
        return -1;
	}
	if(block_write(0,super_block)==-1){
        return -1;
	}
	for(int i=0;i<super_block->fat_blocks_number;i++){
        block_write(i+1,file_allocation_table+BLOCK_SIZE/2*i);
	}
	block_write(super_block->root_dirctory_block_index,root_directory);
	block_disk_close();
	free_all();
	return 0;
}

int fs_info(void)
{
	/* TODO: Phase 1 */
	printf("FS Info:\n");
	printf("total_blk_count=%d\n",super_block->total_blocks_amount);
	printf("fat_blk_count=%d\n",super_block->fat_blocks_number);
	printf("rdir_blk=%d\n",super_block->root_dirctory_block_index);
	printf("data_blk=%d\n",super_block->data_block_start_index);
	printf("data_blk_count=%d\n",super_block->data_blocks_amount);
	printf("fat_free_ratio=%d/%d\n",get_free_data_blocks_count(),super_block->data_blocks_amount);
	printf("rdir_free_ratio=%d/%d\n",get_files_count(root_directory),FS_FILE_MAX_COUNT);
	return 0;
}

int fs_create(const char *filename)
{
	/* TODO: Phase 2 */
    if(strlen(filename)+1>=FS_FILENAME_LEN){
        return -1;
    }
    int index=-1;
    for(int i=0;i<FS_FILE_MAX_COUNT;i++){
        if(root_directory[i].filename[0]=='\0'&&index==-1){
            index=i;
        }
        if(strcmp(filename,root_directory[i].filename)==0){
            return -1;
        }
    }
    if(index==-1){
        return -1;
    }
    strcpy(root_directory[index].filename,filename);
    root_directory[index].file_size=0;
    root_directory[index].first_data_block_index=0xFFFF;
    return 0;
}

int fs_delete(const char *filename)
{
	/* TODO: Phase 2 */
	for(int i=0;i<FS_OPEN_MAX_COUNT;i++){
        if(opened_files[i].file!=NULL&&strcmp(opened_files[i].file->filename,filename)==0){
            return -1;
        }
	}
	int index=-1;
	for(int i=0;i<FS_FILE_MAX_COUNT;i++){
        if(strcmp(root_directory[i].filename,filename)==0){
            index=i;
            break;
        }
	}
	if(index==-1){
        return -1;
	}
	root_directory[index].filename[0]='\0';
	uint16_t data_index=root_directory[index].first_data_block_index;
	while(data_index!=0xFFFF){
        uint16_t next_index=file_allocation_table[data_index];
        file_allocation_table[data_index]=0;
        data_index=next_index;
	}
	return 0;
}

int fs_ls(void)
{
	/* TODO: Phase 2 */
	if(super_block==NULL){
        return -1;
	}
	printf("FS Ls:\n");
	for(int i=0;i<FS_FILE_MAX_COUNT;i++){
        if(root_directory[i].filename[0]!='\0'){
            printf("file: %s, size: %d, data_blk: %d\n",root_directory[i].filename,root_directory[i].file_size,root_directory[i].first_data_block_index);
        }
	}
	return 0;
}

int fs_open(const char *filename)
{
	/* TODO: Phase 3 */
	if(super_block==NULL){
        return -1;
	}
	int index_in_root=-1;
	for(int i=0;i<FS_FILE_MAX_COUNT;i++){
        if(strcmp(root_directory[i].filename,filename)==0){
            index_in_root=i;
            break;
        }
	}
	if(index_in_root==-1){
        return -1;
	}
	for(int i=0;i<FS_OPEN_MAX_COUNT;i++){
        if(opened_files[i].file!=NULL&&strcmp(opened_files[i].file->filename,filename)==0){
            return -1;
        }
	}
	int fd=-1;
	for(int i=0;i<FS_OPEN_MAX_COUNT;i++){
        if(opened_files[i].file==NULL){
            fd=i;
            opened_files[i].file=&root_directory[index_in_root];
            opened_files[i].file_offset=0;
            break;
        }
	}
	return fd;
}

int fs_close(int fd)
{
	/* TODO: Phase 3 */
	if(super_block==NULL){
        return -1;
	}
	if(fd<0||fd>=FS_OPEN_MAX_COUNT){
        return -1;
	}
	if(opened_files[fd].file==NULL){
        return -1;
	}
	opened_files[fd].file=NULL;
	return 0;
}

int fs_stat(int fd)
{
	/* TODO: Phase 3 */
	if(super_block==NULL){
        return -1;
	}
	if(fd<0||fd>=FS_OPEN_MAX_COUNT){
        return -1;
	}
	if(opened_files[fd].file==NULL){
        return -1;
	}
	return opened_files[fd].file->file_size;
}

int fs_lseek(int fd, size_t offset)
{
	/* TODO: Phase 3 */
	if(super_block==NULL){
        return -1;
	}
	if(fd<0||fd>=FS_OPEN_MAX_COUNT){
        return -1;
	}
	if(opened_files[fd].file==NULL){
        return -1;
	}
    if(offset>opened_files[fd].file->file_size){
        return -1;
    }
    opened_files[fd].file_offset=offset;
    return 0;
}

int fs_write(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
    if(super_block==NULL){
        return -1;
	}
	if(fd<0||fd>=FS_OPEN_MAX_COUNT){
        return -1;
	}
	if(opened_files[fd].file==NULL){
        return -1;
	}
	if(buf==NULL){
        return -1;
	}
	int block_index=-1;
	if(opened_files[fd].file->first_data_block_index==0xffff){
        block_index=look_for_one_free_data_block(0);
        if(block_index==-1){
            return 0;
        }
        opened_files[fd].file->first_data_block_index=block_index;
        file_allocation_table[block_index]=0xffff;
	}
    uint32_t current_offset=opened_files[fd].file_offset;
    block_index=get_data_block_index_by_offset(&opened_files[fd]);

    int last_block_index;
    int total_write=0;

    char buffer[BLOCK_SIZE];
    block_read(block_index,buffer);
    int first_write=(count<BLOCK_SIZE-current_offset%BLOCK_SIZE)?count:(BLOCK_SIZE-current_offset);
    memcpy(buffer+current_offset%BLOCK_SIZE,buf,first_write);
    block_write(block_index,buffer);
    total_write+=first_write;

    char* p=(char*)buf;
    p+=total_write;
    count-=total_write;
    current_offset+=total_write;

    last_block_index=block_index;
    while(count>0){
        block_index=file_allocation_table[last_block_index];
        if(block_index==0xffff){
            block_index=look_for_one_free_data_block(0);
            if(block_index==-1){
                break;
            }
            file_allocation_table[last_block_index]=block_index;
            file_allocation_table[block_index]=0xffff;
        }
        if(count>=BLOCK_SIZE){
            block_write(block_index,p);
            p+=BLOCK_SIZE;
            count-=BLOCK_SIZE;
            current_offset+=BLOCK_SIZE;
            total_write+=BLOCK_SIZE;
        }
        else{
            memset(buffer,0,sizeof(char)*BLOCK_SIZE);
            memcpy(buffer,p,count);
            block_write(block_index,buffer);
            current_offset+=count;
            total_write+=count;
            count-=count;
        }
        last_block_index=block_index;
    }
    opened_files[fd].file_offset=current_offset;
    if(current_offset>=opened_files[fd].file->file_size){
        opened_files[fd].file->file_size=current_offset;
    }
    return total_write;
}

int fs_read(int fd, void *buf, size_t count)
{
	/* TODO: Phase 4 */
    if(super_block==NULL){
        return -1;
	}
	if(fd<0||fd>=FS_OPEN_MAX_COUNT){
        return -1;
	}
	if(opened_files[fd].file==NULL){
        return -1;
	}
	if(buf==NULL){
        return -1;
	}
	if(opened_files[fd].file->first_data_block_index==0xffff){
        return 0;
	}
	char buffer[BLOCK_SIZE];
    uint32_t current_offset=opened_files[fd].file_offset;
    int block_index=get_data_block_index_by_offset(&opened_files[fd]);
    block_read(block_index,buffer);
    if(count<=BLOCK_SIZE-current_offset%BLOCK_SIZE){
        memcpy(buf,buffer+current_offset%BLOCK_SIZE,count);
        opened_files[fd].file_offset+=count;
        return count;
    }
    int total_read=0;
    char* p=(char*)buf;
    memcpy(p,buffer+current_offset%BLOCK_SIZE,BLOCK_SIZE-current_offset%BLOCK_SIZE);
    total_read+=BLOCK_SIZE-current_offset%BLOCK_SIZE;
    p+=total_read;
    count-=total_read;
    block_index=file_allocation_table[block_index];
    while(count>0&&block_index!=0xffff){
        if(count>=BLOCK_SIZE){
            block_read(block_index,p);
            p+=BLOCK_SIZE;
            total_read+=BLOCK_SIZE;
            count-=BLOCK_SIZE;
        }
        else{
            block_read(block_index,buffer);
            memcpy(p,buffer,sizeof(char)*count);
            total_read+=count;
            count-=count;
        }
        block_index=file_allocation_table[block_index];
    }
    opened_files[fd].file_offset+=total_read;
    return total_read;
}

int check_signature(){
    char* signature="ECS150FS";
    for(int i=0;i<8;i++){
        if(super_block->signature[i]!=signature[i]){
            return -1;
        }
    }
    return 0;
}
int get_open_files_count(){
    int count=0;
    for(int i=0;i<FS_OPEN_MAX_COUNT;i++){
        if(opened_files[i].file!=NULL){
            count++;
        }
    }
    return 0;
}
int get_free_data_blocks_count(){
    int count=0;
    for(int i=0;i<super_block->data_blocks_amount;i++){
        if(file_allocation_table[i]==0){
            count++;
        }
    }
    return count;
}
int get_files_count(entry_t* entry){
    int count=0;
    for(int i=0;i<FS_FILE_MAX_COUNT;i++){
        if(entry[i].filename[0]!='\0'){
            count++;
        }
    }
    return count;
}
int look_for_one_free_data_block(int start_index){
    for(int i=start_index;i<super_block->data_blocks_amount;i++){
        if(file_allocation_table[i]==0){
            return i;
        }
    }
    return -1;
}
int get_data_block_index_by_offset(file_descriptor_t* file){
    uint32_t offset=file->file_offset;
    int index=file->file->first_data_block_index;

    int blocks=offset/BLOCK_SIZE;
    for(int i=0;i<blocks;i++){
        index=file_allocation_table[index];
    }
    return index;
}
void free_all(){
    if(super_block!=NULL){
        free(super_block);
    }
    if(root_directory!=NULL){
        free(root_directory);
    }
    if(file_allocation_table!=NULL){
        free(file_allocation_table);
    }
    super_block=NULL;
    root_directory=NULL;
    file_allocation_table=NULL;
}
