import json
from io import TextIOWrapper
from typing import Any, Dict

import yaml

TAB = 4*' '

YAML_DUMP_ARGS = dict(
    encoding='utf-8',
    allow_unicode=True,
    sort_keys=False,
)

ENUM_START = """
#pragma once

// THIS FILE WAS AUTO-GENERATED.
enum {enum_name} {{
"""

STRINGS_START = """
#include "lang_autogen.h"

// THIS FILE WAS AUTO-GENERATED.
{string_array_specifiers}char *{string_array_name}[{string_count_label}] = {{
"""

CURLY_BRACE_END = "};\n"


# By default PyYAML quoting rules put multi-line strings in single quotes which is dumb
# This function fixes that
def represent_str(self, data: str):
    tag = 'tag:yaml.org,2002:str'
    style = None
    if '\n' in data:
        style = '|'
    else:
        style = ''

    return self.represent_scalar(tag, data, style=style)


yaml.add_representer(str, represent_str)


def make_header(base_obj: Dict[str, Any], output_file: TextIOWrapper) -> None:
    strings = base_obj['gui_strings']
    prefix = base_obj['enum_label_prefix']
    output_file.write(ENUM_START.format(**base_obj).lstrip())
    for i, string_def in enumerate(strings):
        explicit_val = '' if i != 0 else ' = 0'
        label = string_def['label']
        comment = string_def.get('comment', '')
        if comment:
            comment = f' // {comment}'

        output_file.write(f"{TAB}{prefix}{label}{explicit_val},{comment}\n")

    count_label = base_obj['string_count_label']
    output_file.write('\n')
    output_file.write(f"{TAB}{count_label},\n")
    output_file.write(CURLY_BRACE_END)


def make_source(base_obj: Dict[str, Any], output_file: TextIOWrapper) -> None:
    if base_obj['string_array_specifiers']:
        base_obj['string_array_specifiers'] += ' '

    output_file.write(STRINGS_START.format(**base_obj).lstrip())
    for string_def in base_obj['gui_strings']:
        string = string_def['string']
        # We use JSON to quote the string because JSON strings are equivalent to C strings
        output_file.write(f'{TAB}{json.dumps(string)},\n')

    output_file.write(CURLY_BRACE_END)


def make_template_yml(base_obj: Dict[str, Any], output_file: TextIOWrapper) -> None:
    output = {
        "comments": base_obj['comment_for_template_header'],
        "translations": {},
    }

    extra_comments = base_obj['comments_for_template_labels']
    for string_def in base_obj['gui_strings']:
        label = string_def['label']
        output["translations"][label] = [
            "untranslated",
            {"original": string_def['string']},
        ]
        if label in extra_comments:
            output["translations"][label].append({'comment': extra_comments[label]})

    yaml.dump(output, output_file, **YAML_DUMP_ARGS)


def update_translation_yml(base_obj: Dict[str, Any], translation_obj: Dict[str, Any], output_file: TextIOWrapper) -> None:
    for string_def in base_obj['gui_strings']:
        label = string_def['label']
        if label not in translation_obj['translations']:
            translation_obj['translations'][label] = [
                "untranslated",
                {"original": string_def['string']},
            ]

    yaml.dump(translation_obj, output_file, **YAML_DUMP_ARGS)


def make_lng(
    base_obj: Dict[str, Any],
    translation_obj: Dict[str, Any],
    output_file: TextIOWrapper,
    translation_filename: str,
) -> None:
    for comm_line in translation_obj['comments'].splitlines():
        output_file.write(f'# {comm_line}\n')

    for string_def in base_obj['gui_strings']:
        label = string_def['label']
        translation = translation_obj['translations'].get(label)
        if translation is None:
            #print(f'WARNING: translation for {label} not found in {translation_filename}', file=sys.stderr)
            translation = string_def['string']
        if not isinstance(translation, str):
            if "untranslated" not in translation and "same" not in translation:
                print(f'WARNING: status unknown for {label} in {translation_filename}', file=sys.stderr)

            translation = string_def['string']

        output_file.write(f'{translation}\n')


if __name__ == '__main__':
    import argparse
    import sys
    parser = argparse.ArgumentParser(description='Compile YAML language files into source and .lng files')
    action_group = parser.add_mutually_exclusive_group(required=True)
    action_group.add_argument('--make_header', action='store_true')
    action_group.add_argument('--make_source', action='store_true')
    action_group.add_argument('--make_template_yml', action='store_true')
    action_group.add_argument('--update_translation_yml', action='store_true')
    action_group.add_argument('--make_lng', action='store_true')

    parser.add_argument('--base', type=argparse.FileType('r', encoding='utf-8'), required=True, metavar="BASE_YML")
    parser.add_argument('--translation', type=argparse.FileType('r+', encoding='utf-8'), metavar="LANG_YML")
    parser.add_argument('output_file', nargs='?', type=argparse.FileType('w', encoding='utf-8'), default=sys.stdout)

    args = parser.parse_args()
    if args.translation is None:
        if args.make_lng or args.update_translation_yml:
            option = ''
            if args.make_lng:
                option = 'make_lng'
            elif args.update_translation_yml:
                option = 'update_translation_yml'

            raise KeyError(f"Translation YML is required when --{option} option is selected")

    base_obj = yaml.safe_load(args.base)
    if args.make_header:
        make_header(base_obj, args.output_file)
    elif args.make_source:
        make_source(base_obj, args.output_file)
    elif args.make_template_yml:
        make_template_yml(base_obj, args.output_file)
    elif args.update_translation_yml:
        translation_obj = yaml.safe_load(args.translation)
        args.translation.seek(0)
        update_translation_yml(base_obj, translation_obj, args.translation)
        args.translation.truncate()
    elif args.make_lng:
        translation_obj = yaml.safe_load(args.translation)
        make_lng(base_obj, translation_obj, args.output_file, args.translation.name)
