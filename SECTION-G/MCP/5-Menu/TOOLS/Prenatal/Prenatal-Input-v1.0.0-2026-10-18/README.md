REQUIRED SCRIPTS: Prenatal.Input.manifest.txt | Prenatal.Run.c | Prenatal.Profile.xml | Catalog.xml | Source.Bindings.xml | Guide.md | Build_Run_Record.txt | Intake.Receipts.txt | Run.stdout.txt | Run.stderr.txt | Assets/black.wav | Assets/red.wav | Assets/green.wav | Assets/yellow.wav | Assets/blue.wav | Assets/magenta.wav | Assets/cyan.wav | Assets/white.wav | Assets/pulse.wav
# Prenatal-Input-v1.0.0-2026-10-18

FAMILY: Prenatal
TOOL: Input
VERSION: 1.0.0
DATE: 2026-10-18
ENTRY_KIND: manifest
ENTRY: Prenatal.Input.manifest.txt
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 -Iinclude -ISECTION-A {package_dir}/Prenatal.Run.c SECTION-B/shakti_prenatal_loader.c SECTION-A/shakti_*.c -o prenatal-run
COMMAND: ./prenatal-run --run {assets_dir} {profile} {new_fixture}
EXAMPLE: ./prenatal-run --run SECTION-G/MCP/5-Menu/TOOLS/Prenatal/Prenatal-Input-v1.0.0-2026-10-18/Assets SECTION-G/MCP/5-Menu/TOOLS/Prenatal/Prenatal-Input-v1.0.0-2026-10-18/Prenatal.Profile.xml /tmp/shakti-prenatal-new-fixture

INTRODUCTION:
Prepare bounded prenatal sight and sound inputs, preserve original voice samples, and deposit into the existing native intake. The existing workflow owns binary reconstruction, convergence, progression, memory and release. Includes eight original color recordings, one quiet reusable heartbeat atom, an adjustable XML profile, source bindings and native qualification evidence. Native host bridge, simultaneous independent heartbeat port and device output remain integration work.

PACKAGE DETAILS: Prenatal-Input-v1.0.0-2026-10-18.txt

FILE HASHES:
74b3263c961c5b370f26ffe71615db9d969483c398e97f1599573f6344133ed1  Assets/black.wav
c49d49982dcafa88ec1bffdd87b6a91a0d940b3266084b996126d0c026b95872  Assets/blue.wav
7cf7f86829fe96b62f709b14e3f528c08341ced12ac0810d01d82ec703885a38  Assets/cyan.wav
c23995bd33e296498100c1a02114ea5b81e220f24bd41bd6f354b73e88c8afc8  Assets/green.wav
28acb2bc60b4b85a4936837770c6dbe4ae3ebee2b4c6399ce4af96ee4dfb8538  Assets/magenta.wav
8905ce8c277fcdbf9a3730d1b4b72b80b98c4c6630e9a960216036e0453a5554  Assets/pulse.wav
0e08c0d5c038921f0d930652aee951016305be2abe739143bfa3c98c57f90aae  Assets/red.wav
73c6d5c769f785c96e94dbd97662ab0e509b8eadedca3a5b893ba4a2d797815e  Assets/white.wav
bc04437c3a58291045ccddc4248ffbc5ddd77297ee4e5d8f34d499b3703e158f  Assets/yellow.wav
75789545694d5c13b14fe4bfea9ffe1aa4e90204f1710938df42bf79451a7492  Build_Run_Record.txt
cf291b2bd18b77768d0f0c3c6ef658aeccf942e2658ad94becee85145eb41aea  Catalog.xml
1ce55fd7542d5646c51a4f7c73d4b64f8c62e836acec01c384597be1eaeb57c4  Guide.md
441d83871b90949f36ce72d3e5a6d2f7bf3ae2fb5cdd738bddc7515815d609e9  Intake.Receipts.txt
deb411d3755b71781f9f30354fa520e6c3853d424315842d743dd4f47b4825f1  Prenatal-Input-v1.0.0-2026-10-18.txt
f5f94187b9af10a01734f2620486508e21a6c74c89c3ef089bede469797ef344  Prenatal.Input.manifest.txt
8d0a4692bb716565a6b33b4f4a6d1c8e8b6febad30a7d3ad6d5825e04c96c46b  Prenatal.Profile.xml
ae6974caa55c9bbf887352dc021405b69a3bf83bd1aea1756e59d7f39d63d886  Prenatal.Run.c
02ff568087234fedddc205d296be0aea37690da5133b884c163665a97fbe0678  Run.stderr.txt
749443b73a22702b658759b41b2fbf1db4c6dea6b8f255857dcefc68f0daedb2  Run.stdout.txt
ca4ba95b844a369c23dc43346f9d719268115138b2179ff8a697086153013e05  Source.Bindings.xml

DIRECTORIES:
  Assets/
