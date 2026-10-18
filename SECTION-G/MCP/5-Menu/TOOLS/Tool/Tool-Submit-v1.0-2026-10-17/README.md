REQUIRED SCRIPTS: Tool.Submit.c | Tool.Support.c | Tool.Submit.h | Tool.Hash.h | Tool.Json.h | Tool.Forms.h | Tool.Plan.h | Tool.Git.h | Tool.Requests.h | Tool.Submit.v1.0-2026-10-17.manifest.txt
# Tool-Submit-v1.0-2026-10-17

FAMILY: Tool
TOOL: Submit
VERSION: 1.0
DATE: 2026-10-17
ENTRY_KIND: manifest
ENTRY: Tool.Submit.v1.0-2026-10-17.manifest.txt
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 Tool.Submit.c Tool.Support.c -o shakti-submit
COMMAND: ./shakti-submit "{action}" "{argument}"
EXAMPLE: ./shakti-submit check

INTRODUCTION:
Prepare local submission records and request text without sending network traffic. The example checks local hash and file-mode behavior; remote transport remains user operated.

PACKAGE DETAILS: Tool-Submit-v1.0-2026-10-17.txt

ADD-ON FILES:
  CLANG_RUNS.txt
  Form.Set.txt
  Form.Single.txt
  GCC_RUNS.txt
  HTTP.Settings.txt
  PHONE_START_HERE.txt
  RUN_REPORT.txt
  Release.Check.c.txt
  Run.Suite.c.txt
  SANITIZER_RUNS.txt
  SHA256_INVENTORY.txt
  SOURCE_NOTES.txt
  Tool.Submit.Guide.txt

FILE HASHES:
860f0f82e08115084b2cef170c45e072c0ca54963a034d544647cced85f3059e  CLANG_RUNS.txt
c50c58d40dcbdefff31dd2ce653bcf2f152f991e81def100446f038e07e85a55  Form.Set.txt
48c7fde1d91dee525f2331abb5d304aa32565dd6bb67c6e687a4b535fffb64e0  Form.Single.txt
860f0f82e08115084b2cef170c45e072c0ca54963a034d544647cced85f3059e  GCC_RUNS.txt
85ea47b36a88d74eb9c97b5b35d876d39c7c95b920139e5f8d4a0b543bf6be85  HTTP.Settings.txt
a43f932974f85c66d4d641b3fd21c1ff5f8a23738ff0311047bf05d44802f15a  PHONE_START_HERE.txt
bd356d3ff5a37a66aec0b7bebf7f6157164bd8155481880787e8e4d5c3557811  RUN_REPORT.txt
362aad3bacff430251964332b2423583ec68c6462e767fe3b3f3d53d8807321c  Release.Check.c.txt
ad056bab521f07d06eeba1d057ffbb2f4550d7fcc13c3db2af352f19ee343dc5  Run.Suite.c.txt
860f0f82e08115084b2cef170c45e072c0ca54963a034d544647cced85f3059e  SANITIZER_RUNS.txt
2056576f84fbc0ba0eb8dd746a1ca372952bea994e39392991ce2e77098aecf8  SHA256_INVENTORY.txt
1103481dddc0cc456b4a6d239cb506c3bbc3ee0c8e8f735424942a62650d030a  SOURCE_NOTES.txt
86e0bb174202629bf78d4763883471572141edcb107a975341167317a90d8f7f  Tool-Submit-v1.0-2026-10-17.txt
3c184e9f2af6861ce43485b03fafb35f2b657ec31b31f3828798e146cbd7f1a5  Tool.Forms.h
57486f2a1b3768ae5e5e1a602decfb7c88720d57c14aba16aff549e611c6c753  Tool.Git.h
b4f403fa20904420e76694fedc9abc432870c155f71c868242b8b81d1628e79f  Tool.Hash.h
e98ea0cecad86c27f8abc6112b237cd8199e29703b2db67ba8c533489122fe51  Tool.Json.h
f530287cf8fb578b42ba4e6b5dbaf5fa4dc91a9595f25fd034b1f6d2447e1ac0  Tool.Plan.h
99305565873bb1b29314c07ea32e313dae1fe6d588ebd7494098ee743462f0ab  Tool.Requests.h
d12f95a341bb31f2289dead7a796e37ee06229cf0c49228e7caca57ee0a2d45f  Tool.Submit.Guide.txt
c3406f5c23bf05d0c924d0b584a60eef8019d5de77d7df5feaa74756b34f7152  Tool.Submit.c
b89083d7c9c7dea04cd51c8cf613a94a62043a4d545be16a26482fe75ca2aaea  Tool.Submit.h
86e0bb174202629bf78d4763883471572141edcb107a975341167317a90d8f7f  Tool.Submit.v1.0-2026-10-17.manifest.txt
317870dfb6a553c5d94bba5cbb3c9c7ba084f2a9d4d0cb30c51fe829a2a1f1ef  Tool.Support.c
