REQUIRED SCRIPTS: shakti-xml-wrap-v5.c | shakti-xml-template-v5.c | shakti-xml-wrap-v5.h | Xml.Template.v5-2026-10-17.manifest.txt
# Xml-Template-v5-2026-10-17

FAMILY: Xml
TOOL: Template
VERSION: 5
DATE: 2026-10-17
ENTRY_KIND: manifest
ENTRY: Xml.Template.v5-2026-10-17.manifest.txt
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 shakti-xml-wrap-v5.c shakti-xml-template-v5.c -o shakti-xml-template-v5
COMMAND: ./shakti-xml-template-v5 "{input_file}" "{report_output}" "{names_output}" "{template_output}" "{master_file}"
EXAMPLE: ./shakti-xml-template-v5 Example.Work.xml Example.Report.txt Example.Names.txt Example.Template.xml Example.Master.txt

INTRODUCTION:
Build XML templates and retain attribute values with the supplied v5 wrap collector and template maker.

PACKAGE DETAILS: Xml-Template-v5-2026-10-17.txt

ADD-ON FILES:
  Example.Work.xml
  text.txt

FILE HASHES:
c28b0ac8b1fa72ae57fca84b2ae8980189b86cb9a9f3fb87a39fe6ba9ace2661  Example.Work.xml
020bb7bbcacb8c4bbfe8070bff6286df85313965d390ac63472cc664b35b5d89  Xml-Template-v5-2026-10-17.txt
020bb7bbcacb8c4bbfe8070bff6286df85313965d390ac63472cc664b35b5d89  Xml.Template.v5-2026-10-17.manifest.txt
ac32166b5161c5b944813eb272795607c0ee094be5c00031f6730201bbe3b9ab  shakti-xml-template-v5.c
6c76dd2bdc751a72d48c84b864789bc74622da7baef8132a1050a99b48ee1203  shakti-xml-wrap-v5.c
476e1060baafe84a2ef5577512b9569ff5c8c438debbf075a40c7fd24840f5cb  shakti-xml-wrap-v5.h
f6bbe1fee028f5c44c053949fede5f3f038cc3a789f72069a376bb411a99f029  text.txt
