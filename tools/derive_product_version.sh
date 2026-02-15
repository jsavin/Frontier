#!/bin/sh
# Derive product version from CLI version tag.
# CLI major + 10, stage mapped: alpha→a, beta→b, rc→fc, dev→d
# Examples:
#   v1.0.0-alpha.6       → 11.0a6
#   v1.0.0-beta.3        → 11.0b3
#   v1.0.0               → 11.0
#   v1.1.0-alpha.2       → 11.1a2
#   v1.2.3               → 11.2.3
#   1.0.0-dev             → 11.0d
#   db0949d (bare hash)   → 11.0d (safe fallback)

v="${1#v}"  # strip leading 'v' if present

# Validate input starts with digits (a proper version tag).
# git describe --always can return bare commit hashes when no tags exist.
if [ -z "$v" ] || ! echo "$v" | grep -qE '^[0-9]+\.'; then
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

# Only omit patch when it's genuinely zero (e.g., v1.0.0 → 11.0, not v1.2.2 → 11.2)
if [ "$patch" = "0" ]; then
    echo "${pmajor}.${minor}${stage}${sub}"
else
    echo "${pmajor}.${minor}.${patch}${stage}${sub}"
fi
