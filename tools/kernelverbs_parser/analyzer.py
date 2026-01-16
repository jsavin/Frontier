#!/usr/bin/env python3
"""
Verb implementation analyzer.

Analyzes C source files to determine which kernel verbs are implemented
vs. stubbed, and detects Carbon dependencies and UI adapters.
"""

import os
import re
from typing import List, Optional, Dict
from pathlib import Path

from metadata_writer import VerbImplementation
from matchers import (
    detect_carbon_apis,
    detect_ui_adapters,
    detect_stub_verb,
    parse_annotations,
    estimate_complexity
)
from verb_exceptions import (
    get_pattern_c_exception,
    is_pattern_d_processor,
    generate_pattern_d_candidates
)
from script_implemented_verbs import is_script_implemented

# Constants for validation and limits
MIN_VERB_NAME_LENGTH = 2  # Minimum characters for a valid verb name
# MIN_EXTRACTED_VERBS prevents false positives in enum extraction:
# - Processors with 1 verb are extremely rare (none in the 51 we analyzed)
# - Single matches often indicate partial pattern detection (e.g., matching "table" in comments)
# - Requiring 2+ matches indicates the pattern succeeded across multiple enum entries
MIN_EXTRACTED_VERBS = 2

class VerbImplementationAnalyzer:
    """
    Analyzes verb implementations across all processors.
    """

    def __init__(self, processors: List, build_target: str = 'headless'):
        """
        Initialize analyzer with processor definitions.

        Args:
            processors: List of EFPProcessor objects from parse_kernelverbs.py
            build_target: Target build ('headless' or 'legacy'). Default: 'headless'
                         'headless' = analyze only files linked in frontier-cli (headless build)
                         'legacy' = analyze full codebase including GUI implementations
        """
        self.processors = processors
        self.implementations = []
        self._file_cache: Dict[str, str] = {}  # Cache file contents to avoid redundant I/O
        self.build_target = build_target
        self._build_sources: Optional[set] = None  # Cached set of source files in build

    def _parse_makefile_with_vars(self, makefile_path: Path, project_root: Path, parent_variables: Optional[dict] = None, visited: Optional[set] = None) -> tuple:
        """
        Internal helper that parses Makefile and returns both sources and variables.

        Args:
            makefile_path: Path to Makefile
            project_root: Project root directory
            parent_variables: Variables from parent Makefile (for recursive calls)
            visited: Set of already-visited Makefile paths (prevents circular includes)

        Returns:
            Tuple of (sources_set, variables_dict)
        """
        # Initialize visited set on first call
        if visited is None:
            visited = set()

        # Resolve path to canonical form for circular include detection
        makefile_canonical = makefile_path.resolve()

        # Check for circular includes
        if makefile_canonical in visited:
            return set(), {}

        # Mark this file as visited
        visited.add(makefile_canonical)

        sources = set()
        # Start with parent variables (from outer scope) and add new ones
        variables = parent_variables.copy() if parent_variables else {}

        def expand_variables(text: str) -> str:
            """Expand $(VAR) references in text using tracked variables."""
            import re
            def replace_var(match):
                var_name = match.group(1)
                return variables.get(var_name, match.group(0))  # Return original if not found
            return re.sub(r'\$\((\w+)\)', replace_var, text)

        try:
            with open(makefile_path, 'r') as f:
                in_sources = False
                in_var_assignment = False
                current_var_name = None
                current_var_values = []

                for line in f:
                    stripped = line.strip()

                    # Parse variable assignments (e.g., TESTSDIR = tests)
                    if '=' in line and not stripped.startswith('#'):
                        # Start of variable assignment
                        parts = stripped.split('=', 1)
                        if len(parts) == 2:
                            var_name = parts[0].strip()
                            var_value = parts[1].strip()

                            if '\\' in var_value:
                                # Multi-line variable assignment starting
                                in_var_assignment = True
                                current_var_name = var_name
                                current_var_values = [var_value.rstrip('\\').strip()]
                            else:
                                # Simple single-line assignment
                                variables[var_name] = var_value
                                in_var_assignment = False

                    elif in_var_assignment:
                        # Continuation of multi-line variable assignment
                        value = stripped.rstrip('\\').strip()
                        if value:
                            current_var_values.append(value)

                        # Check if this is the last line (no trailing backslash)
                        if not stripped.endswith('\\'):
                            # Store the accumulated values as space-separated list
                            variables[current_var_name] = ' '.join(current_var_values)
                            in_var_assignment = False
                            current_var_name = None
                            current_var_values = []

                    # Check for -include directives
                    if stripped.startswith('-include') or stripped.startswith('include'):
                        # Extract include path
                        # Example: "-include $(TESTSDIR)/headless_verbs.mk"
                        parts = stripped.split(maxsplit=1)
                        if len(parts) == 2:
                            include_path = parts[1].strip()

                            # Expand variables BEFORE resolving path
                            include_path = expand_variables(include_path)

                            # Resolve relative to Makefile's directory
                            include_abs = (makefile_path.parent / include_path).resolve()

                            # Security: Validate that included file is within project boundaries
                            try:
                                include_abs.relative_to(project_root)
                                is_within_project = True
                            except ValueError:
                                is_within_project = False

                            if not is_within_project:
                                print(f"  Warning: Included file outside project boundaries: {include_abs}")
                            elif include_abs.exists():
                                # Recursively parse included file, passing current variables and visited set
                                included_sources, included_vars = self._parse_makefile_with_vars(include_abs, project_root, variables, visited)
                                sources.update(included_sources)
                                # Merge variables from included file (included file's variables take precedence)
                                variables.update(included_vars)
                            else:
                                print(f"  Warning: Included file not found: {include_abs}")

                    # Check if we're starting a variable assignment block with .c files
                    # Matches: CLI_SOURCES, RUNTIME_SOURCES, HEADLESS_STUBS, DATABASE_SOURCES, etc.
                    if '=' in line and '\\' in line and not stripped.startswith('#'):
                        # This looks like the start of a multi-line variable assignment
                        in_sources = True

                    if in_sources:
                        # Extract .c file path
                        # Lines look like: "    ../Common/source/lang.c \"
                        # or: "    $(TESTSDIR)/headless_file_verbs.c \"
                        # or: "    $(addprefix $(TESTSDIR)/,$(HEADLESS_VERBS_SOURCES))"

                        # Handle $(addprefix prefix,list) function calls
                        import re
                        addprefix_match = re.search(r'\$\(addprefix\s+([^,]+),\s*\$\((\w+)\)\s*\)', stripped)
                        if addprefix_match:
                            prefix = addprefix_match.group(1).strip()
                            var_name = addprefix_match.group(2)

                            # Expand the prefix (may contain variables like $(TESTSDIR)/)
                            prefix = expand_variables(prefix)

                            # Get the list variable value (e.g., HEADLESS_VERBS_SOURCES)
                            if var_name in variables:
                                # The variable value should be a whitespace-separated list
                                file_list = variables[var_name].split()

                                # Add prefix to each file
                                for filename in file_list:
                                    path = prefix + filename

                                    # Resolve ../ prefix (all paths relative to makefile's directory)
                                    if path.startswith('../'):
                                        path = path[3:]  # Remove ../

                                    # Convert to absolute path (relative to project root)
                                    abs_path = (project_root / path).resolve()

                                    if abs_path.exists():
                                        sources.add(str(abs_path))

                        # Process regular .c files
                        elif '.c' in stripped:
                            # Extract path (remove trailing \ and whitespace)
                            path = stripped.rstrip('\\').strip()

                            # Expand variables (e.g., $(TESTSDIR) -> tests)
                            path = expand_variables(path)

                            # Resolve ../ prefix (all paths relative to makefile's directory)
                            if path.startswith('../'):
                                path = path[3:]  # Remove ../

                            # Convert to absolute path (relative to project root)
                            abs_path = (project_root / path).resolve()

                            if abs_path.exists():
                                sources.add(str(abs_path))

                        # End of SOURCES block if no trailing backslash
                        if not stripped.endswith('\\'):
                            in_sources = False

        except IOError as e:
            print(f"Error reading Makefile: {e}")

        return sources, variables

    def parse_makefile_sources(self, makefile_path: Optional[Path] = None) -> set:
        """
        Parse Makefile to extract actual source files in the build.

        Recursively follows -include directives to find all source files.

        Args:
            makefile_path: Path to Makefile (defaults to frontier-cli/Makefile)

        Returns:
            Set of absolute paths to .c files actually compiled in the build
        """
        # Use cached result for main Makefile
        if makefile_path is None and self._build_sources is not None:
            return self._build_sources

        script_dir = Path(__file__).parent
        project_root = script_dir.parent.parent

        if makefile_path is None:
            makefile_path = project_root / "frontier-cli/Makefile"

        if not makefile_path.exists():
            print(f"Warning: Makefile not found at {makefile_path}")
            if makefile_path == project_root / "frontier-cli/Makefile":
                self._build_sources = set()
            return set()

        # Call helper to get sources and variables
        sources, _ = self._parse_makefile_with_vars(makefile_path, project_root)

        # Cache result only for main Makefile
        if makefile_path == project_root / "frontier-cli/Makefile":
            self._build_sources = sources

        return sources

    def find_implementation_file(self, processor_name: str) -> Optional[str]:
        """
        Find the C source file containing verb implementations for a processor.

        For headless build (default):
            - Only returns files actually linked in frontier-cli/Makefile
            - Prefers tests/headless_*_verbs.c over Common/source/* files
            - Returns None if processor not in build

        For legacy build:
            - Returns any implementation file found (old behavior)
            - Searches Common/source/ first

        Args:
            processor_name: Name of the processor (e.g., "file", "frontier")

        Returns:
            Absolute path to implementation file, or None if not found
        """
        # Find project root (two levels up from tools/kernelverbs_parser)
        script_dir = Path(__file__).parent
        project_root = script_dir.parent.parent

        # Get actual build sources for headless mode
        build_sources = self.parse_makefile_sources() if self.build_target == 'headless' else None

        # Pattern D: Multi-processor consolidation
        # These processors are all implemented in langverbs.c
        # BUT: For headless mode, prefer headless stub files over langverbs.c if they exist
        if is_pattern_d_processor(processor_name) and self.build_target != 'headless':
            langverbs_path = project_root / "Common/source/langverbs.c"
            if langverbs_path.exists():
                abs_path = str(langverbs_path.absolute())
                # For headless, verify it's actually in the build
                if build_sources is None or abs_path in build_sources:
                    return abs_path

        # Build search patterns based on build target
        search_patterns = []

        # Special cases for both headless and legacy
        special_cases_headless = {
            'frontier': project_root / 'tests/headless_frontier_verbs.c',
            'sys': project_root / 'tests/headless_sys_verbs.c',
        }

        special_cases_legacy = {
            'frontier': project_root / 'Common/source/shellsysverbs.c',
            'sys': project_root / 'Common/source/shellsysverbs.c',
            'window': project_root / 'Common/source/shellwindowverbs.c',
            'opattributes': project_root / 'Common/source/opverbs.c',
        }

        if self.build_target == 'headless':
            # Headless: prefer headless stubs, then Common/source if actually linked
            if processor_name in special_cases_headless:
                search_patterns.append(special_cases_headless[processor_name])

            search_patterns.extend([
                project_root / f"tests/headless_{processor_name}_verbs.c",
                project_root / f"Common/source/{processor_name}verbs.c",
                project_root / f"Common/source/lang{processor_name}.c",
            ])
        else:
            # Legacy: prefer Common/source (GUI implementations)
            if processor_name in special_cases_legacy:
                search_patterns.append(special_cases_legacy[processor_name])

            search_patterns.extend([
                project_root / f"Common/source/{processor_name}verbs.c",
                project_root / f"Common/source/lang{processor_name}.c",
                project_root / f"tests/headless_{processor_name}_verbs.c",
            ])

        # Try each pattern
        for pattern in search_patterns:
            if pattern.exists():
                abs_path = str(pattern.absolute())

                # For headless build, verify file is actually linked
                if build_sources is not None:
                    if abs_path in build_sources:
                        return abs_path
                    # else: file exists but not in build, try next pattern
                else:
                    # Legacy mode: return first file found
                    return abs_path

        return None

    def read_source_file(self, file_path: str) -> str:
        """
        Read source file contents with caching.

        Uses file cache to avoid redundant I/O when multiple processors
        share the same implementation file (e.g., Pattern D processors in langverbs.c).

        Args:
            file_path: Path to source file

        Returns:
            File contents as string, or empty string if read fails
        """
        # Check cache first
        if file_path in self._file_cache:
            return self._file_cache[file_path]

        try:
            # Validate path is within project to prevent directory traversal
            path = Path(file_path).resolve()
            project_root = Path(__file__).parent.parent.parent.resolve()
            path.relative_to(project_root)  # Raises ValueError if outside project
        except ValueError:
            print(f"Error: File path outside project root: {file_path}")
            return ""

        try:
            with open(file_path, 'r', encoding='utf-8', errors='replace') as f:
                content = f.read()
                # Cache the result for future access
                self._file_cache[file_path] = content
                return content
        except (IOError, OSError) as e:
            print(f"Error reading {file_path}: {e}")
            return ""

    def extract_case_implementation(self, source: str, case_labels: List[str]) -> tuple:
        """
        Extract code for a specific case in a switch statement.

        Args:
            source: Source code containing switch statement
            case_labels: Possible case labels to find (try each in order)

        Returns:
            Tuple of (case_source, line_number) or ("", 0) if not found
        """
        # Try each possible case label
        for case_label in case_labels:
            # Find the case statement
            pattern = rf'case\s+{re.escape(case_label)}\s*:'
            match = re.search(pattern, source)

            if match:
                # Calculate line number
                line_num = source[:match.start()].count('\n') + 1

                # Extract from case to next break/case/default/}
                start = match.end()

                # Find the end (next case, default, or closing brace)
                # Pattern matches: "case label:" at start of line OR "default:" OR "}" at start of line
                end_pattern = r'(^\s*case\s+\w+\s*:|^\s*default\s*:|^\s*\})'
                remaining = source[start:]
                end_match = re.search(end_pattern, remaining, re.MULTILINE)

                if end_match:
                    end = start + end_match.start()
                else:
                    # Take until end of file (shouldn't happen)
                    end = len(source)

                return (source[start:end], line_num)

        return ("", 0)

    def analyze_verb_implementation(self, source: str, processor_name: str,
                                   verb_name: str, token: int,
                                   impl_file: str, line_num: int = 0) -> VerbImplementation:
        """
        Analyze a single verb implementation.

        Args:
            source: Source code containing verb implementation
            processor_name: Processor name
            verb_name: Verb name
            token: Token index
            impl_file: Source file path

        Returns:
            VerbImplementation record
        """
        # Parse annotations first (they override heuristics)
        annotations = parse_annotations(source)

        # Detect implementation vs. stub
        # @IMPLEMENTED, @PLATFORM_SPECIFIC, and @SCRIPT_IMPLEMENTED annotations override stub detection
        # Platform-specific stubs are correct implementations for the target platform
        # Script-implemented verbs are implemented in UserTalk rather than C
        if (annotations.get('implemented', False) or
            annotations.get('platform_specific', False) or
            annotations.get('script_implemented', False)):
            is_stub = False
            is_implemented = True
        else:
            is_stub = detect_stub_verb(source)
            is_implemented = not is_stub

        # Detect Carbon dependencies (only if implemented)
        has_carbon = False
        if is_implemented and not annotations.get('carbon_deps', False):
            has_carbon = detect_carbon_apis(source)
        elif annotations.get('carbon_deps', False):
            has_carbon = True

        # Detect UI adapters (only if implemented)
        uses_ui_adapter = False
        if is_implemented and not annotations.get('ui_adapter', False):
            uses_ui_adapter = detect_ui_adapters(source)
        elif annotations.get('ui_adapter', False):
            uses_ui_adapter = True

        # Platform-specific flag
        platform_specific = annotations.get('platform_specific', False)

        # Estimate complexity
        complexity = estimate_complexity(source) if is_implemented else 1

        return VerbImplementation(
            processor=processor_name,
            verb_name=verb_name,
            token=token,
            is_implemented=is_implemented,
            impl_file=impl_file,
            impl_line=line_num,
            has_carbon_deps=has_carbon,
            uses_ui_adapter=uses_ui_adapter,
            platform_specific=platform_specific,
            complexity=complexity
        )

    def extract_verb_names_from_enum(self, source: str, processor_name: str) -> List[str]:
        """
        Extract verb names from the enum definition in the source file.

        Handles multiple enum formats:
            enum { filv_created = 0, ... };
            typedef enum tyfiletoken { filecreatedfunc, ... } tyfiletoken;

        Args:
            source: Source code
            processor_name: Processor name (e.g., "file")

        Returns:
            List of verb names (without prefix)
        """
        # Try multiple enum patterns
        enum_patterns = [
            # Headless style: enum { filv_created = 0, ... }
            r'enum\s*\{([^}]+)\}',
            # Legacy typedef style: typedef enum tyXtoken { ... } tyXtoken
            r'typedef\s+enum\s+\w*\s*\{([^}]+)\}',
        ]

        enum_body = None
        for pattern in enum_patterns:
            match = re.search(pattern, source, re.DOTALL)
            if match:
                enum_body = match.group(1)
                break

        if not enum_body:
            return []

        # Remove C comments from enum body to avoid false matches
        # Remove /* ... */ style comments
        enum_body = re.sub(r'/\*.*?\*/', '', enum_body, flags=re.DOTALL)
        # Remove // style comments
        enum_body = re.sub(r'//.*?$', '', enum_body, flags=re.MULTILINE)

        # Strategy 1: Try processor-prefixed patterns (headless style)
        # e.g., filv_created, file_created, filecreatedfunc
        prefixes = [
            processor_name[0:3] + "v_",  # e.g., "filv_"
            processor_name,               # e.g., "file"
            processor_name + "_",         # e.g., "file_"
        ]

        suffixes = ["", "func", "_func"]

        # Try each prefix/suffix combination
        for prefix in prefixes:
            for suffix in suffixes:
                if suffix:
                    # Match: prefix + captured verb name + suffix + (comma or equals)
                    # Example: "filecreatedfunc," → captures "created"
                    pattern = rf'{re.escape(prefix)}(\w+){re.escape(suffix)}\s*[,=]'
                else:
                    # Match: prefix + captured verb name + (comma or equals)
                    # Example: "file_created," → captures "created"
                    pattern = rf'{re.escape(prefix)}(\w+)\s*[,=]'

                matches = re.findall(pattern, enum_body)

                if matches:
                    # Clean up verb names (remove trailing "func" if present in name itself)
                    cleaned = []
                    for match in matches:
                        # Remove common suffixes from verb names
                        verb_name = match
                        if verb_name.endswith('func'):
                            verb_name = verb_name[:-4]
                        # Skip empty or very short names (likely false matches)
                        if len(verb_name) >= MIN_VERB_NAME_LENGTH:
                            cleaned.append(verb_name)

                    # Only return if we got reasonable matches
                    # Require at least MIN_EXTRACTED_VERBS to avoid false positives from partial patterns
                    if cleaned and len(cleaned) >= MIN_EXTRACTED_VERBS:
                        return cleaned

        # Strategy 2: Legacy pattern with NO processor prefix
        # Just extract all identifiers ending with "func"
        # e.g., linetextfunc, levelfunc, movefunc
        pattern = r'(\w+func)\s*[,=]'
        matches = re.findall(pattern, enum_body)

        if matches:
            # Remove the "func" suffix to get verb names
            # linetextfunc -> linetext
            return [m[:-4] for m in matches if m.endswith('func')]

        return []

    def analyze_processor(self, processor) -> List[VerbImplementation]:
        """
        Analyze all verbs in a processor.

        Args:
            processor: EFPProcessor object containing name, verb_count, and verb_names

        Returns:
            List of VerbImplementation records
        """
        processor_name = processor.name
        verb_count = processor.verb_count

        # Find implementation file
        impl_file = self.find_implementation_file(processor_name)

        if not impl_file:
            print(f"Warning: No implementation file found for processor '{processor_name}'")
            # Return stub records for all verbs
            return [
                VerbImplementation(
                    processor=processor_name,
                    verb_name=f"verb{i}",
                    token=i,
                    is_implemented=False,
                    impl_file="",
                    impl_line=0,
                    has_carbon_deps=False,
                    uses_ui_adapter=False,
                    platform_specific=False,
                    complexity=1
                )
                for i in range(verb_count)
            ]

        # Read source file
        source = self.read_source_file(impl_file)

        if not source:
            print(f"Warning: Could not read source file: {impl_file}")
            return []

        # Use RC verb names as source of truth (preferred)
        # Fall back to enum extraction only if RC parsing failed
        if processor.verb_names and len(processor.verb_names) == verb_count:
            verb_names = processor.verb_names
        else:
            # Fallback: Extract verb names from enum
            verb_names = self.extract_verb_names_from_enum(source, processor_name)

            if not verb_names:
                print(f"Warning: Could not extract verb names for {processor_name} (expected enum not found)")
                # Fall back to generic names
                verb_names = [f"verb{i}" for i in range(verb_count)]

            if len(verb_names) != verb_count:
                print(f"Warning: Verb count mismatch for {processor_name}: enum has {len(verb_names)}, RC has {verb_count}")
                # Pad or truncate to match
                if len(verb_names) < verb_count:
                    verb_names.extend([f"verb{i}" for i in range(len(verb_names), verb_count)])
                else:
                    verb_names = verb_names[:verb_count]

        # Detect dispatcher pattern (headless verbs that forward to real implementation)
        # Pattern 1: headless_<processor>_verbs_callback function that forwards all verbs
        # Pattern 2: <processor>_valueproc function with full switch (modular callback)
        dispatcher_patterns = [
            rf'headless_{processor_name}_verbs_callback',
            rf'{processor_name}_valueproc\s*\(',  # e.g., date_valueproc(, clock_valueproc(
        ]

        is_dispatcher = False
        for pattern in dispatcher_patterns:
            if re.search(pattern, source):
                is_dispatcher = True
                break

        # If dispatcher pattern detected, check if file is overall implemented (not a stub)
        if is_dispatcher:
            from matchers import detect_stub_verb
            file_is_stub = detect_stub_verb(source)
            if not file_is_stub:
                # Dispatcher with real implementation - all verbs are implemented
                # Return all verbs as implemented without checking individual cases
                print(f"  Detected dispatcher pattern for {processor_name} (all verbs forwarded to implementation)")
                return [
                    VerbImplementation(
                        processor=processor_name,
                        verb_name=verb_names[i],
                        token=i,
                        is_implemented=True,
                        impl_file=impl_file,
                        impl_line=0,
                        has_carbon_deps=False,
                        uses_ui_adapter=False,
                        platform_specific=False,
                        complexity=1
                    )
                    for i in range(len(verb_names))
                ]

        implementations = []

        # Analyze each verb
        for i, verb_name in enumerate(verb_names):
            # Generate possible case labels using exception tables and pattern detection
            possible_labels = []

            # Pattern C: Check exception table first
            exception = get_pattern_c_exception(processor_name, verb_name)
            if exception:
                possible_labels.append(exception)

            # Pattern D: Multi-processor consolidation (langverbs.c)
            # BUT: For headless mode, use standard patterns since we prefer headless stub files
            if is_pattern_d_processor(processor_name) and self.build_target != 'headless':
                possible_labels.extend(generate_pattern_d_candidates(processor_name, verb_name))
            else:
                # Standard patterns (Pattern A, B)
                # 1. Headless style: filv_created
                # 2. Legacy with prefix: filecreatedfunc
                # 3. Legacy without prefix: linetextfunc (most common!)
                # 4. Alternative: file_created
                possible_labels.extend([
                    f"{processor_name[0:3]}v_{verb_name}",  # filv_created (headless)
                    f"{processor_name}{verb_name}func",      # filecreatedfunc (legacy with prefix)
                    f"{verb_name}func",                      # linetextfunc (legacy NO prefix - MOST COMMON!)
                    f"{processor_name}_{verb_name}",         # file_created
                    f"{processor_name}{verb_name}",          # filecreated
                ])


            # Extract case implementation (tries all possible labels)
            case_source, line_num = self.extract_case_implementation(source, possible_labels)

            if case_source:
                impl = self.analyze_verb_implementation(
                    case_source, processor_name, verb_name, i, impl_file, line_num
                )
            else:
                # No C case found - check if script-implemented
                is_script_impl = is_script_implemented(processor_name, verb_name)

                if is_script_impl:
                    # Verb is implemented in UserTalk scripts
                    impl = VerbImplementation(
                        processor=processor_name,
                        verb_name=verb_name,
                        token=i,
                        is_implemented=True,  # Script-implemented counts as implemented
                        impl_file="<UserTalk script>",
                        impl_line=0,
                        has_carbon_deps=False,
                        uses_ui_adapter=False,
                        platform_specific=False,
                        complexity=1
                    )
                else:
                    # Not found in C or scripts - assume stub
                    impl = VerbImplementation(
                        processor=processor_name,
                        verb_name=verb_name,
                        token=i,
                        is_implemented=False,
                        impl_file=impl_file,
                        impl_line=0,
                        has_carbon_deps=False,
                        uses_ui_adapter=False,
                        platform_specific=False,
                        complexity=1
                    )

            implementations.append(impl)

        return implementations

    def analyze_all_processors(self) -> List[VerbImplementation]:
        """
        Analyze all processors and generate implementation records.

        Returns:
            List of all VerbImplementation records
        """
        all_implementations = []

        for processor in self.processors:
            print(f"Analyzing processor: {processor.name} ({processor.verb_count} verbs)")

            impls = self.analyze_processor(processor)
            all_implementations.extend(impls)

        return all_implementations
