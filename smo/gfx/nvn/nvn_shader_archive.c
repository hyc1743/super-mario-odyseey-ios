#include "nvn_shader_archive.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#define MAX_ENTRIES 65536u
#define MAX_MEMBER (16u*1024u*1024u)
typedef struct { char name[64]; long offset; uint32_t size,crc; } member;
static pthread_mutex_t lock=PTHREAD_MUTEX_INITIALIZER;
static FILE *current;
static member *members;
static unsigned member_count;
static uint32_t le32(const unsigned char *b) {
    return b[0]|(uint32_t)b[1]<<8|(uint32_t)b[2]<<16|(uint32_t)b[3]<<24;
}
static int compare(const void *a,const void *b) { return strcmp(((const member *)a)->name,((const member *)b)->name); }
static uint32_t checksum(const unsigned char *p,size_t size) {
    uint32_t c=~0u;
    for(size_t i=0;i<size;i++){
        c^=p[i];for(unsigned j=0;j<8;j++)c=(c>>1)^(0xedb88320u&-(c&1u));
    }
    return ~c;
}
int nvn_shader_archive_open(const char *path) {
    FILE *f=path?fopen(path,"rb"):NULL;
    if(!f)return -1;
    member *entries=calloc(MAX_ENTRIES,sizeof *entries);
    if(!entries){fclose(f);return -1;}
    unsigned count=0;unsigned char header[12];uint64_t total=0;int complete=0;
    if(fseek(f,0,SEEK_END))goto invalid;
    long end=ftell(f);if(end<18||fseek(f,0,SEEK_SET))goto invalid;
    if(fread(header,1,6,f)!=6||memcmp(header,"SMOA\0\1",6))goto invalid;
    while(ftell(f)<=end-12){
        if(fread(header,1,4,f)!=4)goto invalid;
        if(!memcmp(header,"END!",4)){
            if(fread(header,1,8,f)!=8||ftell(f)!=end)goto invalid;
            uint64_t stored=le32(header)|(uint64_t)le32(header+4)<<32;
            if(stored!=total||!count)goto invalid;
            complete=1;break;
        }
        if(fread(header+4,1,6,f)!=6||count==MAX_ENTRIES)goto invalid;
        unsigned namesize=header[0]|(unsigned)header[1]<<8;
        uint32_t size=le32(header+2),crc=le32(header+6);
        if(!namesize||namesize>=sizeof entries[0].name||size>MAX_MEMBER)goto invalid;
        member *item=&entries[count];
        if(fread(item->name,1,namesize,f)!=namesize)goto invalid;
        for(unsigned i=0;i<namesize;i++){
            unsigned char c=(unsigned char)item->name[i];
            if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='_'||c=='-'))goto invalid;
        }
        if(!strcmp(item->name,".")||!strcmp(item->name,".."))goto invalid;
        long offset=ftell(f);
        if(offset<0||offset>end-12||(uint64_t)size>(uint64_t)(end-12-offset))goto invalid;
        item->offset=offset;item->size=size;item->crc=crc;total+=size;count++;
        if(fseek(f,(long)size,SEEK_CUR))goto invalid;
    }
    if(!complete)goto invalid;
    qsort(entries,count,sizeof *entries,compare);
    for(unsigned i=1;i<count;i++)if(!compare(&entries[i-1],&entries[i]))goto invalid;
    pthread_mutex_lock(&lock);
    if(current)fclose(current);
    free(members);current=f;members=entries;member_count=count;
    pthread_mutex_unlock(&lock);
    return (int)count;
invalid:
    free(entries);fclose(f);return -1;
}
int nvn_shader_archive_active(void) {
    pthread_mutex_lock(&lock);int active=current!=NULL;pthread_mutex_unlock(&lock);return active;
}
void *nvn_shader_archive_read(const char *name,size_t *size) {
    if(size)*size=0;
    if(!name||strlen(name)>=sizeof members[0].name)return NULL;
    member key={0};strcpy(key.name,name);
    pthread_mutex_lock(&lock);
    member *item=current?bsearch(&key,members,member_count,sizeof *members,compare):NULL;
    unsigned char *data=item?malloc((size_t)item->size+1):NULL;
    if(data){
        if(fseek(current,item->offset,SEEK_SET)||fread(data,1,item->size,current)!=item->size||
           checksum(data,item->size)!=item->crc){free(data);data=NULL;}
        else {data[item->size]=0;if(size)*size=item->size;}
    }
    pthread_mutex_unlock(&lock);return data;
}
void nvn_shader_archive_close(void) {
    pthread_mutex_lock(&lock);
    if(current)fclose(current);
    current=NULL;free(members);members=NULL;member_count=0;
    pthread_mutex_unlock(&lock);
}
