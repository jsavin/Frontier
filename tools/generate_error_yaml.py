#!/usr/bin/env python3
"""
Generate complete langerrorlist.yaml from legacy resource files.

Extracts error constant names from Common/headers/langinternal.h
and error messages from legacy Frontier's Mac resources.
"""

import re
import sys
from pathlib import Path

def extract_error_constants(langinternal_path):
    """Extract error constant definitions from langinternal.h"""
    error_map = {}

    with open(langinternal_path, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    # Find all #define lines with error constants
    # They appear between #define langerrorlist 257 and #define langstacklist 137
    in_error_section = False

    for line in lines:
        # Start of error section
        if '#define langerrorlist 257' in line:
            in_error_section = True
            continue

        # End of error section
        if '#define langstacklist' in line:
            break

        if in_error_section:
            # Match #define lines with error-related constants
            # Format: #define constantname number [/*comment*/]
            # All constants in this section are error-related
            match = re.match(r'#define\s+(\w+)\s+(\d+)', line)
            if match:
                constant_name = match.group(1)
                error_number = int(match.group(2))
                error_map[error_number] = constant_name

    return error_map

def extract_error_messages(lang_r_path):
    """Extract error message strings from lang.r resource file"""
    messages = {}

    with open(lang_r_path, 'r', encoding='utf-8', errors='ignore') as f:
        lines = f.readlines()

    # Find the STR# resource 257 section
    in_error_section = False
    current_index = 0

    for i, line in enumerate(lines):
        # Look for the start of the error list resource
        if "resource 'STR#' (257," in line:
            in_error_section = True
            current_index = 0
            continue

        # End of this resource
        if in_error_section and line.strip() == '};':
            break

        # Extract error messages
        if in_error_section:
            # Look for /* [N] */ or // [N] style markers
            index_match = re.match(r'\s*(?:/\*\s*\[(\d+)\]?\s*\*/|//\s*\[(\d+)\])', line)
            if index_match:
                current_index = int(index_match.group(1) or index_match.group(2))
                # Check if string is on same line
                # Must handle escaped quotes within the string: \"
                string_on_same_line = re.search(r'"((?:[^"\\]|\\.)*)"', line)
                if string_on_same_line:
                    message = string_on_same_line.group(1)
                    # Clean up message
                    message = clean_message(message)
                    messages[current_index] = message
                continue

            # Look for the actual string on its own line
            # Must handle escaped quotes within the string: \"
            string_match = re.match(r'\s*"((?:[^"\\]|\\.)*)"', line)
            if string_match and current_index > 0:
                message = string_match.group(1)
                # Clean up message
                message = clean_message(message)
                messages[current_index] = message

    return messages

def clean_message(message):
    """Clean up error message text for YAML"""
    # Remove trailing comments
    message = re.sub(r',?\s*//.*$', '', message)

    # Convert MacRoman smart quotes to ASCII
    message = message.replace('�', "'")  # Right single quote
    message = message.replace('�', '"')  # Left double quote
    message = message.replace('�', '"')  # Right double quote

    # Handle escaped quotes in the source
    # First, convert \" to a temporary marker
    message = message.replace('\\"', '\x00QUOTE\x00')

    # Convert octal escape sequences
    message = message.replace('\\042', '"')

    # Now escape any remaining quotes for YAML
    message = message.replace('"', '\\"')

    # Restore the originally-escaped quotes
    message = message.replace('\x00QUOTE\x00', '\\\\\\"')

    return message

def generate_yaml(error_constants, error_messages, output_path):
    """Generate the complete YAML file"""

    with open(output_path, 'w', encoding='utf-8') as f:
        f.write("langerrorlist:\n")

        # Process all errors in order
        max_error = max(max(error_constants.keys()), max(error_messages.keys()))

        for i in range(1, max_error + 1):
            constant_name = error_constants.get(i, f"error{i}")
            message = error_messages.get(i, f"Error {i}.")

            # Clean up constant name (remove 'error' suffix for consistency)
            # But keep full name for exact matching

            f.write(f"  - id: {constant_name}\n")
            f.write(f"    index: {i}\n")
            f.write(f'    text: "{message}"\n')

def main():
    # Paths
    project_root = Path(__file__).parent.parent
    langinternal_h = project_root / "Common/headers/langinternal.h"
    lang_r = Path("/Users/jake/dev/tedchoward/Frontier/Common/resources/Mac/lang.r")
    output_yaml = project_root / "resources/strings/langerrorlist.yaml"

    print("Extracting error constants from langinternal.h...")
    error_constants = extract_error_constants(langinternal_h)
    print(f"Found {len(error_constants)} error constant definitions")

    print("\nExtracting error messages from lang.r...")
    error_messages = extract_error_messages(lang_r)
    print(f"Found {len(error_messages)} error messages")

    print(f"\nGenerating {output_yaml}...")
    generate_yaml(error_constants, error_messages, output_yaml)

    print(f"\nSuccess! Generated {output_yaml}")
    print(f"Total entries: {max(max(error_constants.keys()), max(error_messages.keys()))}")

    # Validation check
    missing_constants = set(error_messages.keys()) - set(error_constants.keys())
    missing_messages = set(error_constants.keys()) - set(error_messages.keys())

    if missing_constants:
        print(f"\nWarning: {len(missing_constants)} messages without constant definitions:")
        print(f"  Indices: {sorted(missing_constants)}")

    if missing_messages:
        print(f"\nWarning: {len(missing_messages)} constants without messages:")
        for idx in sorted(missing_messages):
            print(f"  {idx}: {error_constants[idx]}")

if __name__ == "__main__":
    main()
