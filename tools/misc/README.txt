
TOOLS/MISC EXTRACTORS - README

Overview:
	Purpose: Small utilities to extract top-level function and struct
	declarations from source trees and emit compact declaration files
	for inspection or tooling.

Scripts:
	extract_mc_decs.py
		location: tools/misc/extract_mc_decs.py
		scans: kernel/src
		outputs: tools/misc/extracted_mc_decs/
		notes: writes one file per immediate child directory, named
					 <dir>_extracted_decs.txt (for example: shell_extracted_decs.txt)

	extract_cpp_decs.py
		location: tools/misc/extract_cpp_decs.py
		scans: compiler/
		outputs: tools/misc/extracted_decls/compiler_extracted_decs.txt

	extract_c_decs.py
		location: tools/misc/extract_c_decs.py
		scans: runtime/
		outputs: tools/misc/extracted_c_decs/
		notes: writes one file per immediate child directory, named
					 <dir>_extracted_decs.txt (for example: freestanding_extracted_decs.txt)

Outputs:
	Modern C:    tools/misc/extracted_mc_decs/   (per-directory files)
	Compiler:    tools/misc/extracted_decls/      (single compiler_extracted_decs.txt)
	Runtime C:   tools/misc/extracted_c_decs/     (per-directory files)

Quick run:
	Use the project's virtualenv Python interpreter so dependencies match.

	./.venv/bin/python tools/misc/extract_mc_decs.py
	./.venv/bin/python tools/misc/extract_cpp_decs.py
	./.venv/bin/python tools/misc/extract_c_decs.py

Notes:
	- The clang-based extractors (extract_cpp_decs.py, extract_c_decs.py)
		require libclang to be available. If a script fails to locate
		libclang, set the environment variable LIBCLANG_FILE to the full
		path of libclang.dylib before running. Example:

		export LIBCLANG_FILE=/path/to/libclang.dylib
		./.venv/bin/python tools/misc/extract_c_decs.py

	- To change which directories are scanned, edit the SOURCE_DIRS list
		near the top of the corresponding script.

Tips / Next steps:
	- Outputs are deterministic and can be used as fixtures; consider
		committing them if they are useful for tests or auditing.
	- If you have old files named *_funcs.txt, remove or ignore them to
		avoid confusion with the new *_decs.txt naming.

