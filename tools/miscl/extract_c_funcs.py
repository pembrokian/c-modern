import os
import re
import sys

SOURCE_EXTS = ['.mc']

# fixed output dir and file
OUTPUT_DIR = 'tools/miscl/extracted_c_funcs'
DECL_KIND = 'FUNCTION_DECL'
STRUCT_KIND = 'STRUCT_DECL'

OPENERS = {'(': ')', '[': ']', '<': '>'}
CLOSERS = {')': '(', ']': '[', '>': '<'}


def strip_comments(line, in_block_comment):
    i = 0
    result = []

    while i < len(line):
        if in_block_comment:
            end = line.find('*/', i)
            if end == -1:
                return ''.join(result), True
            i = end + 2
            in_block_comment = False
            continue

        if line.startswith('//', i):
            break

        if line.startswith('/*', i):
            in_block_comment = True
            i += 2
            continue

        result.append(line[i])
        i += 1

    return ''.join(result), in_block_comment


def split_top_level(text, delimiter=','):
    parts = []
    start = 0
    stack = []

    for index, ch in enumerate(text):
        if ch in OPENERS:
            stack.append(ch)
            continue

        if ch in CLOSERS:
            if stack and stack[-1] == CLOSERS[ch]:
                stack.pop()
            continue

        if ch == delimiter and not stack:
            parts.append(text[start:index].strip())
            start = index + 1

    tail = text[start:].strip()
    if tail:
        parts.append(tail)

    return [part for part in parts if part]


def consume_balanced(text, start_index):
    if start_index >= len(text) or text[start_index] not in OPENERS:
        raise ValueError('expected balanced segment start')

    stack = [text[start_index]]
    index = start_index + 1

    while index < len(text):
        ch = text[index]
        if ch in OPENERS:
            stack.append(ch)
        elif ch in CLOSERS:
            if not stack or stack[-1] != CLOSERS[ch]:
                raise ValueError('unbalanced delimiter in function signature')
            stack.pop()
            if not stack:
                return text[start_index + 1:index], index + 1
        index += 1

    raise ValueError('unterminated balanced segment in function signature')


def parse_param(param_text):
    param_text = param_text.strip()
    if not param_text:
        return None

    if ':' not in param_text:
        return param_text, ''

    name, type_text = param_text.split(':', 1)
    return type_text.strip(), name.strip()


def parse_func_signature(signature_text):
    text = signature_text.strip()
    if text.startswith('extern '):
        text = text[len('extern '):].lstrip()

    if not text.startswith('func '):
        raise ValueError('expected function declaration')

    text = text[len('func '):].lstrip()
    match = re.match(r'[A-Za-z_][A-Za-z0-9_]*', text)
    if not match:
        raise ValueError('expected function name')

    name = match.group(0)
    remainder = text[match.end():].lstrip()

    if remainder.startswith('<'):
        _, remainder_index = consume_balanced(remainder, 0)
        remainder = remainder[remainder_index:].lstrip()

    if not remainder.startswith('('):
        raise ValueError('expected parameter list')

    params_text, remainder_index = consume_balanced(remainder, 0)
    remainder = remainder[remainder_index:].lstrip()
    params = []
    for param in split_top_level(params_text):
        parsed_param = parse_param(param)
        if parsed_param is not None:
            params.append(parsed_param)

    return {
        'kind': DECL_KIND,
        'name': name,
        'return': remainder.strip(),
        'params': params,
    }


def parse_struct_signature(signature_text):
    text = signature_text.strip()
    if not text.startswith('struct '):
        raise ValueError('expected struct declaration')

    text = text[len('struct '):].lstrip()
    match = re.match(r'[A-Za-z_][A-Za-z0-9_]*', text)
    if not match:
        raise ValueError('expected struct name')

    return {
        'kind': STRUCT_KIND,
        'name': match.group(0),
        'return': '',
        'params': [],
    }


def collect_signature(lines, start_index):
    collected = []
    in_block_comment = False
    end_index = start_index

    while end_index < len(lines):
        cleaned, in_block_comment = strip_comments(lines[end_index], in_block_comment)
        collected.append(cleaned)
        if '{' in cleaned:
            break
        end_index += 1

    return '\n'.join(collected), end_index


def find_matching_brace(lines, start_index):
    in_block_comment = False
    depth = 0

    for line_index in range(start_index, len(lines)):
        cleaned, in_block_comment = strip_comments(lines[line_index], in_block_comment)
        for ch in cleaned:
            if ch == '{':
                depth += 1
            elif ch == '}':
                depth -= 1
                if depth == 0:
                    return line_index

    return len(lines) - 1


def extract_declarations_from_file(file_path):
    with open(file_path, 'r') as source:
        lines = source.readlines()

    declarations = []
    index = 0
    in_block_comment = False

    while index < len(lines):
        cleaned, in_block_comment = strip_comments(lines[index], in_block_comment)
        stripped = cleaned.lstrip()

        if not (stripped.startswith('func ') or stripped.startswith('extern func ') or stripped.startswith('struct ')):
            index += 1
            continue

        signature_text, signature_end = collect_signature(lines, index)
        if '{' not in signature_text:
            index = signature_end + 1
            continue

        signature_prefix = signature_text.split('{', 1)[0]
        if stripped.startswith('struct '):
            declaration = parse_struct_signature(signature_prefix)
        else:
            declaration = parse_func_signature(signature_prefix)
        declaration['line'] = index + 1
        declaration['column'] = len(cleaned) - len(stripped) + 1
        declarations.append(declaration)

        body_end = find_matching_brace(lines, signature_end)
        index = body_end + 1

    declarations.sort(key=lambda item: (item['line'], item['column'], item['kind'], item['name']))
    return declarations


def scan_dir(dir_path):
    files = []
    for root, _, filenames in os.walk(dir_path):
        for filename in sorted(filenames):
            if any(filename.endswith(ext) for ext in SOURCE_EXTS):
                files.append(os.path.join(root, filename))

    return sorted(files)


def write_output(results, output_file):
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    out_path = os.path.join(OUTPUT_DIR, output_file)

    with open(out_path, 'w') as out:
        for file_path, declarations in results.items():
            out.write(f"==== {file_path} ====\n")
            for item in declarations:
                out.write(f"{item['kind']}: {item['name']}\n")
                if item['kind'] == DECL_KIND:
                    if item['return']:
                        out.write(f"   Return: {item['return']}\n")
                    else:
                        out.write("   Return: <none>\n")
                    for param_type, param_name in item['params']:
                        if param_name:
                            out.write(f"   Param: {param_type} {param_name}\n")
                        else:
                            out.write(f"   Param: {param_type}\n")
            out.write("\n")

    print(f"Wrote output to {out_path}")


def output_file_name(source_dir):
    dir_name = os.path.basename(os.path.normpath(source_dir))
    return f"{dir_name}_extracted_funcs.txt"


def main():
    if len(sys.argv) != 2:
        print('Usage: python extract_c_funcs.py <source_dir>')
        return

    source_dir = sys.argv[1]
    if not os.path.isdir(source_dir):
        print(f'Error: {source_dir} is not a valid directory')
        return

    files = scan_dir(source_dir)
    results = {}
    output_file = output_file_name(source_dir)

    for file_path in files:
        results[file_path] = extract_declarations_from_file(file_path)

    write_output(results, output_file)


if __name__ == '__main__':
    main()