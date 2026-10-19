REQUIRED SCRIPTS: Senses.Assembly.c | Senses.Assembly.manifest.txt | Sources.Bindings.txt | Native.Run.txt | Inputs/document.ppm | Inputs/atlas.ppm | Inputs/sound.wav
# Senses-Assembly-v1.0.2-2026-10-19

FAMILY: Senses
TOOL: Assembly
VERSION: 1.0.2
DATE: 2026-10-19
ENTRY_KIND: manifest
ENTRY: Senses.Assembly.manifest.txt
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 -DSHAKTI_APP_NO_MAIN -Iinclude SECTION-G/MCP/5-Menu/TOOLS/Senses/Senses-Assembly-v1.0.2-2026-10-19/Senses.Assembly.c @Runtime.Sources.txt -o shakti-senses
COMMAND: ./shakti-senses --run "{new_output_directory}" "{image_pnm}" "{wav_or_minus}" "{atlas_or_minus}" "{alphabet}" "{cell_width}" "{cell_height}"
EXAMPLE: ./shakti-senses --run /tmp/senses-output SECTION-G/MCP/5-Menu/TOOLS/Senses/Senses-Assembly-v1.0.2-2026-10-19/Inputs/document.ppm SECTION-G/MCP/5-Menu/TOOLS/Senses/Senses-Assembly-v1.0.2-2026-10-19/Inputs/sound.wav SECTION-G/MCP/5-Menu/TOOLS/Senses/Senses-Assembly-v1.0.2-2026-10-19/Inputs/atlas.ppm " 0123456789+=ABC" 24 32

INTRODUCTION:
Run typed image and PCM files through shared tagged preparation, exact reconstruction, heartbeat publication, supplied-font OCR and existing memory; includes the approved full audio/visual evidence.

PACKAGE DETAILS: Senses-Assembly-v1.0.2-2026-10-19.txt

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
  Evidence/Rebuilt.wav
  Evidence/Recorded-Voice-Rebuilt.wav
  Evidence/Recorded-Voice.txt
  Inputs/Fixture.txt

FILE HASHES:
2e5da1effaa165069aa3b926eff64531230425ad7b09b29fb6cb161d9c191edd  Evidence/COMPLETE.txt
d02e7a489ccb14786272b45d06c896f2af906eb74b5c7abcf69d9ec2a3e364dc  Evidence/Capsule.txt
52864817ba5216c00b0bc3d179e983c2fbf795bbdc43794caf5b9e51b68180e0  Evidence/Characters.txt
9c861498d0575229ec11564b74abc1115e62c0c4bad9350d2d14f46f2fe80aa0  Evidence/Colors-Rebuilt.png
c836897f6f9d7d27fd4478d9f4ee1acb289564a81f4ca61d5252ddd6dbd74daf  Evidence/Frames.txt
7cf29f94e1cbdce1178398641268abe32f37c09732e97e48eb4adcc1e1269c34  Evidence/Memory.txt
e90e2a87b55b194d9f8e7be04309ccfcbf2a26dc5cb710963fbdf7d0f85c4386  Evidence/OCR.txt
70571bea71c27c16911fc500fd5d1bf3f018c14bba044804e3c1498b638afd71  Evidence/Original.png
9aa6806d1df5c03acdf2c3815edaa48fddbfe4626fc5271a2f12c162fd13f99e  Evidence/Planes.bin
70571bea71c27c16911fc500fd5d1bf3f018c14bba044804e3c1498b638afd71  Evidence/Rebuilt.png
5ca3272948f3a4c63ef3aae127799a4ee2f3e5893ab8bb52f234389c4bd0c37b  Evidence/Rebuilt.wav
70e26a1df37e1b89935dc2d6b0cedb8b1224ddbd95ea811bdbfbdaefac221165  Evidence/Recorded-Voice-Rebuilt.wav
8ee453046d25bb0350032e4faf33495019c184a41ce6ae3c482c14d8db0b4525  Evidence/Recorded-Voice.txt
767063d7c8d335cf2d180c1b297876b80abbbac93989bf543ce17a36543dbb96  Inputs/Fixture.txt
344a9edc1f97c4e2c4247969637ac15e7001cc2fa012326ee7bb62215bae4e6b  Inputs/atlas.ppm
d5a0a0a361f7d0b64734789fa5a599d9782ae440b60e3650b6a5722a2b23925e  Inputs/document.ppm
5ca3272948f3a4c63ef3aae127799a4ee2f3e5893ab8bb52f234389c4bd0c37b  Inputs/sound.wav
bde60622060dce162d207b6417ba8ca8123428260a85476e1752244385f84fe6  Native.Run.txt
3aab78b97756395941a3975ff74e72bcfc2adf8e74386e94b728da058f65a20c  Senses-Assembly-v1.0.2-2026-10-19.txt
3a362f9da3395bd15efde94223640a668b42d92acf51fbc9dc87c7339d90ffaa  Senses.Assembly.c
f2fba2d3ce2a4076b154b8e38db46646a0af21c398b56719efb3f9858df17e4b  Senses.Assembly.manifest.txt
e29eaae0615f5ae9fc482d0f5d5e070e700472de5da27f1a9ec511c5cef02dc9  Sources.Bindings.txt

DIRECTORIES:
  Evidence/
  Inputs/
