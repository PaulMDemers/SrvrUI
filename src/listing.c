#include <srvros/desk_listing.h>
#include <srvros/desk_text.h>
#include <string.h>
#include <stdio.h>
int desk_listing_row_ids(const struct desk_file_entry *entries,unsigned count,uint64_t *ids) {
    if(count>DESK_FILE_ENTRIES || (count && (!entries || !ids))) return -1;
    unsigned order[DESK_FILE_ENTRIES],duplicates[DESK_FILE_ENTRIES],used_count=0;
    uint64_t used[DESK_FILE_ENTRIES*2];
    for(unsigned i=0;i<count;i++) {
        if(!entries[i].inode || !memchr(entries[i].name,0,sizeof(entries[i].name))) return -1;
        duplicates[i]=0;order[i]=i;ids[i]=0;
        for(unsigned j=0;j<i;j++) {
            if(!strcmp(entries[i].name,entries[j].name)) return -1;
            if(entries[i].inode==entries[j].inode) duplicates[i]=duplicates[j]=1;
        }
        unsigned at=0;while(at<used_count && used[at]<entries[i].inode) at++;
        if(at==used_count || used[at]!=entries[i].inode) {
            for(unsigned j=used_count;j>at;j--) used[j]=used[j-1];
            used[at]=entries[i].inode;used_count++;
        }
    }
    for(unsigned i=1;i<count;i++) {
        unsigned item=order[i],j=i;
        while(j && strcmp(entries[item].name,entries[order[j-1]].name)<0) { order[j]=order[j-1];j--; }
        order[j]=item;
    }
    for(unsigned n=0;n<count;n++) {
        unsigned i=order[n];
        if(!duplicates[i]) { ids[i]=entries[i].inode;continue; }
        uint64_t key=14695981039346656037ull;
        for(unsigned b=0;b<8;b++) { key^=(entries[i].inode>>(b*8))&255u;key*=1099511628211ull; }
        for(const unsigned char *p=(const unsigned char *)entries[i].name;*p;p++) { key^=*p;key*=1099511628211ull; }
        if(!key) key=1;
        /* Find the first free key, wrapping past UINT64_MAX to one. The sorted
         * occupied set makes collision resolution linear in the listing size. */
        for(unsigned pass=0;pass<2;pass++) {
            int wrapped=0;
            for(unsigned j=0;j<used_count;j++) {
                if(used[j]<key) continue;
                if(used[j]>key) break;
                if(++key==0) { key=1;wrapped=1;break; }
            }
            if(!wrapped) break;
        }
        unsigned at=0;while(at<used_count && used[at]<key) at++;
        for(unsigned j=used_count;j>at;j--) used[j]=used[j-1];
        used[at]=key;used_count++;
        ids[i]=key;
    }
    return 0;
}
int desk_listing_decode(struct desk_file_entry *entries,struct desk_list_item *items,
    char details[][32],const void *data,size_t length,unsigned count) {
    if(!entries || !items || !details || (count && !data) || count>DESK_FILE_ENTRIES || length!=count*sizeof(*entries)) return -1;
    const unsigned char *bytes=data;
    for(unsigned i=0;i<count;i++) {
        struct desk_file_entry entry;memcpy(&entry,bytes+i*sizeof(entry),sizeof(entry));
        if(!memchr(entry.name,0,sizeof(entry.name)) || entry.reserved || entry.directory>1 || !entry.name[0] || !entry.inode ||
           strchr(entry.name,'/') || !strcmp(entry.name,".") || !strcmp(entry.name,"..") || !desk_text_utf8_valid(entry.name,strlen(entry.name))) return -1;
        for(unsigned j=0;j<i;j++) { struct desk_file_entry previous;memcpy(&previous,bytes+j*sizeof(previous),sizeof(previous));if(!strcmp(previous.name,entry.name)) return -1; }
        for(const char *p=entry.name;*p;p++) if((unsigned char)*p<32 || (unsigned char)*p==127) return -1;
    }
    if(count) memmove(entries,data,count*sizeof(*entries));
    for(unsigned i=1;i<count;i++) {
        struct desk_file_entry item=entries[i];unsigned j=i;
        while(j && (item.directory>entries[j-1].directory ||
            (item.directory==entries[j-1].directory && strcmp(item.name,entries[j-1].name)<0))) {
            entries[j]=entries[j-1];j--;
        }
        entries[j]=item;
    }
    uint64_t ids[DESK_FILE_ENTRIES];
    if(desk_listing_row_ids(entries,count,ids)) return -1;
    for(unsigned i=0;i<count;i++) {
        if(entries[i].directory) strcpy(details[i],"Folder");
        else snprintf(details[i],32,"%llu bytes",(unsigned long long)entries[i].size);
        items[i]=(struct desk_list_item){entries[i].name,details[i],ids[i],entries[i].directory};
    }
    return (int)count;
}
