REQUIRED SCRIPTS: Senses.Assembly.c | Senses.Assembly.manifest.txt | Sources.Bindings.txt | Native.Run.txt | Inputs/document.ppm | Inputs/atlas.ppm
# Senses-Assembly-v1.0.1-2026-10-19

FAMILY: Senses
TOOL: Assembly
VERSION: 1.0.1
DATE: 2026-10-19
ENTRY_KIND: manifest
ENTRY: Senses.Assembly.manifest.txt
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 -DSHAKTI_APP_NO_MAIN -Iinclude SECTION-G/MCP/5-Menu/TOOLS/Senses/Senses-Assembly-v1.0.1-2026-10-19/Senses.Assembly.c @Runtime.Sources.txt -o shakti-senses
COMMAND: ./shakti-senses --run "{new_output_directory}" "{image_pnm}" "{wav_or_minus}" "{atlas_or_minus}" "{alphabet}" "{cell_width}" "{cell_height}"
EXAMPLE: ./shakti-senses --run /tmp/senses-output SECTION-G/MCP/5-Menu/TOOLS/Senses/Senses-Assembly-v1.0.1-2026-10-19/Inputs/document.ppm - SECTION-G/MCP/5-Menu/TOOLS/Senses/Senses-Assembly-v1.0.1-2026-10-19/Inputs/atlas.ppm " 0123456789+=ABC" 24 32

INTRODUCTION:
Run typed image and optional PCM files through tagged preparation, exact reconstruction, heartbeat publication, supplied-font OCR and existing memory; published raw evidence is image-only.

PACKAGE DETAILS: Senses-Assembly-v1.0.1-2026-10-19.txt

ADD-ON FILES:
  Evidence/COMPLETE.txt
  Evidence/Capsule.txt
  Evidence/Characters.txt
  Evidence/Colors-Rebuilt.png
  Evidence/Frames.txt
  Evidence/Memory.txt
  Evidence/OCR.txt
  Evidence/Original.png
  Evidence/Planes.bin
  Evidence/Rebuilt.png
  Inputs/Fixture.txt

FILE HASHES:
2e5da1effaa165069aa3b926eff64531230425ad7b09b29fb6cb161d9c191edd  Evidence/COMPLETE.txt
331e2eda05f9bdc41ac24d8cadf48b6c880b791a056fd680304354cd1804964f  Evidence/Capsule.txt
52864817ba5216c00b0bc3d179e983c2fbf795bbdc43794caf5b9e51b68180e0  Evidence/Characters.txt
9c861498d0575229ec11564b74abc1115e62c0c4bad9350d2d14f46f2fe80aa0  Evidence/Colors-Rebuilt.png
ca476fc8405fd08d35dce5b2ef57c4671e54fa673a78e553cf38541c64867923  Evidence/Frames.txt
d30069a8b696da48960059e819af1be869bf1a6a561e0af9f6f98cac329b09ef  Evidence/Memory.txt
e90e2a87b55b194d9f8e7be04309ccfcbf2a26dc5cb710963fbdf7d0f85c4386  Evidence/OCR.txt
70571bea71c27c16911fc500fd5d1bf3f018c14bba044804e3c1498b638afd71  Evidence/Original.png
6361d3468d26944f71a297fe097d4b25da7f8270314e2fee9fb8810c6280782d  Evidence/Planes.bin
70571bea71c27c16911fc500fd5d1bf3f018c14bba044804e3c1498b638afd71  Evidence/Rebuilt.png
72d095f3b3e658c761a4947bf6e30f1941fe19cb4d890337e85753cf7c057b0d  Inputs/Fixture.txt
344a9edc1f97c4e2c4247969637ac15e7001cc2fa012326ee7bb62215bae4e6b  Inputs/atlas.ppm
d5a0a0a361f7d0b64734789fa5a599d9782ae440b60e3650b6a5722a2b23925e  Inputs/document.ppm
819db391bdde54ff46be4c745abab7efbd5e086e26d4187f298c86b1ce643df9  Native.Run.txt
d1b2fb0f97e609bbd39c9d3a9ad040759a52c2129e12035da9efa84990864352  Senses-Assembly-v1.0.1-2026-10-19.txt
3a362f9da3395bd15efde94223640a668b42d92acf51fbc9dc87c7339d90ffaa  Senses.Assembly.c
0d1ad84fb616f0b92cdab4a3089af185afbf740f407aea5b60532a27fa22f687  Senses.Assembly.manifest.txt
f75e44699f79f9779831f638b1fb1b96703e063cb56f5c6e8df5e8a09a80a693  Sources.Bindings.txt

DIRECTORIES:
  Evidence/
  Inputs/
