#!/bin/bash
# Derive product version from CLI version tag.
# Maps CLI version to Frontier product version: CLI major + 10.
# Stage names mapped: alpha→a, beta→b, rc→fc, dev→d.
#
# Uses sed -E (extended regex) which requires bash; not POSIX sh.
#
# Examples:
#   v1.0.0-alpha.6       → 11.0a6
#   v1.0.0-beta.3        → 11.0b3
#   v1.0.0               → 11.0
#   v1.1.0-alpha.2       → 11.1a2
#   v1.2.3               → 11.2.3
#   v1.2.2               → 11.2.2
#   1.0.0-dev             → 11.0d
#
# Edge cases (all produce safe fallback "11.0d"):
#   db0949d              - bare commit hash from git describe --always
#   ""                   - empty input
#   abc                  - non-numeric garbage
#
# Note: v0.x tags would produce product major 10, which collides with
# legacy Frontier 10.x. CLI tags should always start at v1.x or higher.

v="${1#v}"  # strip leading 'v' if present

# Validate input starts with digits followed by dot (a proper semver tag).
# git describe --always can return bare commit hashes when no tags exist.
if [[ -z "$v" || ! "$v" =~ ^[0-9]+\. ]]; then
    echo "11.0d"
    exit 0
fi

major=$(echo "$v" | sed -E 's/^([0-9]+).*/\1/')
rest=$(echo "$v" | sed -E 's/^[0-9]+\.//')
pmajor=$((major + 10))
minor=$(echo "$rest" | sed -E 's/^([0-9]+).*/\1/')
rest2=$(echo "$rest" | sed -E 's/^[0-9]+\.//')
patch=$(echo "$rest2" | sed -E 's/^([0-9]+).*/\1/')

stage=""
sub=""
if echo "$v" | grep -qE '\-(alpha|beta|rc|dev)'; then
    stage=$(echo "$v" | sed -E 's/.*-(alpha|beta|rc|dev).*/\1/')
    sub=$(echo "$v" | sed -E 's/.*-(alpha|beta|rc|dev)\.?//; s/[^0-9].*//')
    case "$stage" in
        alpha) stage="a" ;;
        beta)  stage="b" ;;
        rc)    stage="fc" ;;
        dev)   stage="d" ;;
    esac
fi

# Only omit patch when it's genuinely zero or absent (e.g., v1.0.0 → 11.0, not v1.2.2 → 11.2)
if [ -z "$patch" ] || [ "$patch" = "0" ]; then
    echo "${pmajor}.${minor}${stage}${sub}"
else
    echo "${pmajor}.${minor}.${patch}${stage}${sub}"
fi
