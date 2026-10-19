REQUIRED SCRIPTS: shakti_prenatal_flow.c | shakti_prenatal_loader.c | shakti_prenatal_flow.h | shakti_prenatal_loader.h | Prenatal.Flow.Run.c | Prenatal.Loader.manifest.txt | Prenatal.Profile.xml | Assets/black.wav | Assets/red.wav | Assets/green.wav | Assets/yellow.wav | Assets/blue.wav | Assets/magenta.wav | Assets/cyan.wav | Assets/white.wav | Assets/pulse.wav | Sources.Bindings.txt | Native.Run.txt | Prenatal.Loader.Guide.txt
# Prenatal-Loader-v2.0.0-2026-10-19

FAMILY: Prenatal
TOOL: Loader
VERSION: 2.0.0
DATE: 2026-10-19
ENTRY_KIND: manifest
ENTRY: Prenatal.Loader.manifest.txt
BUILD: cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 -DSHAKTI_PRENATAL_RUN_HOOKS -Iinclude -ISECTION-A SECTION-G/MCP/5-Menu/TOOLS/Prenatal/Prenatal-Loader-v2.0.0-2026-10-19/Prenatal.Flow.Run.c SECTION-A/shakti_*.c SECTION-B/shakti_prenatal_loader.c -o shakti-prenatal-run
COMMAND: ./shakti-prenatal-run --run "{new_fixture_directory}" "{absolute_assets_directory}" "{absolute_profile_path}"
EXAMPLE: ./shakti-prenatal-run --run /tmp/shakti-prenatal-v2-001 /tmp/shakti-prenatal-v2-package/Assets /tmp/shakti-prenatal-v2-package/Prenatal.Profile.xml

INTRODUCTION:
Drive the resident recorded prenatal source collection through the continuous heart-owned C loading zone, full binary reconstruction and shared convergence; native synthetic RUN includes live replay and whole-memory preservation.

PACKAGE DETAILS: Prenatal-Loader-v2.0.0-2026-10-19.txt

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
4d5f2edafcc0e60b2005d916a5e105af05ecd668973fa4d46d96886e289d7d9b  Native.Run.txt
6ed09e9f806d06e39b2dd94d06c9420302138b4196d0465df5b0abefe33a79d0  Prenatal-Loader-v2.0.0-2026-10-19.txt
de428aef5cb9c7741f3730ffcc5356acb8a49fa537497600bbfd83cf054a4379  Prenatal.Flow.Run.c
cd0eac7003d7ebff871549a8fb89ea4d2875a01602ef1601b8ece194a2f104d6  Prenatal.Loader.Guide.txt
9b1cbfcede755fad98f9d5d4df279eee08b0ff3f99306e8b174d45e93cfd8cc4  Prenatal.Loader.manifest.txt
ab3389c62412cbad3395e338293d7febc691299fa7cf749b206c13ba06268fc0  Prenatal.Profile.xml
48bf4cfc71c2fb66ccd3db0e1b5d6c4dd6ec78d2be625828d6dbe89ca3810a74  Sources.Bindings.txt
01879699d8d8cff26c85119c0e9b843325f527838f4b7f0e8108c6424b2e234e  shakti_prenatal_flow.c
a5f708bf92fd28fd14828905b9d7d989dcfd72b0a65552cfea8441126477a32b  shakti_prenatal_flow.h
d892aa670b64750963b66dc307872df1e78ac04c25ef6f5da7c1ca4dabf80d08  shakti_prenatal_loader.c
0e55ab6b5aa54ac9d383fd6f8ae8cb7f8a71e385b7ece97765736b87eb51d843  shakti_prenatal_loader.h

DIRECTORIES:
  Assets/
