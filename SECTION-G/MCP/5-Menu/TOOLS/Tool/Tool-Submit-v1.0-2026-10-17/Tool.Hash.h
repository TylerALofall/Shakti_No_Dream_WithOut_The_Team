/* Tool.Hash.h -- SHA-256 and base64, included only by Tool.Support.c.
 * Source bytes -> deterministic fingerprints / GitHub file contents.
 * No keys, signatures, heap, network, or operating-system crypto libraries.
 */
static uint32_t ts_ror(uint32_t x, unsigned n)
{
    return (x >> n) | (x << (32 - n));
}
static void ts_sha_block(uint32_t h[8], const unsigned char b[64])
{
    static const uint32_t k[64] = {
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
        0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
        0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
        0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
        0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
        0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
        0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
        0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
        0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
    };
    uint32_t w[64], a,hb,c,d,e,f,g,z,t1,t2;
    unsigned i;
    for (i = 0; i < 16; ++i)
        w[i] = ((uint32_t)b[i*4] << 24) | ((uint32_t)b[i*4+1] << 16)
             | ((uint32_t)b[i*4+2] << 8) | b[i*4+3];
    for (i = 16; i < 64; ++i) {
        uint32_t s0 = ts_ror(w[i-15],7)^ts_ror(w[i-15],18)^(w[i-15]>>3);
        uint32_t s1 = ts_ror(w[i-2],17)^ts_ror(w[i-2],19)^(w[i-2]>>10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a=h[0]; hb=h[1]; c=h[2]; d=h[3]; e=h[4]; f=h[5]; g=h[6]; z=h[7];
    for (i = 0; i < 64; ++i) {
        t1=z+(ts_ror(e,6)^ts_ror(e,11)^ts_ror(e,25))+((e&f)^(~e&g))+k[i]+w[i];
        t2=(ts_ror(a,2)^ts_ror(a,13)^ts_ror(a,22))+((a&hb)^(a&c)^(hb&c));
        z=g; g=f; f=e; e=d+t1; d=c; c=hb; hb=a; a=t1+t2;
    }
    h[0]+=a; h[1]+=hb; h[2]+=c; h[3]+=d; h[4]+=e; h[5]+=f; h[6]+=g; h[7]+=z;
}
void ts_sha256(const void *bytes, size_t n, char hex[65])
{
    const unsigned char *p = bytes;
    uint32_t h[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
                     0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    unsigned char tail[128] = {0};
    uint64_t bits = (uint64_t)n * 8;
    size_t i, full = n / 64, rest = n % 64, end = rest < 56 ? 64 : 128;
    for (i = 0; i < full; ++i) ts_sha_block(h,p+64*i);
    memcpy(tail,p+64*full,rest);
    tail[rest] = 0x80;
    for (i = 0; i < 8; ++i) tail[end-1-i] = (unsigned char)(bits>>(i*8));
    ts_sha_block(h,tail);
    if (end == 128) ts_sha_block(h,tail+64);
    for (i = 0; i < 8; ++i) snprintf(hex+i*8,9,"%08lx",(unsigned long)h[i]);
}
void ts_base64(const unsigned char *p, size_t n)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t i;
    for (i = 0; i < n; i += 3) {
        unsigned v = (unsigned)p[i] << 16;
        char b[5];
        if (i+1 < n) v |= (unsigned)p[i+1] << 8;
        if (i+2 < n) v |= p[i+2];
        b[0]=alphabet[(v>>18)&63]; b[1]=alphabet[(v>>12)&63];
        b[2]=i+1<n?alphabet[(v>>6)&63]:'=';
        b[3]=i+2<n?alphabet[v&63]:'='; b[4]=0;
        ts_add(b);
    }
}
/* Published standard vectors exercise empty, short and multiblock padding. */
void ts_hash_checks(void)
{
    char h[65];
    const char *s="abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    ts_sha256("",0,h);
    ts_need(!strcmp(h,"e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"),"SHA-256 empty vector");
    ts_sha256("abc",3,h);
    ts_need(!strcmp(h,"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad"),"SHA-256 abc vector");
    ts_sha256(s,strlen(s),h);
    ts_need(!strcmp(h,"248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1"),"SHA-256 padding vector");
    ts_reset(); ts_base64((const unsigned char *)"foobar",6);
    ts_need(!strcmp(ts_output,"Zm9vYmFy"),"base64 vector");
    ts_reset();
}
