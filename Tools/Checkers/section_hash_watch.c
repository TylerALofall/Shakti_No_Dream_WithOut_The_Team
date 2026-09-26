/*
 * section_hash_watch.c — C99, POSIX directory access, no heap allocation.
 *
 * OUTLINE / DEFINITIONS
 * ROOT         directory containing SECTION-A ... SECTION-O, or SECTION—A ... SECTION—O.
 * SECTION      one of those 15 directories; nested directories are included.
 * SOURCE       every regular .c/.h file; --all-files includes other regular files.
 * SECTION_LOG  ROOT/.00/SECTION-X_LOG.txt (append-only CSV).
 * MASTER       ROOT/SECTION_MASTER_LOG.txt (append-only CSV, changes only).
 *
 * Build: cc -std=c99 -O2 -Wall -Wextra -Werror -o section_hash_watch section_hash_watch.c
 * Use:   ./section_hash_watch /path/to/ROOT [--all-files]
 * Check: ./section_hash_watch --self-test
 *
 * The file contents are only read with fread(), never parsed as text.
 * CSV paths use percent-encoding for unsafe filename bytes, so commas,
 * newlines and percent signs cannot corrupt a record. Dates are UTC.
 * Limits are explicit; exceeding them stops the scan before log writes.
 */
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define MAX_FILES 4096
#define PATH_CAP 1024
#define KEY_CAP (PATH_CAP * 3)
#define LINE_CAP (KEY_CAP + 256)
#define MAX_DEPTH 32
#define SECTIONS 15

