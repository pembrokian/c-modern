import os
import sys
from clang.cindex import Config, CursorKind, Index

SOURCE_EXTS = ['.c']
# SOURCE_DIRS = ['runtime']
SOURCE_DIRS = ['tests/tool']
LIBCLANG_CANDIDATES = [
    os.environ.get('LIBCLANG_FILE'),
    '/opt/homebrew/Cellar/llvm@21/21.1.8/lib/libclang.dylib',
    '/Library/Developer/CommandLineTools/usr/lib/libclang.dylib',
    '/Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/lib/libclang.dylib',
    '/Applications/Xcode.app/Contents/Frameworks/libclang.dylib',
]

# fixed output dir and file
OUTPUT_DIR = 'tools/decl_extraction/extracted_c_decs'
DECL_KINDS = {
    CursorKind.FUNCTION_DECL,
    CursorKind.NAMESPACE,
    CursorKind.STRUCT_DECL,
    CursorKind.CLASS_DECL,
    CursorKind.UNION_DECL,
    CursorKind.TYPEDEF_DECL,
    CursorKind.TYPE_ALIAS_DECL,
    CursorKind.ENUM_DECL,
}


def configure_libclang():
    for candidate in LIBCLANG_CANDIDATES:
        if candidate and os.path.exists(candidate):
            Config.set_library_file(candidate)
            return candidate

    raise RuntimeError('Unable to locate libclang.dylib; set LIBCLANG_FILE to a valid path')


def extract_declarations_from_file(file_path):
    index = Index.create()
    tu = index.parse(file_path)
    declarations = []

    for cursor in tu.cursor.walk_preorder():
        if cursor.kind not in DECL_KINDS:
            continue

        if not cursor.location.file or os.path.abspath(cursor.location.file.name) != os.path.abspath(file_path):
            continue

        declaration = {
            'kind': cursor.kind.name,
            'name': cursor.spelling,
            'line': cursor.location.line,
            'column': cursor.location.column,
        }

        if cursor.kind == CursorKind.FUNCTION_DECL:
            declaration['return'] = cursor.result_type.spelling
            declaration['params'] = [
                (argument.type.spelling, argument.spelling)
                for argument in cursor.get_arguments() or []
            ]

        declarations.append(declaration)

    declarations.sort(key=lambda item: (item['line'], item['column'], item['kind'], item['name']))

    return declarations


def scan_dir(dir_path):
    files = []
    for root, _, filenames in os.walk(dir_path):
        for f in sorted(filenames):
            if any(f.endswith(ext) for ext in SOURCE_EXTS):
                files.append(os.path.join(root, f))

    return sorted(files)


def discover_source_dirs(root_dir):
    subdirs = []
    with os.scandir(root_dir) as entries:
        for entry in entries:
            if entry.is_dir():
                subdirs.append(entry.path)

    return sorted(subdirs)


def output_file_name(source_dir):
    dir_name = os.path.basename(os.path.normpath(source_dir))
    return f"{dir_name}_extracted_decs.txt"


def write_output(results, output_file):
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    out_path = os.path.join(OUTPUT_DIR, output_file)

    with open(out_path, 'w') as out:
        for file, funcs in results.items():
            out.write(f"==== {file} ====\n")
            for item in funcs:
                out.write(f"{item['kind']}: {item['name']}\n")
                if item['kind'] == CursorKind.FUNCTION_DECL.name:
                    out.write(f"   Return: {item['return']}\n")
                    for t, n in item['params']:
                        out.write(f"   Param: {t} {n}\n")
            out.write("\n")

    print(f"Wrote output to {out_path}")


def main():
    if len(sys.argv) != 1:
        print('Usage: python extract_c_decs.py')
        return

    configure_libclang()

    outputs = {}

    for source_dir in SOURCE_DIRS:
        if not os.path.isdir(source_dir):
            print(f'Error: {source_dir} is not a valid directory')
            return

        for child_dir in discover_source_dirs(source_dir):
            results = {}
            for file_path in scan_dir(child_dir):
                results[file_path] = extract_declarations_from_file(file_path)
            outputs[output_file_name(child_dir)] = results

    for output_file, results in outputs.items():
        write_output(results, output_file)


if __name__ == '__main__':
    main()