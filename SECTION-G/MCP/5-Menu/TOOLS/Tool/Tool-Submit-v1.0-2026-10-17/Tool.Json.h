/* Tool.Json.h -- strict bounded JSON reader, included by Tool.Support.c.
 * Raw saved response -> typed preorder nodes. Missing fields, duplicate keys,
 * bad escapes, NUL, invalid UTF-8 and partial documents stop the transaction.
 * Decodes strings once, in place; no heuristic searches for response fields.
 */
static size_t ts_cursor;
static int ts_nodes;
static void ts_space(void)
{
    while (strchr(" \t\r\n",ts_input[ts_cursor]) && ts_input[ts_cursor]) ++ts_cursor;
}
void ts_utf8(const char *text, size_t n)
{
    size_t i=0;
    while (i<n) {
        unsigned c=(unsigned char)text[i++], code=0, need=0, low=0;
        ts_need(c!=0,"NUL byte is not an admitted text file");
        if (c<128) continue;
        if (c>=0xc2 && c<=0xdf) { code=c&31; need=1; low=0x80; }
        else if (c>=0xe0 && c<=0xef) { code=c&15; need=2; low=0x800; }
        else if (c>=0xf0 && c<=0xf4) { code=c&7; need=3; low=0x10000; }
        else ts_die("invalid UTF-8 lead byte");
        ts_need(n-i>=need,"incomplete UTF-8 sequence");
        while (need--) {
            c=(unsigned char)text[i++];
            ts_need((c&0xc0)==0x80,"invalid UTF-8 continuation");
            code=(code<<6)|(c&63);
        }
        ts_need(code>=low && code<=0x10ffff && !(code>=0xd800 && code<=0xdfff),"invalid UTF-8 code point");
    }
}
static unsigned ts_u4(void)
{
    unsigned value=0, i;
    for (i=0;i<4;++i) {
        unsigned c=(unsigned char)ts_input[ts_cursor++];
        ts_need(isxdigit((int)c),"invalid JSON Unicode escape");
        value=value*16+(c<='9'?c-'0':(c|32)-'a'+10);
    }
    return value;
}
static void ts_codepoint(size_t *w, unsigned cp)
{
    ts_need(cp && cp<=0x10ffff && !(cp>=0xd800 && cp<=0xdfff),"unsupported JSON code point");
    if (cp<0x80) ts_input[(*w)++]=(char)cp;
    else if (cp<0x800) {
        ts_input[(*w)++]=(char)(0xc0|(cp>>6)); ts_input[(*w)++]=(char)(0x80|(cp&63));
    } else if (cp<0x10000) {
        ts_input[(*w)++]=(char)(0xe0|(cp>>12));
        ts_input[(*w)++]=(char)(0x80|((cp>>6)&63)); ts_input[(*w)++]=(char)(0x80|(cp&63));
    } else {
        ts_input[(*w)++]=(char)(0xf0|(cp>>18)); ts_input[(*w)++]=(char)(0x80|((cp>>12)&63));
        ts_input[(*w)++]=(char)(0x80|((cp>>6)&63)); ts_input[(*w)++]=(char)(0x80|(cp&63));
    }
}
static void ts_parse_string(int node)
{
    size_t w;
    ++ts_cursor;
    ts_json[node].start=w=ts_cursor;
    while (ts_input[ts_cursor]!='"') {
        unsigned c=(unsigned char)ts_input[ts_cursor++];
        ts_need(c>=32,"unterminated JSON string or raw control character");
        if (c=='\\') {
            c=(unsigned char)ts_input[ts_cursor++];
            if (c=='u') {
                unsigned cp=ts_u4();
                if (cp>=0xd800 && cp<=0xdbff) {
                    unsigned lo;
                    ts_need(ts_input[ts_cursor]=='\\' && ts_input[ts_cursor+1]=='u',"missing low surrogate");
                    ts_cursor+=2; lo=ts_u4();
                    ts_need(lo>=0xdc00 && lo<=0xdfff,"bad low surrogate");
                    cp=0x10000+((cp-0xd800)<<10)+(lo-0xdc00);
                }
                ts_codepoint(&w,cp); continue;
            }
            if (c=='b') c='\b'; else if (c=='f') c='\f';
            else if (c=='n') c='\n'; else if (c=='r') c='\r'; else if (c=='t') c='\t';
            else ts_need(c=='"' || c=='\\' || c=='/',"invalid JSON escape");
        }
        ts_input[w++]=(char)c;
    }
    ++ts_cursor;
    ts_input[w]=0; ts_json[node].len=w-ts_json[node].start;
    ts_utf8(ts_input+ts_json[node].start,ts_json[node].len);
}
static int ts_parse_value(unsigned depth)
{
    int node, type;
    ts_need(depth<32 && ts_nodes<TS_TOKENS,"JSON depth/token capacity");
    ts_space(); node=ts_nodes++; type=(unsigned char)ts_input[ts_cursor];
    ts_json[node].type=type; ts_json[node].child=-1;
    ts_json[node].start=ts_cursor;
    if (type=='"') ts_parse_string(node);
    else if (type=='{' || type=='[') {
        int close=type=='{'?'}':']';
        ++ts_cursor; ts_space();
        if (ts_input[ts_cursor]!=close) ts_json[node].child=ts_nodes;
        while (ts_input[ts_cursor]!=close) {
            if (type=='{') {
                int key, prev;
                ts_need(ts_input[ts_cursor]=='"',"JSON object key required");
                key=ts_parse_value(depth+1);
                for (prev=ts_json[node].child;prev<key;prev=ts_json[prev+1].next)
                    ts_need(strcmp(ts_string(prev),ts_string(key))!=0,"duplicate JSON key");
                ts_space(); ts_need(ts_input[ts_cursor++]==':',"JSON colon required");
            }
            (void)ts_parse_value(depth+1); ts_space();
            if (ts_input[ts_cursor]==close) break;
            ts_need(ts_input[ts_cursor++]==',',"JSON comma required"); ts_space();
            ts_need(ts_input[ts_cursor]!=close,"JSON trailing comma");
        }
        ++ts_cursor;
    } else {
        size_t start=ts_cursor, n;
        if (type=='t' || type=='f' || type=='n') {
            const char *word=type=='t'?"true":type=='f'?"false":"null";
            n=strlen(word);
            ts_need(!strncmp(ts_input+start,word,n),"invalid JSON primitive"); ts_cursor+=n;
        } else {
            if (ts_input[ts_cursor]=='-') ++ts_cursor;
            if (ts_input[ts_cursor]=='0') ++ts_cursor;
            else {
                ts_need(ts_input[ts_cursor]>='1' && ts_input[ts_cursor]<='9',"invalid JSON number");
                while (isdigit((unsigned char)ts_input[ts_cursor])) ++ts_cursor;
            }
            if (ts_input[ts_cursor]=='.') {
                ++ts_cursor; ts_need(isdigit((unsigned char)ts_input[ts_cursor]),"invalid JSON fraction");
                while (isdigit((unsigned char)ts_input[ts_cursor])) ++ts_cursor;
            }
            if (ts_input[ts_cursor]=='e' || ts_input[ts_cursor]=='E') {
                ++ts_cursor;
                if (ts_input[ts_cursor]=='+' || ts_input[ts_cursor]=='-') ++ts_cursor;
                ts_need(isdigit((unsigned char)ts_input[ts_cursor]),"invalid JSON exponent");
                while (isdigit((unsigned char)ts_input[ts_cursor])) ++ts_cursor;
            }
        }
        ts_json[node].len=ts_cursor-start;
    }
    ts_json[node].next=ts_nodes;
    return node;
}
int ts_json_parse(void)
{
    ts_cursor=0; ts_nodes=0;
    ts_utf8(ts_input,strlen(ts_input));
    (void)ts_parse_value(0); ts_space();
    ts_need(!ts_input[ts_cursor],"extra text after JSON; paste the response body only");
    return 0;
}
int ts_json_read(const char *path)
{
    size_t n=ts_read(path,ts_input,TS_BUFFER+1);
    ts_need(strlen(ts_input)==n,"NUL in JSON response");
    return ts_json_parse();
}
int ts_optional(int object, const char *key)
{
    int p;
    ts_need(object>=0 && object<ts_nodes && ts_json[object].type=='{',"JSON object required");
    for (p=ts_json[object].child;p>=0 && p<ts_json[object].next;p=ts_json[p+1].next)
        if (!strcmp(ts_string(p),key)) return p+1;
    return -1;
}
int ts_get(int object, const char *key)
{
    int p=ts_optional(object,key);
    if (p<0) { fprintf(stderr,"Missing response field: %s\n",key); ts_die("incomplete JSON object"); }
    return p;
}
const char *ts_string(int node)
{
    ts_need(node>=0 && node<ts_nodes && ts_json[node].type=='"',"JSON string required");
    return ts_input+ts_json[node].start;
}
int ts_null(int node)
{
    ts_need(node>=0 && node<ts_nodes,"JSON node required");
    return ts_json[node].type=='n';
}
int ts_number(int node)
{
    char b[32], *end; long n;
    ts_need(ts_json[node].type == '-' || isdigit(ts_json[node].type),"JSON number type required");
    ts_need(ts_json[node].len>0 && ts_json[node].len<sizeof b,"JSON integer capacity");
    memcpy(b,ts_input+ts_json[node].start,ts_json[node].len); b[ts_json[node].len]=0;
    errno=0; n=strtol(b,&end,10);
    ts_need(!errno && !*end && n>=0 && n<=INT_MAX,"nonnegative integer required");
    return (int)n;
}
void ts_expect(int node, const char *value)
{
    ts_need(!strcmp(ts_string(node),value),"response identity/type does not match submission");
}