typedef struct { uint32_t h[8]; unsigned char block[64]; size_t used; uint64_t bytes; } Sha;
static const uint32_t k[64] = {
    0x428a2f98U,0x71374491U,0xb5c0fbcfU,0xe9b5dba5U,0x3956c25bU,0x59f111f1U,0x923f82a4U,0xab1c5ed5U,
    0xd807aa98U,0x12835b01U,0x243185beU,0x550e7dc3U,0x72be5d74U,0x80deb1feU,0x9bdc06a7U,0xc19bf174U,
    0xe49b69c1U,0xefbe4786U,0x0fc19dc6U,0x240ca1ccU,0x2de92c6fU,0x4a7484aaU,0x5cb0a9dcU,0x76f988daU,
    0x983e5152U,0xa831c66dU,0xb00327c8U,0xbf597fc7U,0xc6e00bf3U,0xd5a79147U,0x06ca6351U,0x14292967U,
    0x27b70a85U,0x2e1b2138U,0x4d2c6dfcU,0x53380d13U,0x650a7354U,0x766a0abbU,0x81c2c92eU,0x92722c85U,
    0xa2bfe8a1U,0xa81a664bU,0xc24b8b70U,0xc76c51a3U,0xd192e819U,0xd6990624U,0xf40e3585U,0x106aa070U,
    0x19a4c116U,0x1e376c08U,0x2748774cU,0x34b0bcb5U,0x391c0cb3U,0x4ed8aa4aU,0x5b9cca4fU,0x682e6ff3U,
    0x748f82eeU,0x78a5636fU,0x84c87814U,0x8cc70208U,0x90befffaU,0xa4506cebU,0xbef9a3f7U,0xc67178f2U
};
static uint32_t rr(uint32_t n, unsigned s) { return (n >> s) | (n << (32U-s)); }
static void sha_block(Sha *s, const unsigned char *p) {
    uint32_t w[64], a,b,c,d,e,f,g,h,t1,t2;
    unsigned i;
    for(i=0;i<16;i++) w[i]=((uint32_t)p[i*4]<<24)|((uint32_t)p[i*4+1]<<16)|((uint32_t)p[i*4+2]<<8)|p[i*4+3];
    for(i=16;i<64;i++) w[i]=(rr(w[i-2],17)^rr(w[i-2],19)^(w[i-2]>>10))+w[i-7]+(rr(w[i-15],7)^rr(w[i-15],18)^(w[i-15]>>3))+w[i-16];
    a=s->h[0];b=s->h[1];c=s->h[2];d=s->h[3];e=s->h[4];f=s->h[5];g=s->h[6];h=s->h[7];
    for(i=0;i<64;i++) {
        t1=h+(rr(e,6)^rr(e,11)^rr(e,25))+((e&f)^(~e&g))+k[i]+w[i];
        t2=(rr(a,2)^rr(a,13)^rr(a,22))+((a&b)^(a&c)^(b&c));
        h=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;
    }
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;
    s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void sha_init(Sha *s) {
    static const uint32_t iv[8]={0x6a09e667U,0xbb67ae85U,0x3c6ef372U,0xa54ff53aU,0x510e527fU,0x9b05688cU,0x1f83d9abU,0x5be0cd19U};
    memcpy(s->h,iv,sizeof iv);s->used=0;s->bytes=0;
}
static int sha_add(Sha *s,const unsigned char *p,size_t n) {
    size_t take;
    if(n>(UINT64_MAX/8U)-s->bytes) return -1;
    s->bytes+=(uint64_t)n;
    while(n) {
        take=64-s->used;if(take>n) take=n;
        memcpy(s->block+s->used,p,take);s->used+=take;p+=take;n-=take;
        if(s->used==64) {sha_block(s,s->block);s->used=0;}
    }
    return 0;
}
static void sha_end(Sha *s,char out[65]) {
    static const char hex[]="0123456789abcdef";
    uint64_t bits=s->bytes*8U;unsigned i,j;
    s->block[s->used++]=0x80;
    if(s->used>56) {while(s->used<64)s->block[s->used++]=0;sha_block(s,s->block);s->used=0;}
    while(s->used<56)s->block[s->used++]=0;
    for(i=0;i<8;i++)s->block[56+i]=(unsigned char)(bits>>(56-8*i));
    sha_block(s,s->block);
    for(i=0;i<8;i++)for(j=0;j<4;j++) {
        unsigned char v=(unsigned char)(s->h[i]>>(24-8*j));
        out[i*8+j*2]=hex[v>>4];out[i*8+j*2+1]=hex[v&15];
    }
    out[64]=0;
}
static int self_test(void) {
    static const char *v[]={"","abc","The quick brown fox jumps over the lazy dog"};
    static const char *want[]={
      "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
      "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
      "d7a8fbb307d7809469ca9abcb0082e4f8d5651e46d3cdb762d02d0bf37c9e592"
    }; Sha s;char out[65];size_t i;
    for(i=0;i<3;i++) {sha_init(&s);if(sha_add(&s,(const unsigned char*)v[i],strlen(v[i])))return -1;sha_end(&s,out);if(strcmp(out,want[i]))return -1;}
    return 0;
}

typedef struct {
    char key[KEY_CAP], path[PATH_CAP], previous[65], current[65];
    char section_change[160], master_change[160], status[12], delta[24];
    Sha section_history, master_history;
    uint64_t old_bytes, bytes;
    int section, had_previous, was_removed, seen;
} Item;
static Item items[MAX_FILES];
static size_t used;
static char root[PATH_CAP], date_utc[11];
static char section_names[SECTIONS][16];
static int include_all;

static int fail(const char *what,const char *path) {
    fprintf(stderr,"ERROR: %s: %s\n",what,path?path:"(none)");return -1;
}
static int join(char *out,size_t cap,const char *a,const char *b) {
    int n=snprintf(out,cap,"%s/%s",a,b);return n<0||(size_t)n>=cap?-1:0;
}
static int encode_key(const char *src,char dst[KEY_CAP]) {
    static const char hex[]="0123456789ABCDEF";size_t p=0;unsigned char c;
    while((c=(unsigned char)*src++)!=0) {
        if(c==0xE2&&(unsigned char)src[0]==0x80&&(unsigned char)src[1]==0x94) {
            if(p+3>=KEY_CAP)return -1;
            dst[p++]=(char)c;dst[p++]=*src++;dst[p++]=*src++;
            continue;
        }
        if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='/'||c=='.'||c=='_'||c=='-') {
            if(p+1>=KEY_CAP)return -1;
            dst[p++]=(char)c;
        } else {
            if(p+3>=KEY_CAP)return -1;
            dst[p++]='%';dst[p++]=hex[c>>4];dst[p++]=hex[c&15];
        }
    }
    dst[p]=0;return 0;
}
static int section_of(const char *key) {
    if(!strncmp(key,"SECTION-",8)&&key[8]>='A'&&key[8]<='O'&&key[9]=='/')
        return key[8]-'A';
    if(!strncmp(key,"SECTION\xE2\x80\x94",10)&&key[10]>='A'&&key[10]<='O'&&key[11]=='/')
        return key[10]-'A';
    return -1;
}
static Item *get_item(const char *key,int section) {
    size_t i;
    for(i=0;i<used;i++)if(!strcmp(items[i].key,key))return &items[i];
    if(used==MAX_FILES||strlen(key)>=KEY_CAP)return NULL;
    i=used++;strcpy(items[i].key,key);items[i].section=section;
    sha_init(&items[i].section_history);sha_init(&items[i].master_history);
    return &items[i];
}
static int parse_uint(const char *p,uint64_t *out) {
    const char *start=p;
    char *end;unsigned long long v;
    if(!*p)return -1;
    while(*p) {if(!isdigit((unsigned char)*p++))return -1;}
    errno=0;v=strtoull(start,&end,10);
    if(errno||*end)return -1;
    *out=(uint64_t)v;return 0;
}
static int valid_sha(const char *p) {
    size_t i;if(strlen(p)!=64)return 0;
    for(i=0;i<64;i++)if(!((p[i]>='0'&&p[i]<='9')||(p[i]>='a'&&p[i]<='f')))return 0;
    return 1;
}
static int split(char *line,char **parts,int count) {
    int i;char *p=line,*comma;size_t n=strlen(line);
    if(!n||line[n-1]!='\n')return -1;
    line[n-1]=0;
    for(i=0;i<count-1;i++) {
        comma=strchr(p,',');if(!comma)return -1;
        *comma=0;parts[i]=p;p=comma+1;
    }
    if(strchr(p,','))return -1;
    parts[count-1]=p;return 0;
}
static void delta_of(char out[24],uint64_t before,uint64_t after) {
    if(after>before)snprintf(out,24,"+%" PRIu64,after-before);
    else if(after<before)snprintf(out,24,"-%" PRIu64,before-after);
    else strcpy(out,"0");
}
static int signature(char out[160],const char *date,const char *hash,
                     const char *status,uint64_t bytes,const char *delta) {
    int n=snprintf(out,160,"%s:%s:%s:%" PRIu64 ":%s",date,hash,status,bytes,delta);
    return n<0||n>=160?-1:0;
}
static int valid_date(const char *p) {
    int i;if(strlen(p)!=10)return 0;
    for(i=0;i<10;i++)if((i==4||i==7)?p[i]!='-':!isdigit((unsigned char)p[i]))return 0;
    return 1;
}
static int apply_section_row(char **p,int section) {
    char *tag=p[0],date[11],*key;
    char expected[24];uint64_t bytes;size_t n=strlen(tag);
    Item *it;
    if(n<17||tag[10]!='-'||strcmp(tag+n-5,"-SCAN"))return -1;
    memcpy(date,tag,10);date[10]=0;if(!valid_date(date))return -1;
    tag[n-5]=0;key=tag+11;
    if(section_of(key)!=section||!valid_sha(p[1])||parse_uint(p[3],&bytes))return -1;
    if(strchr(key,'\r')||strlen(key)>=KEY_CAP)return -1;
    it=get_item(key,section);if(!it)return -1;
    if(!strcmp(p[2],"NO CHANGE")) {
        if(!it->had_previous||it->was_removed||strcmp(p[1],it->previous)||bytes!=it->old_bytes||strcmp(p[4],"0"))return -1;
    } else if(!strcmp(p[2],"NEW")) {
        if(it->had_previous&&!it->was_removed)return -1;
        delta_of(expected,0,bytes);if(strcmp(expected,p[4]))return -1;
    } else if(!strcmp(p[2],"EDITED")) {
        if(!it->had_previous||it->was_removed||(bytes==it->old_bytes&&!strcmp(p[1],it->previous)))return -1;
        delta_of(expected,it->old_bytes,bytes);if(strcmp(expected,p[4]))return -1;
    } else if(!strcmp(p[2],"REMOVED")) {
        if(!it->had_previous||it->was_removed||strcmp(p[1],it->previous)||bytes)return -1;
        delta_of(expected,it->old_bytes,0);if(strcmp(expected,p[4]))return -1;
    } else return -1;
    if(strcmp(p[2],"NO CHANGE")) {
        if(signature(it->section_change,date,p[1],p[2],bytes,p[4])||
           sha_add(&it->section_history,(const unsigned char*)it->section_change,strlen(it->section_change))||
           sha_add(&it->section_history,(const unsigned char*)"\n",1))return -1;
    }
    strcpy(it->previous,p[1]);it->old_bytes=bytes;it->had_previous=1;
    it->was_removed=!strcmp(p[2],"REMOVED");return 0;
}
static int load_section(int section,int *exists) {
    char path[PATH_CAP],name[32],line[LINE_CAP],*p[5];FILE *f;
    int row=0;
    snprintf(name,sizeof name,".00/SECTION-%c_LOG.txt",'A'+section);
    if(join(path,sizeof path,root,name))return fail("log path too long",name);
    f=fopen(path,"rb");
    if(!f) {if(errno==ENOENT){*exists=0;return 0;}return fail("cannot read log",path);}
    *exists=1;
    while(fgets(line,sizeof line,f)) {
        row++;
        if(row==1) {if(strcmp(line,"scan_tag,sha256,status,bytes,delta_bytes\n")){fclose(f);return fail("invalid section log header",path);}continue;}
        if(split(line,p,5)||apply_section_row(p,section)) {fclose(f);return fail("invalid section log history",path);}
    }
    if(ferror(f)||!row) {fclose(f);return fail("unreadable or empty section log",path);}
    if(fclose(f))return fail("cannot close log",path);
    return 0;
}
static int load_master(int *exists) {
    char path[PATH_CAP],line[LINE_CAP],*p[6];FILE *f;int row=0,section;Item *it;uint64_t bytes;
    if(join(path,sizeof path,root,"SECTION_MASTER_LOG.txt"))return fail("master path too long",root);
    f=fopen(path,"rb");
    if(!f){if(errno==ENOENT){*exists=0;return 0;}return fail("cannot read master",path);}
    *exists=1;
    while(fgets(line,sizeof line,f)) {
        row++;
        if(row==1){if(strcmp(line,"date,relative_path,sha256,status,bytes,delta_bytes\n")){fclose(f);return fail("invalid master header",path);}continue;}
        if(split(line,p,6)||!valid_date(p[0])||(section=section_of(p[1]))<0||
           !valid_sha(p[2])||(!strcmp(p[3],"NO CHANGE"))||
           (strcmp(p[3],"NEW")&&strcmp(p[3],"EDITED")&&strcmp(p[3],"REMOVED"))||
           parse_uint(p[4],&bytes)){fclose(f);return fail("invalid master record",path);}
        it=get_item(p[1],section);
        if(!it||signature(it->master_change,p[0],p[2],p[3],bytes,p[5])||
           sha_add(&it->master_history,(const unsigned char*)it->master_change,strlen(it->master_change))||
           sha_add(&it->master_history,(const unsigned char*)"\n",1)){
            fclose(f);return fail("invalid master history",path);
        }
    }
    if(ferror(f)||!row){fclose(f);return fail("unreadable or empty master",path);}
    if(fclose(f))return fail("cannot close master",path);
    return 0;
}
static int hash_file(const char *path,const struct stat *seen,char out[65],uint64_t *bytes) {
    static unsigned char buf[65536];
    struct stat before,after,at_path;Sha sha;FILE *f;size_t n;
    f=fopen(path,"rb");if(!f)return fail("cannot open file",path);
    if(fstat(fileno(f),&before)||!S_ISREG(before.st_mode)||before.st_dev!=seen->st_dev||before.st_ino!=seen->st_ino) {
        fclose(f);return fail("file changed before hash",path);
    }
    sha_init(&sha);
    while((n=fread(buf,1,sizeof buf,f))!=0) {
        if(sha_add(&sha,buf,n)){fclose(f);return fail("SHA-256 input length exceeded",path);}
    }
    if(ferror(f)||fstat(fileno(f),&after)) {
        fclose(f);return fail("file read or stat failed",path);
    }
    if(fclose(f)||lstat(path,&at_path))return fail("file read or stat failed",path);
    if(before.st_dev!=after.st_dev||before.st_ino!=after.st_ino||
       before.st_size!=after.st_size||before.st_mtime!=after.st_mtime||
       before.st_ctime!=after.st_ctime||before.st_dev!=at_path.st_dev||
       before.st_ino!=at_path.st_ino||(uint64_t)before.st_size!=sha.bytes)
       return fail("file changed during hash",path);
    *bytes=sha.bytes;sha_end(&sha,out);return 0;
}
static int selected(const char *name) {
    size_t n=strlen(name);
    return include_all||(n>=2&&(!strcmp(name+n-2,".c")||!strcmp(name+n-2,".h")));
}
static int scan_tree(const char *relative,int depth) {
    char directory[PATH_CAP],child[PATH_CAP],rel[PATH_CAP],key[KEY_CAP];
    DIR *d;struct dirent *e;struct stat st;Item *it;int section;
    if(depth>MAX_DEPTH||join(directory,sizeof directory,root,relative))return fail("directory limit",relative);
    d=opendir(directory);if(!d)return fail("cannot open section directory",directory);
    for(;;) {
        errno=0;e=readdir(d);
        if(!e) {
            if(errno){closedir(d);return fail("cannot read directory",directory);}
            break;
        }
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;
        if(join(child,sizeof child,directory,e->d_name)||join(rel,sizeof rel,relative,e->d_name)) {
            closedir(d);return fail("path limit",relative);
        }
        if(lstat(child,&st)){closedir(d);return fail("cannot stat entry",child);}
        if(S_ISDIR(st.st_mode)) {
            if(scan_tree(rel,depth+1)){closedir(d);return -1;}
        } else if(S_ISLNK(st.st_mode)) {
            fprintf(stderr,"SKIP symlink: %s\n",rel);
        } else if(S_ISREG(st.st_mode)&&selected(e->d_name)) {
            if(encode_key(rel,key)||(section=section_of(key))<0) {
                closedir(d);return fail("cannot encode path",rel);
            }
            it=get_item(key,section);
            if(!it||it->seen){closedir(d);return fail("file limit or duplicate path",rel);}
            strcpy(it->path,child);it->seen=1;
            if(hash_file(child,&st,it->current,&it->bytes)){closedir(d);return -1;}
        }
    }
    if(closedir(d))return fail("cannot close directory",directory);
    return 0;
}
static int cmp_items(const void *a,const void *b) {
    return strcmp(((const Item*)a)->key,((const Item*)b)->key);
}
static int write_all(int fd,const char *data,size_t n) {
    while(n) {ssize_t w=write(fd,data,n);if(w<0&&errno==EINTR)continue;if(w<=0)return -1;data+=(size_t)w;n-=(size_t)w;}
    return 0;
}
static int open_append(const char *path,const char *header) {
    struct stat st;int fd,flags=O_WRONLY|O_APPEND|O_CREAT;
#ifdef O_NOFOLLOW
    flags|=O_NOFOLLOW;
#endif
    fd=open(path,flags,0644);
    if(fd<0)return -1;
    if(fstat(fd,&st)||!S_ISREG(st.st_mode)||(!st.st_size&&write_all(fd,header,strlen(header)))) {
        close(fd);return -1;
    }
    return fd;
}
static int emit_section(int section) {
    char path[PATH_CAP],name[32],line[LINE_CAP];size_t i;int fd,n;
    snprintf(name,sizeof name,".00/SECTION-%c_LOG.txt",'A'+section);
    if(join(path,sizeof path,root,name))return fail("log path too long",name);
    fd=open_append(path,"scan_tag,sha256,status,bytes,delta_bytes\n");
    if(fd<0)return fail("cannot append section log",path);
    for(i=0;i<used;i++)if(items[i].section==section&&items[i].status[0]) {
        Item *it=&items[i];
        n=snprintf(line,sizeof line,"%s-%s-SCAN,%s,%s,%" PRIu64 ",%s\n",
                   date_utc,it->key,it->current,it->status,it->bytes,it->delta);
        if(n<0||(size_t)n>=sizeof line||write_all(fd,line,(size_t)n)){
            close(fd);return fail("cannot append section row",path);
        }
    }
    if(fsync(fd)){close(fd);return fail("cannot sync section log",path);}
    if(close(fd))return fail("cannot close section log",path);
    return 0;
}
static int emit_master(void) {
    char path[PATH_CAP],line[LINE_CAP];size_t i;int fd,n;
    if(join(path,sizeof path,root,"SECTION_MASTER_LOG.txt"))return fail("master path too long",root);
    fd=open_append(path,"date,relative_path,sha256,status,bytes,delta_bytes\n");
    if(fd<0)return fail("cannot append master",path);
    for(i=0;i<used;i++) {
        Item *it=&items[i];
        if(!it->status[0]||!strcmp(it->status,"NO CHANGE"))continue;
        n=snprintf(line,sizeof line,"%s,%s,%s,%s,%" PRIu64 ",%s\n",
                   date_utc,it->key,it->current,it->status,it->bytes,it->delta);
        if(n<0||(size_t)n>=sizeof line||write_all(fd,line,(size_t)n)){
            close(fd);return fail("cannot append master row",path);
        }
    }
    if(fsync(fd)){close(fd);return fail("cannot sync master",path);}
    if(close(fd))return fail("cannot close master",path);
    return 0;
}
static int run(const char *argument) {
    char dir[PATH_CAP],rel[16],alternative[16],lockpath[PATH_CAP],expected[24];struct stat st;
    time_t now;struct tm utc;size_t i;int a,exists,logs=0,missing=0,master,lockfd,changes=0;
    if(!*argument||strlen(argument)>=PATH_CAP-64)return fail("invalid root path",argument);
    strcpy(root,argument);
    while(strlen(root)>1&&root[strlen(root)-1]=='/')root[strlen(root)-1]=0;
    now=time(NULL);
    if(now==(time_t)-1||!gmtime_r(&now,&utc)||!strftime(date_utc,sizeof date_utc,"%Y-%m-%d",&utc))
        return fail("UTC date unavailable",root);
    for(a=0;a<SECTIONS;a++) {
        snprintf(rel,sizeof rel,"SECTION-%c",'A'+a);
        snprintf(alternative,sizeof alternative,"SECTION\xE2\x80\x94%c",'A'+a);
        if(join(dir,sizeof dir,root,rel))return fail("section path too long",rel);
        if(!lstat(dir,&st)) {
            if(!S_ISDIR(st.st_mode))return fail("invalid section directory",dir);
            strcpy(section_names[a],rel);
            if(join(dir,sizeof dir,root,alternative))return fail("section path too long",alternative);
            if(!lstat(dir,&st))return fail("both section spellings exist",dir);
            if(errno!=ENOENT)return fail("cannot stat alternate section",dir);
        } else {
            if(errno!=ENOENT)return fail("cannot stat section",dir);
            if(join(dir,sizeof dir,root,alternative)||lstat(dir,&st)||!S_ISDIR(st.st_mode))
                return fail("missing or invalid section",alternative);
            strcpy(section_names[a],alternative);
        }
    }
    if(join(dir,sizeof dir,root,".00"))return fail("log directory path too long",root);
    if(mkdir(dir,0755)&&errno!=EEXIST)return fail("cannot create .00",dir);
    if(lstat(dir,&st)||!S_ISDIR(st.st_mode))return fail("invalid .00 directory",dir);
    if(join(lockpath,sizeof lockpath,dir,".SECTION_HASH_LOCK"))return fail("lock path too long",dir);
    lockfd=open(lockpath,O_RDWR|O_CREAT,0644);
    if(lockfd<0||flock(lockfd,LOCK_EX))return fail("cannot lock logs",lockpath);
    for(a=0;a<SECTIONS;a++) {
        if(load_section(a,&exists))return -1;
        if(exists)logs++;else missing++;
    }
    if(logs&&missing)return fail("one or more section logs missing",dir);
    if(load_master(&master))return -1;
    if(master&&!logs)return fail("master exists without section logs",root);
    for(i=0;i<used;i++) {
        char a_hash[65],b_hash[65];Item *it=&items[i];
        sha_end(&it->section_history,a_hash);sha_end(&it->master_history,b_hash);
        if(strcmp(it->section_change,it->master_change)||strcmp(a_hash,b_hash))
            return fail("section/master history mismatch",it->key);
    }
    for(a=0;a<SECTIONS;a++) {
        if(scan_tree(section_names[a],0))return -1;
    }
    for(i=0;i<used;i++) {
        Item *it=&items[i];
        if(!it->seen) {
            if(!selected(it->key))continue;
            if(!it->had_previous||it->was_removed)continue;
            strcpy(it->status,"REMOVED");strcpy(it->current,it->previous);it->bytes=0;
            delta_of(it->delta,it->old_bytes,0);
        } else if(!it->had_previous||it->was_removed) {
            strcpy(it->status,"NEW");delta_of(it->delta,0,it->bytes);
        } else if(it->old_bytes==it->bytes&&!strcmp(it->previous,it->current)) {
            strcpy(it->status,"NO CHANGE");strcpy(it->delta,"0");
        } else {
            strcpy(it->status,"EDITED");delta_of(it->delta,it->old_bytes,it->bytes);
        }
        if(!strcmp(it->status,"EDITED")) {
            delta_of(expected,it->old_bytes,it->bytes);
            if(strcmp(expected,it->delta))return fail("byte delta error",it->key);
        }
    }
    qsort(items,used,sizeof items[0],cmp_items);
    for(a=0;a<SECTIONS;a++)if(emit_section(a))return -1;
    if(emit_master())return -1;
    for(i=0;i<used;i++) {
        Item *it=&items[i];
        if(!it->status[0]||!strcmp(it->status,"NO CHANGE"))continue;
        printf("%s %s bytes=%" PRIu64 " delta=%s sha256=%s\n",
               it->status,it->key,it->bytes,it->delta,it->current);changes++;
    }
    printf("Scanned %zu tracked paths; %d change(s); UTC %s.\n",used,changes,date_utc);
    close(lockfd);return 0;
}
int main(int argc,char **argv) {
    if(argc==2&&!strcmp(argv[1],"--self-test")) {
        if(self_test())return fail("SHA-256 self-test failed",NULL),1;
        puts("SHA-256 self-test passed.");return 0;
    }
    if(argc<2||argc>3||(argc==3&&strcmp(argv[2],"--all-files"))) {
        fprintf(stderr,"Usage: %s ROOT [--all-files]\n       %s --self-test\n",argv[0],argv[0]);return 2;
    }
    include_all=argc==3;
    if(self_test())return fail("SHA-256 self-test failed",NULL),1;
    return run(argv[1])?1:0;
}
