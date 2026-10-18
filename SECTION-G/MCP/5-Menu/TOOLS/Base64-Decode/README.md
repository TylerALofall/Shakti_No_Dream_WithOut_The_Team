BUILD:
From this directory, with a C99 compiler:
cc -std=c99 -pedantic -Wall -Wextra -Werror -O2 Base64.Decode.c -o shakti-decode

RUN WITH YOUR FILES:
./shakti-decode -- "{input_file}" "{output_file}"
Replace {input_file} with the path to Base64-encoded text. Replace
{output_file} with the path for the decoded bytes. Keep the quotation marks
when a path contains spaces. An existing output file is replaced.

RUN THE INCLUDED EXAMPLE:
./shakti-decode -- Base64.Sample.Input.txt Base64.Sample.Output.bin
The input contains U2hha3RpAP8= and a trailing newline. The result is eight
bytes: 53 68 61 6b 74 69 00 ff (hexadecimal). That is Shakti followed by a
zero byte and a byte with value 255. Base64.Sample.Expected.bin contains
those expected bytes for comparison with Base64.Sample.Output.bin.

FILE ROLES:
Base64.Decode.c is the single decoder source file.
Base64.Sample.Input.txt is the Base64 text to decode.
Base64.Sample.Expected.bin is decoded example data, not an executable.
The .bin extension is only a filename convention; Base64 text is the input
format. The package supplies portable source; the build command creates a
program for the platform running the compiler.
