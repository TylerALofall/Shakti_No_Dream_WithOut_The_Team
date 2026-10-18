REQUIRED SCRIPTS: shakti-xml-wrap-v4.c | shakti-xml-template-v4.c | shakti-xml-wrap-v4.h | Xml.Template.v4-2026-10-17.manifest.txt
# Xml-Template-v4-2026-10-17

FAMILY: Xml
TOOL: Template
VERSION: 4
DATE: 2026-10-17
ENTRY_KIND: manifest
ENTRY: Xml.Template.v4-2026-10-17.manifest.txt
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 shakti-xml-wrap-v4.c shakti-xml-template-v4.c -o shakti-xml-template-v4
COMMAND: ./shakti-xml-template-v4 "{input_file}" "{report_output}" "{names_output}" "{template_output}" "{master_file}"
EXAMPLE: ./shakti-xml-template-v4 Example.Work.xml Example.Report.txt Example.Names.txt Example.Template.xml Example.Master.txt

INTRODUCTION:
Build XML templates with the supplied v4 wrap collector and template maker; preserve their paired header and C modules.

PACKAGE DETAILS: Xml-Template-v4-2026-10-17.txt

ADD-ON FILES:
  Example.Work.xml

FILE HASHES:
c28b0ac8b1fa72ae57fca84b2ae8980189b86cb9a9f3fb87a39fe6ba9ace2661  Example.Work.xml
4858c71581d379c92611f4d8de6283358db6980beb77c507dccf26e00946484f  Xml-Template-v4-2026-10-17.txt
4858c71581d379c92611f4d8de6283358db6980beb77c507dccf26e00946484f  Xml.Template.v4-2026-10-17.manifest.txt
30c72749272f55ba09525bdc934040e6de9d69614f07fdc36b9a4ba69356e806  shakti-xml-template-v4.c
ac191ec3788ce605143a1150bc05aa5202e6a2e8eeb77decfddb076fc12208a0  shakti-xml-wrap-v4.c
2b75f18c38e927ef5ed18d68eb00a5f9da4e1a3cc6b5ae548d02c58f2ad36b87  shakti-xml-wrap-v4.h
