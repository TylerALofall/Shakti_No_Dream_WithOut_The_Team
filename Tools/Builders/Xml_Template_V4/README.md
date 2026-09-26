# Xml_Template_V4

FILE SETS (in order):
1 of 2 — shakti-xml-template-v4
2 of 2 — shakti-xml-wrap-v4

SCRIPT FILES (in order):
1. README.md
2. shakti-xml-template-v4.c
3. shakti-xml-wrap-v4.c
4. shakti-xml-wrap-v4.h
5. tests/

## I. INTRODUCTION

The v4 XML template pair: a bounded reader (wrap) and a template maker. Wrap scans XML with a fixed-budget fgetc reader — no heap, no guessing; unclosed or malformed structure benches loudly. Template walks the wrapped structure and emits a template where every element value becomes a `[parent.child]` placeholder and every attribute becomes `[owner.@name]`. Two-match confirmation gates any master-list append. Output is .tmp-then-rename; existing destination paths are refused.

## II. REQUIREMENTS

C99 strict, zero heap:

```
cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 shakti-xml-wrap-v4.c shakti-xml-template-v4.c -o shakti-xml-template-v4
```

## III. RUNNING NOTES

Fresh run, structure check on example_matched.txt — report bytes exactly as emitted:

```
# structure from example_matched.txt; root a
a	[a]	level=1	indent=0	at=0	BRANCH
	b	[a.b]	level=2	indent=2	at=6	BRANCH
		c	[b.c]	level=3	indent=4	at=14	LEAF

	d	[a.d]	level=2	indent=2	at=32	BRANCH
		e	[d.e]	level=3	indent=4	at=40	LEAF

# discovery order	element	open->close	lines	parent	level	boundary
1	a	1->8	8	(root)	1	matched
2	b	2->4	3	a	2	matched
3	c	3->3	1	b	3	matched
4	d	5->7	3	a	2	matched
5	e	6->6	1	d	3	matched

# match confirmation: one structure; master unchanged

# element	found	template_patterns
a	1	1
b	1	1
c	1	1
d	1	1
e	1	1
```

Fresh run, template from the B work order — emitted bytes exactly:

```
<Work_Order lvl="1">
	<QA_specialist lvl="2">[work_order.qa_specialist]</QA_specialist>

	<date lvl="2">[work_order.date]</date>

	<repair lvl="2">[work_order.repair]</repair>

	<total_items lvl="2">[work_order.total_items]</total_items>

	<task_description lvl="2">[work_order.task_description]</task_description>

	<SECTION lvl="2" id="[section.@id]">
		<file_name lvl="3">[section.file_name]</file_name>
		<function_name lvl="3">[section.function_name]</function_name>
		<function_order lvl="3">[section.function_order]</function_order>
		<function_type lvl="3">[section.function_type]</function_type>
		<function_input lvl="3">[section.function_input]</function_input>
		<function_return lvl="3">[section.function_return]</function_return>
		<function_description lvl="3">[section.function_description]</function_description>
		<function_complete lvl="3">[section.function_complete]</function_complete>
	</SECTION>

	<QA_Specialist_attestation lvl="2">[work_order.qa_specialist_attestation]</QA_Specialist_attestation>

	<Supervisor_attestation lvl="2">[work_order.supervisor_attestation]</Supervisor_attestation>
</Work_Order>
```

B_WORK run line: `OK: 1 wrapped object(s), 17 unique names, 0 master additions, 23 repeated shapes; confirmed`.

## IV. REFERENCES

Test samples in tests/, named after the files they came from:

- b_template.xml
- check_report.txt
- check_template.xml
- example_matched.txt
- example_unclosed.txt
