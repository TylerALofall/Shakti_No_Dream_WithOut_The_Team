#ifndef SHAKTI_XML_WRAP_V4_H
#define SHAKTI_XML_WRAP_V4_H

#include <stdio.h>

#define XML_MAX_NODES 40960 /* Refuse cleanly past this fixed capacity. */
#define XML_MAX_DEPTH 256
#define XML_NAME 64
#define XML_ATTRS 192

/* Nodes are in opening order. after is the first node outside this wrap. */
typedef struct {
    char name[XML_NAME], attrs[XML_ATTRS];
    int parent, depth, after, self, indent;
    long offset;
    unsigned line, close_line;
} XmlNode;

typedef struct {
    XmlNode node[XML_MAX_NODES];
    int count, roots;
    char error[192];
} XmlScan;

int shakti_xml_scan(FILE *input, XmlScan *result);
int shakti_xml_same_shape(const XmlScan *result, int a, int b);
int shakti_xml_confirmed(const XmlScan *result);

#endif
